#include "tray_xembed.h"
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <assert.h>

static bool usable;
static unsigned activations;
static void available(void *context, bool value) { (void)context; usable = value; }
static void activate(void *context, int id) { (void)context; if (id > 0) { assert(id == TRAY_START); activations++; } }
static gboolean deadline(void *context) { (void)context; g_error("XEmbed event did not arrive"); return G_SOURCE_REMOVE; }
static gboolean keep_socket(GtkSocket *socket, void *context) { (void)socket; (void)context; return TRUE; }
static gboolean painted(GtkWidget *widget, cairo_t *cr, void *context)
{
    (void)widget; (void)cr;
    *(bool *)context = true;
    return FALSE;
}
static void wait_for_paint(GtkWidget *widget)
{
    bool ready = false;
    gulong handler = g_signal_connect_after(widget, "draw", G_CALLBACK(painted), &ready);
    gtk_widget_queue_draw(widget);
    while (!ready) g_main_context_iteration(NULL, TRUE);
    g_signal_handler_disconnect(widget, handler);
    gdk_display_sync(gtk_widget_get_display(widget));
}
static void paint_plug(void)
{
    GList *windows = gtk_window_list_toplevels();
    for (GList *p = windows; p; p = p->next)
        if (GTK_IS_PLUG(p->data) && gtk_widget_get_mapped(p->data)) wait_for_paint(p->data);
    g_list_free(windows);
}

