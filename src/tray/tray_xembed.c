#include "tray_xembed.h"
#include <gtk/gtkx.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>

struct tray_xembed {
    GdkDisplay *display;
    Display *xdisplay;
    Atom selection;
    Window owner;
    int event_base;
    bool enabled;
    bool usable;
    GtkWidget *plug;
    GtkWidget *popup;
    guint redock;
    char *icon;
    const tray_menu *menu;
    void (*activate)(void *, int);
    void (*available)(void *, bool);
    void *context;
};

static void dock(tray_xembed *tray);

static void availability(tray_xembed *tray, bool usable)
{
    if (tray->usable == usable) return;
    tray->usable = usable;
    tray->available(tray->context, usable);
}

static void remove_plug(tray_xembed *tray)
{
    if (tray->redock) { g_source_remove(tray->redock); tray->redock = 0; }
    if (tray->popup) gtk_widget_destroy(tray->popup);
    if (tray->plug) {
        g_signal_handlers_disconnect_by_data(tray->plug, tray);
        gtk_widget_destroy(tray->plug);
        tray->plug = NULL;
    }
    availability(tray, false);
}

static gboolean redock(void *context)
{
    tray_xembed *tray = context;
    tray->redock = 0;
    remove_plug(tray);
    dock(tray);
    return G_SOURCE_REMOVE;
}

static gboolean unplugged(GtkWidget *widget, GdkEvent *event, void *context)
{
    (void)event;
    tray_xembed *tray = context;
    /* GtkPlug reports loss of its socket as delete-event. Hide before any
     * toplevel mapping, then recheck the owner on the next event-loop turn. */
    gtk_widget_hide(widget);
    availability(tray, false);
    if (!tray->redock) tray->redock = g_idle_add(redock, tray);
    return TRUE;
}

static void embedded(GtkPlug *plug, void *context)
{
    tray_xembed *tray = context;
    if (!tray->enabled || !gtk_plug_get_embedded(plug)) return;
    gtk_widget_show_all(GTK_WIDGET(plug));
    availability(tray, true);
}

static void selected(GtkMenuItem *widget, void *context)
{
    tray_xembed *tray = context;
    tray->activate(tray->context, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "tray-action")));
}

static GtkWidget *menu_row(tray_xembed *tray, const tray_menu_item *item)
{
    if (item->id == TRAY_SEPARATOR) return gtk_separator_menu_item_new();
    GtkWidget *row = item->radio ? gtk_check_menu_item_new_with_label(item->label)
        : gtk_menu_item_new_with_label(item->label);
    if (item->radio) {
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(row), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(row), item->checked);
    }
    gtk_widget_set_sensitive(row, item->enabled);
    if (item->id == TRAY_MAP_MENU) {
        GtkWidget *maps = gtk_menu_new();
        for (size_t i = 0; i < tray->menu->map_count; i++)
            gtk_menu_shell_append(GTK_MENU_SHELL(maps), menu_row(tray, &tray->menu->maps[i]));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(row), maps);
    } else {
        g_object_set_data(G_OBJECT(row), "tray-action", GINT_TO_POINTER(item->id));
        g_signal_connect(row, "activate", G_CALLBACK(selected), tray);
    }
    return row;
}

static void popup_destroyed(GtkWidget *widget, void *context)
{
    tray_xembed *tray = context;
    tray->popup = NULL;
    g_object_unref(widget);
}

static gboolean popup(GtkWidget *widget, GdkEventButton *event, void *context)
{
    tray_xembed *tray = context;
    if (event && event->button != 1 && event->button != 3) return FALSE;
    tray->activate(tray->context, -1);
    if (tray->popup) gtk_widget_destroy(tray->popup);
    tray->popup = gtk_menu_new();
    g_object_ref_sink(tray->popup);
    for (size_t i = 0; i < G_N_ELEMENTS(tray->menu->items); i++)
        gtk_menu_shell_append(GTK_MENU_SHELL(tray->popup), menu_row(tray, &tray->menu->items[i]));
    g_signal_connect(tray->popup, "destroy", G_CALLBACK(popup_destroyed), tray);
    g_signal_connect_swapped(tray->popup, "selection-done", G_CALLBACK(gtk_widget_destroy), tray->popup);
    gtk_widget_show_all(tray->popup);
    gtk_menu_popup_at_widget(GTK_MENU(tray->popup), widget, GDK_GRAVITY_SOUTH_WEST,
        GDK_GRAVITY_NORTH_WEST, (GdkEvent *)event);
    return TRUE;
}

static gboolean keyboard_popup(GtkWidget *widget, void *context) { return popup(widget, NULL, context); }

