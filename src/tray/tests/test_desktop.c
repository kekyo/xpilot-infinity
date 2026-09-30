#include "tray_menu.h"
#include <gio/gio.h>
#include <glib-unix.h>
#include <X11/Xlib.h>
#include <assert.h>
#include <string.h>

typedef struct {
    GDBusConnection *bus;
    char *item_owner;
    bool start;
    bool denied;
    bool map;
    bool map_running;
    bool exited;
    bool query_pending;
    bool query_again;
    unsigned stage;
    GSubprocess *application;
    Display *display;
} desktop_test;

static void query_layout(desktop_test *test);

static void event_sent(GObject *object, GAsyncResult *result, void *context)
{
    (void)context;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    assert(error == NULL && reply != NULL);
    g_variant_unref(reply);
}

static void send_event(desktop_test *test, int id)
{
    g_dbus_connection_call(test->bus, test->item_owner, "/Menu",
        "com.canonical.dbusmenu", "Event",
        g_variant_new("(isvu)", id, "clicked", g_variant_new_int32(0), 0),
        G_VARIANT_TYPE_UNIT, G_DBUS_CALL_FLAGS_NONE, 10000, NULL, event_sent, NULL);
}

static void layout_ready(GObject *object, GAsyncResult *result, void *context)
{
    desktop_test *test = context;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    assert(error == NULL && reply != NULL);
    test->query_pending = false;
    guint revision;
    GVariant *layout;
    g_variant_get(reply, "(u@(ia{sv}av))", &revision, &layout);
    GVariant *children = g_variant_get_child_value(layout, 2);
    bool can_start = false, can_stop = false;
    int map_id = 0;
    bool map_enabled = false, map_checked = false;
    char *label = NULL;
    for (size_t i = 0; i < g_variant_n_children(children); i++) {
        GVariant *wrapped = g_variant_get_child_value(children, i);
        GVariant *row = g_variant_get_variant(wrapped);
        int id;
        GVariant *properties, *nested;
        g_variant_get(row, "(i@a{sv}@av)", &id, &properties, &nested);
        gboolean enabled = FALSE;
        g_variant_lookup(properties, "enabled", "b", &enabled);
        if (id == TRAY_START) can_start = enabled;
        if (id == TRAY_STOP) can_stop = enabled;
        if (id == TRAY_STATUS) g_variant_lookup(properties, "label", "s", &label);
        if (id == TRAY_MAP_MENU && test->map) {
            for (size_t j = 0; j < g_variant_n_children(nested); j++) {
                GVariant *wrapper = g_variant_get_child_value(nested, j);
                GVariant *map_row = g_variant_get_variant(wrapper);
                GVariant *map_properties = g_variant_get_child_value(map_row, 1);
                const char *name = NULL;
                g_variant_lookup(map_properties, "label", "&s", &name);
                if (name && !strcmp(name, test->map_running ? "ndh.xp2" : "blood-music.xp2")) {
                    g_variant_get_child(map_row, 0, "i", &map_id);
                    gboolean available = FALSE;
                    gint32 checked = 0;
                    g_variant_lookup(map_properties, "enabled", "b", &available);
                    g_variant_lookup(map_properties, "toggle-state", "i", &checked);
                    map_enabled = available;
                    map_checked = checked == 1;
                }
                g_variant_unref(map_properties); g_variant_unref(map_row); g_variant_unref(wrapper);
            }
        }
        g_variant_unref(nested); g_variant_unref(properties);
        g_variant_unref(row); g_variant_unref(wrapped);
    }
    assert(label != NULL);
    g_print("desktop menu: %s (start=%d stop=%d)\n", label, can_start, can_stop);
    if (test->map && test->stage == 0 && map_id && map_enabled) {
        assert(!map_checked);
        test->stage = 1;
        send_event(test, map_id);
    } else if (test->map && test->stage == 1 && map_checked && map_enabled
               && (test->map_running ? can_stop : can_start)) {
        test->stage = 2;
        send_event(test, TRAY_QUIT);
    } else if (!test->map && test->stage == 0 && (test->start ? can_start : can_stop)) {
        assert(test->start ? !can_stop : !can_start);
        test->stage = 1;
        send_event(test, test->start ? TRAY_START : TRAY_STOP);
    } else if (!test->map && test->stage == 1) {
        bool reached = test->denied ? strstr(label, "not authorized") != NULL
            : test->start ? can_stop && strstr(label, "Service running")
                          : can_start && strstr(label, "Service stopped");
        if (reached) {
            test->stage = 2;
            send_event(test, TRAY_QUIT);
        }
    }
    g_free(label);
    g_variant_unref(children); g_variant_unref(layout); g_variant_unref(reply);
    if (test->query_again && test->stage < 2) {
        test->query_again = false;
        query_layout(test);
    }
}

static void query_layout(desktop_test *test)
{
    if (test->stage >= 2 || !test->item_owner) return;
    if (test->query_pending) { test->query_again = true; return; }
    test->query_pending = true;
    g_dbus_connection_call(test->bus, test->item_owner, "/Menu", "com.canonical.dbusmenu",
        "GetLayout", g_variant_new("(ii@as)", 0, -1, g_variant_new_strv(NULL, 0)),
        G_VARIANT_TYPE("(u(ia{sv}av))"), G_DBUS_CALL_FLAGS_NONE, 10000, NULL, layout_ready, test);
}

static void menu_changed(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *signal, GVariant *parameters, void *context)
{
    desktop_test *test = context;
    (void)bus; (void)path; (void)interface; (void)signal; (void)parameters;
    if (test->item_owner && !strcmp(test->item_owner, sender)) query_layout(test);
}