typedef struct { GtkWidget *socket; Display *display; Atom opcode; unsigned docks; Window plug; } host;
static GdkFilterReturn host_event(GdkXEvent *raw, GdkEvent *event, void *context)
{
    (void)event;
    host *test = context;
    XEvent *x = raw;
    if (x->type == MapNotify) {
        char *title = NULL;
        if (XFetchName(test->display, x->xmap.window, &title) && title) {
            if (!strcmp(title, "XPilot Infinity Server Tray")) {
                Window root, parent, *children = NULL;
                unsigned count;
                assert(XQueryTree(test->display, x->xmap.window, &root, &parent, &children, &count));
                if (children) XFree(children);
                assert(parent != root); /* Never map a floating icon during host transitions. */
            }
            XFree(title);
        }
    }
    if (x->type == ClientMessage && x->xclient.message_type == test->opcode && x->xclient.data.l[1] == 0) {
        test->plug = x->xclient.data.l[2];
        XWindowAttributes attributes;
        assert(XGetWindowAttributes(test->display, test->plug, &attributes));
        assert(attributes.map_state == IsUnmapped); /* No floating icon before embedding. */
        test->docks++;
        gtk_socket_add_id(GTK_SOCKET(test->socket), test->plug);
        return GDK_FILTER_REMOVE;
    }
    return GDK_FILTER_CONTINUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    guint timeout = g_timeout_add_seconds(30, deadline, NULL);
    GdkDisplay *display = gdk_display_get_default();
    /* Separate X connections exercise the real cross-client XEmbed protocol. */
    GdkDisplay *other = gdk_display_open(gdk_display_get_name(display));
    assert(other);
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_screen(GTK_WINDOW(window), gdk_display_get_default_screen(other));
    gtk_window_move(GTK_WINDOW(window), 100, 100);
    GtkWidget *socket = gtk_socket_new();
    g_signal_connect(socket, "plug-removed", G_CALLBACK(keep_socket), NULL);
    gtk_widget_set_size_request(socket, 32, 32);
    gtk_container_add(GTK_CONTAINER(window), socket);
    gtk_widget_show_all(window);
    host test = {socket, gdk_x11_display_get_xdisplay(other), 0, 0, 0};
    test.opcode = XInternAtom(test.display, "_NET_SYSTEM_TRAY_OPCODE", False);
    XWindowAttributes root_attributes;
    XGetWindowAttributes(test.display, DefaultRootWindow(test.display), &root_attributes);
    XSelectInput(test.display, DefaultRootWindow(test.display), root_attributes.your_event_mask | SubstructureNotifyMask);
    gdk_window_add_filter(NULL, host_event, &test);
    Atom selection = XInternAtom(test.display, "_NET_SYSTEM_TRAY_S0", False);
    Window owner = gdk_x11_window_get_xid(gtk_widget_get_window(window));
    tray_status status = {0}; status.service.state = XP_SERVICE_STOPPED; status.can_start = true;
    tray_menu menu = {0}; tray_menu_update(&menu, &status, "ndh.xp2");
    tray_xembed *tray = tray_xembed_create(display, &menu, XPILOT_TEST_ICON, activate, available, NULL);
    assert(tray && !usable);
    tray_xembed_enable(tray, true);
    XSetSelectionOwner(test.display, selection, owner, CurrentTime); XFlush(test.display);
    while (!usable) g_main_context_iteration(NULL, TRUE);
    assert(test.docks == 1);
    paint_plug();
    g_print("Initial XEmbed docking passed\n");
    /* SNI handover removes this icon, then loss of SNI permits a fresh dock. */
    tray_xembed_enable(tray, false); assert(!usable);
    tray_xembed_enable(tray, true);
    while (!usable) g_main_context_iteration(NULL, TRUE);
    assert(test.docks == 2);
    paint_plug();
    g_print("XEmbed handover passed\n");
    XSetSelectionOwner(test.display, selection, None, CurrentTime); XFlush(test.display);
    while (usable) g_main_context_iteration(NULL, TRUE);
    XSetSelectionOwner(test.display, selection, owner, CurrentTime); XFlush(test.display);
    while (!usable) g_main_context_iteration(NULL, TRUE);
    assert(test.docks == 3);
    paint_plug();
    g_print("XEmbed host recovery passed\n");
    XWindowAttributes visible;
    do {
        g_main_context_iteration(NULL, TRUE);
        assert(XGetWindowAttributes(test.display, test.plug, &visible));
    } while (visible.map_state != IsViewable || visible.width < 16 || visible.height < 16);
    g_print("XEmbed icon is viewable: %dx%d\n", visible.width, visible.height);
    Window child; int x, y;
    assert(XTranslateCoordinates(test.display, test.plug, DefaultRootWindow(test.display), 8, 8, &x, &y, &child));
    g_print("XEmbed click at %d,%d\n", x, y);
    XTestFakeMotionEvent(test.display, -1, x, y, CurrentTime);
    XTestFakeButtonEvent(test.display, 3, True, CurrentTime);
    XTestFakeButtonEvent(test.display, 3, False, CurrentTime); XFlush(test.display);
    /* Wait for the menu's map event before sending keyboard selection. */
    bool mapped = false;
    while (!mapped) {
        g_main_context_iteration(NULL, TRUE);
        GList *windows = gtk_window_list_toplevels();
        for (GList *p = windows; p; p = p->next) {
            GtkWidget *child_widget = gtk_bin_get_child(GTK_BIN(p->data));
            if (child_widget && GTK_IS_MENU(child_widget) && gtk_widget_get_mapped(child_widget)) {
                mapped = true;
                wait_for_paint(child_widget);
            }
        }
        g_list_free(windows);
    }
    KeyCode down = XKeysymToKeycode(test.display, XK_Down), enter = XKeysymToKeycode(test.display, XK_Return);
    g_print("XEmbed menu opened\n");
    XTestFakeKeyEvent(test.display, down, True, CurrentTime); XTestFakeKeyEvent(test.display, down, False, CurrentTime);
    XTestFakeKeyEvent(test.display, enter, True, CurrentTime); XTestFakeKeyEvent(test.display, enter, False, CurrentTime);
    XFlush(test.display);
    while (!activations) g_main_context_iteration(NULL, TRUE);
    tray_xembed_destroy(tray);
    gdk_window_remove_filter(NULL, host_event, &test);
    gtk_widget_destroy(window);
    gdk_display_sync(other);
    while (g_main_context_iteration(NULL, FALSE)) {}
    /* GDK owns both displays and releases them on process exit. */
    tray_menu_clear(&menu);
    g_source_remove(timeout);
    return 0;
}
