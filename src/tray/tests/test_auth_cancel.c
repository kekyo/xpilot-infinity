#include "settings_protocol.h"
#include <polkit/polkit.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    GMainLoop *loop;
    bool challenged;
    bool canceled;
} fixture;

static void agent_call(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, void *context)
{
    (void)bus; (void)sender; (void)path; (void)interface; (void)parameters;
    fixture *test = context;
    if (!strcmp(method, "BeginAuthentication")) {
        test->challenged = true;
        g_dbus_method_invocation_return_error(invocation, POLKIT_ERROR, POLKIT_ERROR_CANCELLED,
                                              "Test user dismissed the authentication dialog");
    } else g_dbus_method_invocation_return_value(invocation, NULL);
}

static void completed(GObject *object, GAsyncResult *result, void *context)
{
    fixture *test = context;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    char *remote = error ? g_dbus_error_get_remote_error(error) : NULL;
    test->canceled = remote && !strcmp(remote, XP_SETTINGS_BUS ".Canceled");
    if (error) g_print("%s\n", error->message);
    g_free(remote); g_clear_error(&error); g_clear_pointer(&reply, g_variant_unref);
    g_main_loop_quit(test->loop);
}

int main(void)
{
    assert(getuid() != 0 && access("/run/systemd/container", F_OK) == 0);
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    assert(bus && !error);
    const char *xml = "<node><interface name='org.freedesktop.PolicyKit1.AuthenticationAgent'>"
        "<method name='BeginAuthentication'><arg type='s' direction='in'/><arg type='s' direction='in'/>"
        "<arg type='s' direction='in'/><arg type='a{ss}' direction='in'/><arg type='s' direction='in'/>"
        "<arg type='a(sa{sv})' direction='in'/></method>"
        "<method name='CancelAuthentication'><arg type='s' direction='in'/></method></interface></node>";
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, &error);
    assert(info && !error);
    fixture test = {g_main_loop_new(NULL, FALSE), false, false};
    const GDBusInterfaceVTable vtable = {agent_call, NULL, NULL, {0}};
    guint registration = g_dbus_connection_register_object(bus, "/test/Agent", info->interfaces[0],
        &vtable, &test, NULL, &error);
    assert(registration && !error);
    PolkitAuthority *authority = polkit_authority_get_sync(NULL, &error);
    assert(authority && !error);
    PolkitSubject *name = polkit_system_bus_name_new(g_dbus_connection_get_unique_name(bus));
    PolkitSubject *subject = polkit_system_bus_name_get_process_sync(POLKIT_SYSTEM_BUS_NAME(name), NULL, &error);
    g_object_unref(name);
    assert(subject && !error);
    if (!polkit_authority_register_authentication_agent_sync(authority, subject, "C", "/test/Agent", NULL, &error))
        g_error("Register test authentication agent: %s", error->message);
    char *before = NULL;
    assert(g_file_get_contents(XP_SETTINGS_FILE, &before, NULL, &error));
    char *generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, before, -1);
    const char *methods[] = {"SelectMap", "Apply"};
    for (unsigned i = 0; i < 2; i++) {
        test.challenged = test.canceled = false;
        g_dbus_connection_call(bus, XP_SETTINGS_BUS, XP_SETTINGS_PATH, XP_SETTINGS_BUS, methods[i],
            g_variant_new("(ss)", generation, i ? "XPILOT_SERVER_OPTIONS='-port 15347'" : "ndh.xp2"),
            G_VARIANT_TYPE("(bbss)"), G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
            120000, NULL, completed, &test);
        g_main_loop_run(test.loop);
        assert(test.challenged && test.canceled);
        char *after = NULL;
        assert(g_file_get_contents(XP_SETTINGS_FILE, &after, NULL, &error));
        assert(!strcmp(before, after));
        g_free(after);
    }
    assert(polkit_authority_unregister_authentication_agent_sync(authority, subject, "/test/Agent", NULL, &error));
    g_dbus_connection_unregister_object(bus, registration);
    g_free(before); g_free(generation);
    g_main_loop_unref(test.loop); g_object_unref(subject); g_object_unref(authority);
    g_dbus_node_info_unref(info); g_object_unref(bus);
    return 0;
}