static void register_item(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, void *context)
{
    desktop_test *test = context;
    (void)bus; (void)path; (void)interface; (void)method; (void)parameters;
    g_free(test->item_owner);
    test->item_owner = g_strdup(sender);
    g_dbus_method_invocation_return_value(invocation, NULL);
    query_layout(test);
}

static GVariant *host_property(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *property, GError **error, void *context)
{
    (void)bus; (void)sender; (void)path; (void)interface; (void)property; (void)error; (void)context;
    return g_variant_new_boolean(TRUE);
}

static void exited(GObject *object, GAsyncResult *result, void *context)
{
    desktop_test *test = context;
    GError *error = NULL;
    bool success = g_subprocess_wait_check_finish(G_SUBPROCESS(object), result, &error);
    if (error) g_printerr("Desktop exited: %s\n", error->message);
    assert(success && test->stage == 2);
    test->exited = true;
    g_clear_error(&error);
}

static gboolean expired(void *context)
{
    desktop_test *test = context;
    g_subprocess_force_exit(test->application);
    g_error("Timed out waiting for desktop menu state (stage %u)", test->stage);
    return G_SOURCE_REMOVE;
}

static gboolean window_event(int fd, GIOCondition condition, void *context)
{
    desktop_test *test = context;
    (void)fd;
    assert(!(condition & (G_IO_ERR | G_IO_HUP)));
    while (XPending(test->display)) {
        XEvent event;
        XNextEvent(test->display, &event);
        if (event.type != MapNotify || test->stage == 2) continue;
        char *title = NULL;
        XFetchName(test->display, event.xmap.window, &title);
        bool fallback = title && !strcmp(title, "XPilot Infinity Server");
        if (title) XFree(title);
        if (!fallback) continue;
        XEvent close = {0};
        close.xclient.type = ClientMessage;
        close.xclient.window = event.xmap.window;
        close.xclient.message_type = XInternAtom(test->display, "WM_PROTOCOLS", False);
        close.xclient.format = 32;
        close.xclient.data.l[0] = XInternAtom(test->display, "WM_DELETE_WINDOW", False);
        close.xclient.data.l[1] = CurrentTime;
        assert(XSendEvent(test->display, event.xmap.window, False, NoEventMask, &close));
        XFlush(test->display);
        test->stage = 2;
        g_print("Fallback window appeared without a tray host and accepted close.\n");
    }
    return G_SOURCE_CONTINUE;
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    assert(g_file_test("/run/systemd/container", G_FILE_TEST_EXISTS));
    desktop_test test = {0};
    bool fallback = !strcmp(argv[2], "fallback");
    test.start = strcmp(argv[2], "stop") != 0;
    test.denied = !strcmp(argv[2], "denied");
    test.map_running = !strcmp(argv[2], "map-running");
    test.map = test.map_running || !strcmp(argv[2], "map");
    char *display = g_strdup(g_getenv("DISPLAY"));
    GTestDBus *test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_up(test_bus);
    if (display) { g_setenv("DISPLAY", display, TRUE); g_free(display); }
    GError *error = NULL;
    test.bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    assert(error == NULL && test.bus != NULL);
    const char *xml = "<node><interface name='org.kde.StatusNotifierWatcher'>"
        "<method name='RegisterStatusNotifierItem'><arg type='s' direction='in'/></method>"
        "<property name='IsStatusNotifierHostRegistered' type='b' access='read'/></interface></node>";
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, &error);
    assert(info && !error);
    const GDBusInterfaceVTable vtable = {register_item, host_property, NULL, {0}};
    guint registration = g_dbus_connection_register_object(test.bus, "/StatusNotifierWatcher",
        info->interfaces[0], &vtable, &test, NULL, &error);
    assert(registration && !error);
    if (!fallback) {
    GVariant *owned = g_dbus_connection_call_sync(test.bus, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", "org.kde.StatusNotifierWatcher", 0),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    assert(owned && !error);
    g_variant_unref(owned);
    }
    guint subscription = g_dbus_connection_signal_subscribe(test.bus, NULL,
        "com.canonical.dbusmenu", "LayoutUpdated", "/Menu", NULL,
        G_DBUS_SIGNAL_FLAGS_NONE, menu_changed, &test, NULL);
    guint window_watch = 0;
    if (fallback) {
        test.display = XOpenDisplay(NULL);
        assert(test.display);
        XSelectInput(test.display, DefaultRootWindow(test.display), SubstructureNotifyMask);
        XSync(test.display, False);
        window_watch = g_unix_fd_add(ConnectionNumber(test.display),
            G_IO_IN | G_IO_ERR | G_IO_HUP, window_event, &test);
    }
    test.application = g_subprocess_new(G_SUBPROCESS_FLAGS_NONE, &error, argv[1], NULL);
    assert(test.application && !error);
    g_subprocess_wait_check_async(test.application, NULL, exited, &test);
    guint deadline = g_timeout_add_seconds(120, expired, &test);
    while (!test.exited) g_main_context_iteration(NULL, TRUE);
    g_source_remove(deadline);
    if (window_watch) {
        g_source_remove(window_watch);
        XCloseDisplay(test.display);
    }
    g_dbus_connection_signal_unsubscribe(test.bus, subscription);
    g_dbus_connection_unregister_object(test.bus, registration);
    g_dbus_node_info_unref(info);
    g_object_unref(test.application);
    g_free(test.item_owner);
    g_object_unref(test.bus);
    g_test_dbus_down(test_bus);
    g_object_unref(test_bus);
    return 0;
}
