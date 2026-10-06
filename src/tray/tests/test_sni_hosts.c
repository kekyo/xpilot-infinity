#include "tray_sni.h"
#include <assert.h>

#define WATCHER "org.kde.StatusNotifierWatcher"
#define PATH "/StatusNotifierWatcher"
static bool usable;
static unsigned changes;
static bool host;
static GDBusMethodInvocation *pending;
static void activate(void *context, int id) { (void)context; (void)id; }
static void available(void *context, bool value) { (void)context; usable = value; changes++; }
static gboolean deadline(void *context) { (void)context; g_error("SNI lifecycle event did not arrive"); return G_SOURCE_REMOVE; }
static GVariant *property(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *name, GError **error, void *context)
{
    (void)bus; (void)sender; (void)path; (void)interface; (void)name; (void)error; (void)context;
    return g_variant_new_boolean(host);
}
static void method(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *name, GVariant *args, GDBusMethodInvocation *invocation, void *context)
{
    (void)bus; (void)sender; (void)path; (void)interface; (void)name; (void)args; (void)context;
    assert(!pending);
    pending = g_object_ref(invocation);
}
static void name(GDBusConnection *bus, bool acquire)
{
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_sync(bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", acquire ? "RequestName" : "ReleaseName",
        acquire ? g_variant_new("(su)", WATCHER, 0) : g_variant_new("(s)", WATCHER),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, 10000, NULL, &error);
    assert(reply && !error); g_variant_unref(reply);
}
static void host_signal(GDBusConnection *bus, bool value)
{
    host = value;
    g_dbus_connection_emit_signal(bus, NULL, PATH, WATCHER,
        value ? "StatusNotifierHostRegistered" : "StatusNotifierHostUnregistered", NULL, NULL);
}
static void accept(void)
{
    assert(pending);
    g_dbus_method_invocation_return_value(pending, NULL);
    g_clear_object(&pending);
}

int main(void)
{
    guint timeout = g_timeout_add_seconds(30, deadline, NULL);
    GTestDBus *fixture = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(fixture);
    GError *error = NULL;
    GDBusConnection *bus = g_dbus_connection_new_for_address_sync(g_test_dbus_get_bus_address(fixture),
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION, NULL, NULL, &error);
    assert(bus && !error);
    tray_menu menu = {0}; tray_status status = {0}; tray_menu_update(&menu, &status, "ndh.xp2");
    tray_sni *sni = tray_sni_create(bus, &menu, XPILOT_TEST_ICON, activate, available, NULL, &error);
    assert(sni && !error);
    while (!changes) g_main_context_iteration(NULL, TRUE);
    assert(!usable);
    const char *xml = "<node><interface name='" WATCHER "'>"
        "<method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
        "<property name='IsStatusNotifierHostRegistered' type='b' access='read'/>"
        "<signal name='StatusNotifierHostRegistered'/><signal name='StatusNotifierHostUnregistered'/>"
        "</interface></node>";
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, &error);
    const GDBusInterfaceVTable vtable = {method, property, NULL, {0}};
    guint id = g_dbus_connection_register_object(bus, PATH, info->interfaces[0], &vtable, NULL, NULL, &error);
    assert(id && !error);
    name(bus, true);
    host_signal(bus, true);
    while (!pending) g_main_context_iteration(NULL, TRUE);
    assert(!usable); /* A dock request is not a usable icon. */
    accept();
    while (!usable) g_main_context_iteration(NULL, TRUE);
    host_signal(bus, false);
    while (usable) g_main_context_iteration(NULL, TRUE);
    host_signal(bus, true);
    while (!pending) g_main_context_iteration(NULL, TRUE);
    GDBusMethodInvocation *old = pending; pending = NULL;
    name(bus, false);
    name(bus, true);
    while (!pending) g_main_context_iteration(NULL, TRUE);
    /* A replaced watcher's delayed reply must not make an unregistered host usable. */
    g_dbus_method_invocation_return_value(old, NULL); g_object_unref(old);
    assert(!usable);
    accept();
    while (!usable) g_main_context_iteration(NULL, TRUE);
    name(bus, false);
    while (usable) g_main_context_iteration(NULL, TRUE);
    name(bus, true);
    while (!pending) g_main_context_iteration(NULL, TRUE);
    tray_sni_destroy(sni);
    unsigned final_changes = changes;
    accept();
    while (g_main_context_iteration(NULL, FALSE)) {}
    assert(changes == final_changes);
    g_dbus_connection_unregister_object(bus, id);
    g_dbus_node_info_unref(info); tray_menu_clear(&menu);
    g_dbus_connection_close_sync(bus, NULL, NULL); g_object_unref(bus);
    g_test_dbus_down(fixture); g_object_unref(fixture);
    g_source_remove(timeout);
    return 0;
}
