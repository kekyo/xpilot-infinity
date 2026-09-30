#include "tray_sni.h"
#include <assert.h>

static unsigned activations;
static bool host_available;
static void activate(void *context, int id)
{
    (void)context;
    assert(id == TRAY_START);
    activations++;
}
static void available(void *context, bool usable)
{
    (void)context;
    host_available = usable;
}

static GVariant *reply;
static GError *reply_error;
static bool replied;
static void called(GObject *object, GAsyncResult *result, void *context)
{
    (void)context;
    reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &reply_error);
    replied = true;
}
static GVariant *call(GDBusConnection *bus, const char *method, GVariant *parameters)
{
    replied = false;
    g_dbus_connection_call(bus, g_dbus_connection_get_unique_name(bus), "/Menu",
        "com.canonical.dbusmenu", method, parameters, NULL,
        G_DBUS_CALL_FLAGS_NONE, 10000, NULL, called, NULL);
    while (!replied)
        g_main_context_iteration(NULL, TRUE);
    assert(reply_error == NULL);
    assert(reply != NULL);
    return reply;
}

int main(void)
{
    GTestDBus *test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);
    GError *error = NULL;
    GDBusConnection *bus = g_dbus_connection_new_for_address_sync(
        g_test_dbus_get_bus_address(test_bus),
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
        NULL, NULL, &error);
    assert(bus != NULL && error == NULL);
    tray_status status = {0};
    status.service.state = XP_SERVICE_STOPPED;
    status.can_start = true;
    tray_menu menu = {0};
    tray_menu_update(&menu, &status, "ndh.xp2");
    tray_sni *sni = tray_sni_create(bus, &menu, XPILOT_TEST_ICON,
        activate, available, NULL, &error);
    assert(sni != NULL && error == NULL);
    GVariant *layout_reply = call(bus, "GetLayout", g_variant_new("(ii@as)",
        0, -1, g_variant_new_strv(NULL, 0)));
    guint revision;
    GVariant *layout;
    g_variant_get(layout_reply, "(u@(ia{sv}av))", &revision, &layout);
    GVariant *children = g_variant_get_child_value(layout, 2);
    assert(g_variant_n_children(children) == 7);
    g_variant_unref(children);
    g_variant_unref(layout);
    g_variant_unref(layout_reply);
    GVariant *event_reply = call(bus, "Event", g_variant_new("(isvu)",
        TRAY_START, "clicked", g_variant_new_int32(0), 0));
    g_variant_unref(event_reply);
    assert(activations == 1);
    status.can_start = false;
    tray_menu_update(&menu, &status, "ndh.xp2");
    tray_sni_update(sni);
    event_reply = call(bus, "Event", g_variant_new("(isvu)",
        TRAY_START, "clicked", g_variant_new_int32(0), 0));
    g_variant_unref(event_reply);
    assert(activations == 1);
    assert(!host_available);
    tray_sni_destroy(sni);
    /* Drain pending watcher callbacks after destruction. */
    while (g_main_context_iteration(NULL, FALSE)) {}
    g_dbus_connection_close_sync(bus, NULL, NULL);
    g_object_unref(bus);
    g_test_dbus_down(test_bus);
    g_object_unref(test_bus);
    return 0;
}
