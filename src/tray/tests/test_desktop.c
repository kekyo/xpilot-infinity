#include "tray_menu.h"
#include <gio/gio.h>
#include <glib-unix.h>
#define ATSPI_DISABLE_DEPRECATED
#include <atspi/atspi.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <assert.h>
#include <signal.h>
#include <string.h>

typedef struct {
    GDBusConnection *bus;
    char *item_owner;
    bool start;
    bool denied;
    bool map;
    bool map_running;
    bool exited;
    bool editor;
    int exit_signal;
    bool real_editor;
    bool document_saved;
    AtspiAccessible *editor_window;
    char *before;
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
    bool can_start = false, can_stop = false, can_edit = false, can_apply = false;
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
        if (id == TRAY_EDIT) can_edit = enabled;
        if (id == TRAY_APPLY) can_apply = enabled;
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
    if (test->exit_signal && test->stage == 0 && can_stop) {
        test->stage = 2;
        g_subprocess_send_signal(test->application, test->exit_signal);
    } else if (test->editor && test->stage == 0 && can_edit) {
        assert(!can_apply);
        test->stage = 1;
        send_event(test, TRAY_EDIT);
    } else if (test->editor && test->stage == 1 && can_apply) {
        char *actual = NULL;
        assert(g_file_get_contents("/etc/default/xpilot-infinity-server", &actual, NULL, &error));
        assert(!strcmp(actual, test->before));
        g_free(actual);
        test->stage = 2;
        send_event(test, TRAY_APPLY);
    } else if (test->editor && test->stage == 3 && !can_apply && can_edit && (can_start || can_stop)) {
        char *actual = NULL;
        assert(g_file_get_contents("/etc/default/xpilot-infinity-server", &actual, NULL, &error));
        const char *port = g_getenv("XPILOT_EDITOR_TEST_PORT");
        if (port && strstr(actual, port)) {
            test->stage = 4;
            send_event(test, TRAY_QUIT);
        }
        g_free(actual);
    } else if (!test->editor && test->map && test->stage == 0 && map_id && map_enabled) {
        assert(!map_checked);
        test->stage = 1;
        send_event(test, map_id);
    } else if (test->map && test->stage == 1 && map_checked && map_enabled
               && (test->map_running ? can_stop : can_start)) {
        test->stage = 2;
        send_event(test, TRAY_QUIT);
    } else if (!test->editor && !test->map && test->stage == 0 && (test->start ? can_start : can_stop)) {
        assert(test->start ? !can_stop : !can_start);
        test->stage = 1;
        send_event(test, test->start ? TRAY_START : TRAY_STOP);
    } else if (!test->editor && !test->map && test->stage == 1) {
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
    if (test->query_again && test->stage < (test->editor ? 4u : 2u)) {
        test->query_again = false;
        query_layout(test);
    }
}

static void query_layout(desktop_test *test)
{
    if (test->stage >= (test->editor ? 4u : 2u) || !test->item_owner) return;
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
    if (test->exit_signal) success = !success && g_subprocess_get_if_signaled(G_SUBPROCESS(object));
    if (error) g_printerr("Desktop exited: %s\n", error->message);
    assert(success && test->stage == (test->editor ? 4u : 2u));
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
        if (event.type != MapNotify || test->stage == (test->editor ? 4u : 2u)) continue;
        char *title = NULL;
        XFetchName(test->display, event.xmap.window, &title);
        if (test->editor) {
            bool apply = title && !strcmp(title, "Apply saved configuration");
            if (title) XFree(title);
            if (!apply || test->stage != 2) continue;
            XSetInputFocus(test->display, event.xmap.window, RevertToParent, CurrentTime);
            assert(XTestFakeKeyEvent(test->display, XKeysymToKeycode(test->display, XK_Alt_L), True, CurrentTime));
            assert(XTestFakeKeyEvent(test->display, XKeysymToKeycode(test->display, XK_a), True, CurrentTime));
            assert(XTestFakeKeyEvent(test->display, XKeysymToKeycode(test->display, XK_a), False, CurrentTime));
            assert(XTestFakeKeyEvent(test->display, XKeysymToKeycode(test->display, XK_Alt_L), False, CurrentTime));
            XFlush(test->display);
            test->stage = 3;
            continue;
        }
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

static bool activate_button(AtspiAccessible *node, const char *label)
{
    GError *error = NULL;
    char *name = atspi_accessible_get_name(node, &error);
    assert(!error);
    bool matches = name && !strcmp(name, label);
    g_free(name);
    if (matches) {
        AtspiAction *action = atspi_accessible_get_action_iface(node);
        if (action) {
            bool activated = atspi_action_do_action(action, 0, &error);
            assert(activated && !error);
            g_object_unref(action);
            return true;
        }
    }
    int count = atspi_accessible_get_child_count(node, &error);
    assert(!error);
    for (int i = 0; i < count; i++) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        assert(child && !error);
        bool activated = activate_button(child, label);
        g_object_unref(child);
        if (activated) return true;
    }
    return false;
}

static bool edit_document(AtspiAccessible *node, desktop_test *test)
{
    GError *error = NULL;
    AtspiEditableText *editable = atspi_accessible_get_editable_text_iface(node);
    AtspiText *text = atspi_accessible_get_text_iface(node);
    bool edited = false;
    if (editable && text) {
        char *contents = atspi_text_get_text(text, 0, (int)g_utf8_strlen(test->before, -1), &error);
        assert(!error);
        size_t length = contents ? strlen(contents) : 0;
        size_t baseline_length = strlen(test->before);
        /* Gedit keeps its implicit final newline outside the visible buffer. */
        bool matches = contents && (!strcmp(contents, test->before)
            || (baseline_length == length + 1 && test->before[length] == '\n'
                && !memcmp(contents, test->before, length)));
        if (matches) {
            char *replacement = g_strdup_printf("XPILOT_SERVER_OPTIONS='-noQuit +reportMeta -port %s -map ndh.xp2'",
                g_getenv("XPILOT_EDITOR_TEST_PORT"));
            test->document_saved = true;
            assert(atspi_editable_text_set_text_contents(editable, replacement, &error));
            assert(!error);
            g_free(replacement);
            edited = true;
        }
        g_free(contents);
    }
    g_clear_object(&editable);
    g_clear_object(&text);
    if (edited) return true;
    int count = atspi_accessible_get_child_count(node, &error);
    assert(!error);
    for (int i = 0; i < count; i++) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        assert(child && !error);
        edited = edit_document(child, test);
        g_object_unref(child);
        if (edited) return true;
    }
    return false;
}

