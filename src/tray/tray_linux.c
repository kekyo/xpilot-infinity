#include "tray_platform.h"
#include "service_native.h"
#include "service_config.h"
#include "tray_sni.h"
#include <gtk/gtk.h>
#include <stdlib.h>

#define CONFIG_FILE "/etc/default/xpilot-infinity-server"

typedef struct {
    service_native *native;
    tray_controller *controller;
    tray_menu menu;
    tray_sni *sni;
    GApplication *application;
    GtkWidget *window;
    GtkWidget *rows[7];
    GFileMonitor *config_monitor;
    bool config_changed;
    bool host_available;
    bool done;
    char *configured_map;
} linux_tray;

static void show_details(linux_tray *tray)
{
    const tray_status *status = tray_controller_status(tray->controller);
    char *detail = g_strdup_printf(
        "%s\n%s\n\nService: xpilot-infinity-server.service\n"
        "Configuration: " CONFIG_FILE "\n"
        "Logs: journalctl -u xpilot-infinity-server.service\n\n"
        "Install the XPilot Infinity server service if it is not registered.\n"
        "Start and stop may require administrator authentication.\n"
        "Quitting this tray leaves the server running.",
        status->service.detail, status->operation_detail);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(tray->window),
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", detail);
    g_free(detail);
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show(dialog);
}

static void activate(void *context, int id)
{
    linux_tray *tray = context;
    if (!id) {
        gtk_window_present(GTK_WINDOW(tray->window));
        return;
    }
    const tray_menu_item *item = tray_menu_find(&tray->menu, id);
    if (!item || !item->enabled)
        return;
    switch (id) {
    case TRAY_START: tray_controller_request(tray->controller, XP_SERVICE_START); break;
    case TRAY_STOP: tray_controller_request(tray->controller, XP_SERVICE_STOP); break;
    case TRAY_DETAILS: show_details(tray); break;
    case TRAY_QUIT: tray->done = true; break;
    default: break;
    }
}

static void available(void *context, bool usable)
{
    linux_tray *tray = context;
    tray->host_available = usable;
    if (usable)
        gtk_widget_hide(tray->window);
    else
        gtk_widget_show_all(tray->window);
}

static void clicked(GtkButton *button, void *context)
{
    activate(context, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "tray-action")));
}

static gboolean window_closed(GtkWidget *widget, GdkEvent *event, void *context)
{
    linux_tray *tray = context;
    (void)event;
    if (tray->host_available)
        gtk_widget_hide(widget);
    else
        tray->done = true;
    return TRUE;
}

static void application_activated(GApplication *application, void *context)
{
    (void)application;
    linux_tray *tray = context;
    if (tray->window)
        gtk_window_present(GTK_WINDOW(tray->window));
}

static void config_changed(GFileMonitor *monitor, GFile *file, GFile *other,
                            GFileMonitorEvent event, void *context)
{
    (void)monitor; (void)file; (void)other; (void)event;
    linux_tray *tray = context;
    tray->config_changed = true;
}

static void update(linux_tray *tray)
{
    if (tray->config_changed) {
        tray->config_changed = false;
        char *text = NULL;
        free(tray->configured_map);
        tray->configured_map = NULL;
        if (g_file_get_contents(CONFIG_FILE, &text, NULL, NULL))
            tray->configured_map = service_config_map(text, true);
        g_free(text);
    }
    unsigned revision = tray->menu.revision;
    const tray_status *status = tray_controller_status(tray->controller);
    tray_menu_update(&tray->menu, status,
        tray->configured_map ? tray->configured_map : "Unavailable or managed by other service settings");
    if (revision != tray->menu.revision) {
        for (size_t i = 0; i < G_N_ELEMENTS(tray->menu.items); i++) {
            const tray_menu_item *item = &tray->menu.items[i];
            if (item->id == TRAY_SEPARATOR)
                continue;
            if (GTK_IS_LABEL(tray->rows[i]))
                gtk_label_set_text(GTK_LABEL(tray->rows[i]), item->label);
            else {
                gtk_button_set_label(GTK_BUTTON(tray->rows[i]), item->label);
                gtk_widget_set_sensitive(tray->rows[i], item->enabled);
            }
        }
        if (tray->sni)
            tray_sni_update(tray->sni);
    }
}