static void dock(tray_xembed *tray)
{
    if (!tray->enabled || tray->plug) return;
    tray->owner = XGetSelectionOwner(tray->xdisplay, tray->selection);
    if (!tray->owner) return;
    tray->plug = gtk_plug_new_for_display(tray->display, 0);
    GdkScreen *screen = gtk_widget_get_screen(tray->plug);
    Atom actual;
    int format;
    unsigned long count, left;
    unsigned char *value = NULL;
    gdk_x11_display_error_trap_push(tray->display);
    int result = XGetWindowProperty(tray->xdisplay, tray->owner,
        XInternAtom(tray->xdisplay, "_NET_SYSTEM_TRAY_VISUAL", False), 0, 1, False,
        XA_VISUALID, &actual, &format, &count, &left, &value);
    if (result == Success && actual == XA_VISUALID && format == 32 && count == 1) {
        GdkVisual *visual = gdk_x11_screen_lookup_visual(screen, *(unsigned long *)value);
        if (visual) gtk_widget_set_visual(tray->plug, visual);
    }
    if (value) XFree(value);
    gdk_x11_display_error_trap_pop_ignored(tray->display);
    gtk_window_set_title(GTK_WINDOW(tray->plug), "XPilot Infinity Server Tray");
    gtk_window_set_icon_from_file(GTK_WINDOW(tray->plug), tray->icon, NULL);
    gtk_widget_set_tooltip_text(tray->plug, "XPilot Infinity Server");
    GtkWidget *button = gtk_event_box_new();
    gtk_widget_add_events(button, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
    GdkPixbuf *pixels = gdk_pixbuf_new_from_file_at_scale(tray->icon, 22, 22, TRUE, NULL);
    GtkWidget *image = pixels ? gtk_image_new_from_pixbuf(pixels) : gtk_image_new_from_icon_name("network-server", GTK_ICON_SIZE_MENU);
    g_clear_object(&pixels);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_container_add(GTK_CONTAINER(tray->plug), button);
    gtk_widget_set_can_focus(button, TRUE);
    g_signal_connect(button, "button-press-event", G_CALLBACK(popup), tray);
    g_signal_connect(button, "popup-menu", G_CALLBACK(keyboard_popup), tray);
    g_signal_connect(tray->plug, "embedded", G_CALLBACK(embedded), tray);
    g_signal_connect(tray->plug, "delete-event", G_CALLBACK(unplugged), tray);
    gtk_widget_add_events(tray->plug, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(tray->plug, "button-press-event", G_CALLBACK(popup), tray);
    /* Realize without mapping: an absent or slow host must not leave a floating icon. */
    gtk_widget_realize(tray->plug);
    XEvent request = {0};
    request.xclient.type = ClientMessage;
    request.xclient.window = tray->owner;
    request.xclient.message_type = XInternAtom(tray->xdisplay, "_NET_SYSTEM_TRAY_OPCODE", False);
    request.xclient.format = 32;
    request.xclient.data.l[0] = CurrentTime;
    request.xclient.data.l[1] = 0;
    request.xclient.data.l[2] = gtk_plug_get_id(GTK_PLUG(tray->plug));
    gdk_x11_display_error_trap_push(tray->display);
    XSendEvent(tray->xdisplay, tray->owner, False, NoEventMask, &request);
    gdk_x11_display_error_trap_pop_ignored(tray->display);
    XFlush(tray->xdisplay);
}

static GdkFilterReturn selection_changed(GdkXEvent *raw, GdkEvent *event, void *context)
{
    (void)event;
    tray_xembed *tray = context;
    XEvent *x = raw;
    if (x->xany.display == tray->xdisplay && x->type == tray->event_base + XFixesSelectionNotify) {
        XFixesSelectionNotifyEvent *notification = raw;
        if (notification->selection == tray->selection) {
            Window owner = XGetSelectionOwner(tray->xdisplay, tray->selection);
            if (owner != tray->owner) { remove_plug(tray); tray->owner = owner; dock(tray); }
        }
    }
    return GDK_FILTER_CONTINUE;
}

tray_xembed *tray_xembed_create(GdkDisplay *display, const tray_menu *menu, const char *icon,
    void (*activate)(void *, int), void (*available)(void *, bool), void *context)
{
    if (!GDK_IS_X11_DISPLAY(display)) return NULL;
    Display *xdisplay = gdk_x11_display_get_xdisplay(display);
    int event_base, error_base;
    if (!XFixesQueryExtension(xdisplay, &event_base, &error_base)) return NULL;
    tray_xembed *tray = g_new0(tray_xembed, 1);
    tray->display = g_object_ref(display); tray->xdisplay = xdisplay; tray->event_base = event_base;
    tray->menu = menu; tray->icon = g_strdup(icon);
    tray->activate = activate; tray->available = available; tray->context = context;
    char *name = g_strdup_printf("_NET_SYSTEM_TRAY_S%d", DefaultScreen(xdisplay));
    tray->selection = XInternAtom(xdisplay, name, False); g_free(name);
    XFixesSelectSelectionInput(xdisplay, DefaultRootWindow(xdisplay), tray->selection,
        XFixesSetSelectionOwnerNotifyMask | XFixesSelectionWindowDestroyNotifyMask | XFixesSelectionClientCloseNotifyMask);
    gdk_window_add_filter(NULL, selection_changed, tray);
    return tray;
}

void tray_xembed_enable(tray_xembed *tray, bool enabled)
{
    if (!tray || tray->enabled == enabled) return;
    tray->enabled = enabled;
    if (enabled) dock(tray); else remove_plug(tray);
}

void tray_xembed_destroy(tray_xembed *tray)
{
    if (!tray) return;
    tray->enabled = false;
    remove_plug(tray);
    XFixesSelectSelectionInput(tray->xdisplay, DefaultRootWindow(tray->xdisplay), tray->selection, 0);
    gdk_window_remove_filter(NULL, selection_changed, tray);
    g_object_unref(tray->display); g_free(tray->icon); g_free(tray);
}