static bool inspect_accessible_window(AtspiAccessible *node, desktop_test *test)
{
    /* GtkMessageDialog exposes its message type as the accessible name, which
       may differ from its window title. The confirmation action is unambiguous. */
    if (test->editor && test->stage == 2) {
        test->stage = 3;
        bool activated = activate_button(node, "Apply saved changes");
        if (!activated) test->stage = 2;
        return activated;
    }
    GError *error = NULL;
    char *name = atspi_accessible_get_name(node, &error);
    assert(!error);
    bool activated = false;
    if (test->real_editor && test->stage == 1 && name && strstr(name, "xpilot-infinity-server.txt")
        && atspi_accessible_get_role(node, &error) == ATSPI_ROLE_FRAME && !test->editor_window) {
        assert(!error);
        test->editor_window = g_object_ref(node);
        g_print("Default editor window: %s\n", name);
    }
    if (!test->editor && test->stage == 0 && name && !strcmp(name, "XPilot Infinity Server")) {
        test->stage = 2;
        activated = activate_button(node, "Quit tray (server keeps running)");
        if (!activated) test->stage = 0;
    }
    g_free(name);
    if (activated) return true;
    int count = atspi_accessible_get_child_count(node, &error);
    assert(!error);
    for (int i = 0; i < count; i++) {
        AtspiAccessible *child = atspi_accessible_get_child_at_index(node, i, &error);
        assert(child && !error);
        activated = inspect_accessible_window(child, test);
        g_object_unref(child);
        if (activated) return true;
    }
    return false;
}