int tray_platform_run(int argc, char **argv)
{
    if (!gtk_init_check(&argc, &argv)) {
        g_printerr("A desktop display is required to run the server tray.\n");
        return 1;
    }
    linux_tray tray = {0};
    GError *error = NULL;
    tray.application = g_application_new("org.xpilot.Infinity.ServerTray", 0);
    g_signal_connect(tray.application, "activate", G_CALLBACK(application_activated), &tray);
    if (!g_application_register(tray.application, NULL, &error)) {
        g_printerr("Could not register the tray application: %s\n", error->message);
        g_clear_error(&error);
        g_object_unref(tray.application);
        return 1;
    }
    if (g_application_get_is_remote(tray.application)) {
        g_application_activate(tray.application);
        g_object_unref(tray.application);
        return 0;
    }
    tray.native = service_native_create();
    tray.controller = tray_controller_create(service_native_control(tray.native));
    if (!tray.controller) {
        service_native_destroy(tray.native);
        g_object_unref(tray.application);
        return 1;
    }
    tray_controller_connect(tray.controller);
    tray_menu_update(&tray.menu, tray_controller_status(tray.controller), "Checking configuration");
    tray.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(tray.window), "XPilot Infinity Server");
    gtk_window_set_default_size(GTK_WINDOW(tray.window), 440, -1);
    gtk_window_set_icon_from_file(GTK_WINDOW(tray.window), XPILOT_TRAY_ICON, NULL);
    g_signal_connect(tray.window, "delete-event", G_CALLBACK(window_closed), &tray);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(tray.window), box);
    for (size_t i = 0; i < G_N_ELEMENTS(tray.menu.items); i++) {
        const tray_menu_item *item = &tray.menu.items[i];
        GtkWidget *widget;
        if (item->id == TRAY_SEPARATOR)
            widget = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        else if (item->id == TRAY_STATUS || item->id == TRAY_MAP) {
            widget = gtk_label_new(item->label);
            gtk_label_set_line_wrap(GTK_LABEL(widget), TRUE);
            gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
        } else {
            widget = gtk_button_new_with_label(item->label);
            gtk_widget_set_sensitive(widget, item->enabled);
            g_object_set_data(G_OBJECT(widget), "tray-action", GINT_TO_POINTER(item->id));
            g_signal_connect(widget, "clicked", G_CALLBACK(clicked), &tray);
        }
        tray.rows[i] = widget;
        gtk_box_pack_start(GTK_BOX(box), widget, FALSE, FALSE, 0);
    }
    GFile *directory = g_file_new_for_path("/etc/default");
    tray.config_monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, NULL);
    g_object_unref(directory);
    if (tray.config_monitor)
        g_signal_connect(tray.config_monitor, "changed", G_CALLBACK(config_changed), &tray);
    tray.config_changed = true;
    update(&tray);
    GDBusConnection *bus = g_application_get_dbus_connection(tray.application);
    if (bus)
        tray.sni = tray_sni_create(bus, &tray.menu, XPILOT_TRAY_ICON,
                                   activate, available, &tray, &error);
    if (!tray.sni) {
        if (error) g_printerr("Tray unavailable: %s\n", error->message);
        g_clear_error(&error);
        available(&tray, false);
    }
    while (!tray.done) {
        service_native_dispatch(tray.native, 60000);
        update(&tray);
    }
    tray_sni_destroy(tray.sni);
    g_clear_object(&tray.config_monitor);
    tray_controller_destroy(tray.controller);
    service_native_destroy(tray.native);
    free(tray.configured_map);
    gtk_widget_destroy(tray.window);
    if (bus)
        g_dbus_connection_flush_sync(bus, NULL, NULL);
    g_object_unref(tray.application);
    return 0;
}