static void accessible_window(AtspiEvent *event, void *context)
{
    desktop_test *test = context;
    /* GTK can publish an already mapped window when first joining AT-SPI.
       Registry child registration is also a readiness event in that case. */
    if (test->stage == (test->editor ? 2u : 0u) || (test->real_editor && test->stage == 1))
        inspect_accessible_window(event->source, test);
    if (test->real_editor && test->stage == 1 && test->editor_window && !test->document_saved
        && edit_document(test->editor_window, test)) {
        assert(activate_button(test->editor_window, "Save"));
        g_print("The default GUI editor saved the editing copy.\n");
    }
    g_boxed_free(ATSPI_TYPE_EVENT, event);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    assert(g_file_test("/run/systemd/container", G_FILE_TEST_EXISTS));
    desktop_test test = {0};
    bool fallback = !strcmp(argv[2], "fallback");
    test.editor = !strcmp(argv[2], "editor");
    test.real_editor = test.editor && g_getenv("XPILOT_REAL_EDITOR");
    test.exit_signal = !strcmp(argv[2], "crash") ? SIGKILL : !strcmp(argv[2], "terminate") ? SIGTERM : 0;
    test.start = strcmp(argv[2], "stop") != 0;
    test.denied = !strcmp(argv[2], "denied");
    test.map_running = !strcmp(argv[2], "map-running");
    test.map = test.map_running || !strcmp(argv[2], "map");
    char *display = g_strdup(g_getenv("DISPLAY"));
    char *runtime_directory = g_strdup(g_getenv("XDG_RUNTIME_DIR"));
    GTestDBus *test_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
    g_test_dbus_add_service_dir(test_bus, "/usr/share/dbus-1/services");
    g_test_dbus_up(test_bus);
    if (display) { g_setenv("DISPLAY", display, TRUE); g_free(display); }
    /* GTestDBus clears this to prevent using the real session bus. Keep our
       isolated compositor's socket directory while retaining its test bus. */
    if (runtime_directory) {
        g_setenv("XDG_RUNTIME_DIR", runtime_directory, TRUE);
        g_free(runtime_directory);
    }
    GError *error = NULL;
    if (test.editor) assert(g_file_get_contents("/etc/default/xpilot-infinity-server", &test.before, NULL, &error));
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
    AtspiEventListener *accessible_watch = NULL;
    bool wayland = !g_strcmp0(g_getenv("GDK_BACKEND"), "wayland");
    if ((fallback || test.editor) && (wayland || test.real_editor)) {
        assert(atspi_init() == 0);
        accessible_watch = atspi_event_listener_new(accessible_window, &test, NULL);
        assert(atspi_event_listener_register(accessible_watch, "window:create", &error));
        assert(atspi_event_listener_register(accessible_watch, "object:children-changed:add", &error));
        if (test.real_editor) {
            assert(atspi_event_listener_register(accessible_watch, "object:text-changed", &error));
            assert(atspi_event_listener_register(accessible_watch, "object:property-change:accessible-name", &error));
        }
        assert(!error);
    } else if (fallback || test.editor) {
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
    if (accessible_watch) {
        assert(atspi_event_listener_deregister(accessible_watch, "window:create", &error));
        assert(atspi_event_listener_deregister(accessible_watch, "object:children-changed:add", &error));
        if (test.real_editor) {
            assert(atspi_event_listener_deregister(accessible_watch, "object:text-changed", &error));
            assert(atspi_event_listener_deregister(accessible_watch, "object:property-change:accessible-name", &error));
        }
        assert(!error);
        g_object_unref(accessible_watch);
        g_clear_object(&test.editor_window);
        atspi_exit();
    }
    if (window_watch) {
        g_source_remove(window_watch);
        XCloseDisplay(test.display);
    }
    g_dbus_connection_signal_unsubscribe(test.bus, subscription);
    g_dbus_connection_unregister_object(test.bus, registration);
    g_dbus_node_info_unref(info);
    g_object_unref(test.application);
    g_free(test.item_owner);
    g_free(test.before);
    g_object_unref(test.bus);
    g_test_dbus_down(test_bus);
    g_object_unref(test_bus);
    return 0;
}
