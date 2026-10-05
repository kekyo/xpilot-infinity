#include "tray_platform.h"
#include "service_native.h"
#include "service_config.h"
#include "settings_protocol.h"
#include "tray_sni.h"
#include "tray_xembed.h"
#include <gtk/gtk.h>
#include <stdlib.h>

#define CONFIG_FILE "/etc/default/xpilot-infinity-server"

typedef struct {
    service_native *native;
    tray_controller *controller;
    tray_menu menu;
    tray_sni *sni;
    tray_xembed *xembed;
    bool sni_available;
    bool xembed_available;
    GApplication *application;
    GtkWidget *window;
    GtkWidget *rows[11];
    GFileMonitor *config_monitor;
    GFileMonitor *maps_monitor;
    GFileMonitor *editor_monitor;
    config_editor *editor;
    char *configuration;
    char *applying_snapshot;
    editor_state editing_state;
    bool editor_changed;
    bool editor_launching;
    GDBusConnection *settings_bus;
    gulong settings_closed;
    guint settings_retry;
    unsigned settings_retry_delay;
    GCancellable *settings_cancel;
    unsigned settings_pending;
    bool settings_available;
    bool settings_busy;
    bool maps_changed;
    map_catalog catalog;
    char *generation;
    char *settings_detail;
    char *last_saved_detail;
    bool map_supported;
    bool config_changed;
    bool host_available;
    bool done;
    char *configured_map;
} linux_tray;

static void inspect_settings(linux_tray *tray);
static void update(linux_tray *tray);
static void settings_bus_ready(GObject *object, GAsyncResult *result, void *context);

static void show_details(linux_tray *tray)
{
    const tray_status *status = tray_controller_status(tray->controller);
    char *detail = g_strdup_printf(
        "%s\n%s\n%s\n%s\n\nService: xpilot-infinity-server.service\n"
        "Configuration: " CONFIG_FILE "\n"
        "Logs: journalctl -u xpilot-infinity-server.service\n\n"
        "Install the XPilot Infinity server service if it is not registered.\n"
        "Start and stop may require administrator authentication.\n"
        "Quitting this tray leaves the server running.\n\n"
        "The editing copy is saved separately. Save in your editor, then choose Apply saved changes.\n"
        "Editing copy: %s",
        status->service.detail, status->operation_detail,
        tray->settings_detail ? tray->settings_detail : "",
        tray->last_saved_detail ? tray->last_saved_detail : "",
        tray->editor ? config_editor_path(tray->editor) : "Unavailable");
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(tray->window),
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "%s", detail);
    g_free(detail);
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show(dialog);
}

static void settings_applied(GObject *object, GAsyncResult *result, void *context)
{
    linux_tray *tray = context;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    tray->settings_pending--;
    tray->settings_busy = false;
    if (!tray->done) {
        g_free(tray->settings_detail);
        bool success = false;
        if (reply) {
            gboolean saved, applied;
            const char *generation, *detail;
            g_variant_get(reply, "(bb&s&s)", &saved, &applied, &generation, &detail);
            tray->settings_detail = g_strdup(detail);
            success = applied;
            if (saved && tray->applying_snapshot && tray->editor
                && !config_editor_accept(tray->editor, tray->applying_snapshot, generation)) {
                g_free(tray->settings_detail);
                tray->settings_detail = g_strdup_printf("Configuration saved, but the editing baseline could not be recorded: %s",
                    config_editor_error(tray->editor));
                success = false;
            }
        } else tray->settings_detail = g_strdup(error->message);
        tray->config_changed = true;
        if (!success) show_details(tray);
    }
    g_clear_pointer(&tray->applying_snapshot, free);
    tray->editor_changed = true;
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
}

static void editor_error(linux_tray *tray, const char *detail)
{
    g_free(tray->settings_detail);
    tray->settings_detail = g_strdup(detail);
    show_details(tray);
}

static void editor_launched(GObject *object, GAsyncResult *result, void *context)
{
    linux_tray *tray = context;
    GError *error = NULL;
    bool launched = g_app_info_launch_uris_finish(G_APP_INFO(object), result, &error);
    tray->settings_pending--;
    tray->editor_launching = false;
    if (!tray->done && !launched) editor_error(tray, error->message);
    tray->editor_changed = true;
    g_clear_error(&error);
}

static void open_editor(linux_tray *tray)
{
    tray->config_changed = tray->editor_changed = true;
    update(tray);
    if (!tray->editor || tray->editor_launching) return;
    if (tray->editing_state == EDITOR_NONE && !tray->configuration) {
        editor_error(tray, "The shared configuration is not readable. Ask the administrator to restore normal-user read access."); return;
    }
    if (!config_editor_begin(tray->editor, tray->configuration ? tray->configuration : "",
        tray->generation ? tray->generation : "absent")) {
        editor_error(tray, config_editor_error(tray->editor)); return;
    }
    GAppInfo *application = g_app_info_get_default_for_type("text/plain", FALSE);
    if (!application) { editor_error(tray, "No default text editor is configured for text/plain."); return; }
    GError *error = NULL;
    char *uri = g_filename_to_uri(config_editor_path(tray->editor), NULL, &error);
    if (!uri) {
        editor_error(tray, error->message); g_error_free(error); g_object_unref(application); return;
    }
    GList uris = {uri, NULL, NULL};
    GdkAppLaunchContext *launch = gdk_display_get_app_launch_context(gtk_widget_get_display(tray->window));
    tray->editor_launching = true;
    tray->settings_pending++;
    g_app_info_launch_uris_async(application, &uris, G_APP_LAUNCH_CONTEXT(launch), tray->settings_cancel,
        editor_launched, tray);
    g_object_unref(launch); g_object_unref(application); g_free(uri);
    tray->editor_changed = true;
}

static void editing_response(GtkDialog *dialog, int response, void *context)
{
    linux_tray *tray = context;
    int action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "tray-action"));
    gtk_widget_destroy(GTK_WIDGET(dialog));
    if (response != GTK_RESPONSE_ACCEPT || tray->settings_busy || !tray->editor) return;
    tray->config_changed = tray->editor_changed = true;
    update(tray);
    if (action == TRAY_DISCARD) {
        if (!config_editor_discard(tray->editor)) editor_error(tray, config_editor_error(tray->editor));
        tray->editor_changed = true;
        return;
    }
    if (!tray->settings_bus || !tray->generation || !tray->settings_available) return;
    tray->applying_snapshot = config_editor_snapshot(tray->editor, tray->generation);
    if (!tray->applying_snapshot) { editor_error(tray, config_editor_error(tray->editor)); return; }
    if (!config_editor_prepare(tray->editor, tray->applying_snapshot, tray->generation)) {
        g_clear_pointer(&tray->applying_snapshot, free);
        editor_error(tray, config_editor_error(tray->editor)); return;
    }
    tray->settings_busy = true;
    tray->settings_pending++;
    g_dbus_connection_call(tray->settings_bus, XP_SETTINGS_BUS, XP_SETTINGS_PATH,
        XP_SETTINGS_BUS, "Apply", g_variant_new("(ss)", tray->generation, tray->applying_snapshot),
        G_VARIANT_TYPE("(bbss)"), G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
        G_MAXINT, tray->settings_cancel, settings_applied, tray);
}

static void confirm_editing(linux_tray *tray, int action)
{
    const char *message = action == TRAY_DISCARD
        ? "Discard the editing copy and its saved changes? The shared configuration is unchanged. Close the external editor to prevent it from saving the discarded copy again."
        : "Apply the saved editing copy to " CONFIG_FILE "? Unsaved editor changes are not included. A running service will restart and disconnect players. Administrator authentication may be required.";
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(tray->window), GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), action == TRAY_DISCARD ? "Discard editing copy" : "Apply saved configuration");
    gtk_dialog_add_button(GTK_DIALOG(dialog), action == TRAY_DISCARD ? "_Discard copy" : "_Apply saved changes", GTK_RESPONSE_ACCEPT);
    g_object_set_data(G_OBJECT(dialog), "tray-action", GINT_TO_POINTER(action));
    g_signal_connect(dialog, "response", G_CALLBACK(editing_response), tray);
    gtk_widget_show(dialog);
}

static void editor_changed(GFileMonitor *monitor, GFile *file, GFile *other,
                            GFileMonitorEvent event, void *context)
{
    (void)monitor; (void)file; (void)other; (void)event;
    ((linux_tray *)context)->editor_changed = true;
}

static void select_map(linux_tray *tray, const tray_menu_item *item)
{
    if (!tray->settings_bus || !tray->generation || tray->settings_busy || item->checked) return;
    tray->settings_busy = true;
    tray->settings_pending++;
    g_dbus_connection_call(tray->settings_bus, XP_SETTINGS_BUS, XP_SETTINGS_PATH,
        XP_SETTINGS_BUS, "SelectMap", g_variant_new("(ss)", tray->generation, item->label),
        G_VARIANT_TYPE("(bbss)"), G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION,
        G_MAXINT, tray->settings_cancel, settings_applied, tray);
}

static void popup_clicked(GtkMenuItem *widget, void *context)
{
    linux_tray *tray = context;
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "tray-action"));
    const tray_menu_item *item = tray_menu_find(&tray->menu, id);
    if (item && item->enabled) select_map(tray, item);
    update(tray);
}

static void popup_maps(linux_tray *tray)
{
    tray->maps_changed = tray->config_changed = true;
    update(tray);
    GtkWidget *menu = gtk_menu_new();
    g_object_ref_sink(menu);
    for (size_t i = 0; i < tray->menu.map_count; i++) {
        const tray_menu_item *item = &tray->menu.maps[i];
        GtkWidget *widget = gtk_check_menu_item_new_with_label(item->label);
        gtk_check_menu_item_set_draw_as_radio(GTK_CHECK_MENU_ITEM(widget), TRUE);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), item->checked);
        gtk_widget_set_sensitive(widget, item->enabled);
        g_object_set_data(G_OBJECT(widget), "tray-action", GINT_TO_POINTER(item->id));
        g_signal_connect(widget, "activate", G_CALLBACK(popup_clicked), tray);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), widget);
    }
    g_signal_connect_swapped(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), menu);
    g_signal_connect_swapped(menu, "destroy", G_CALLBACK(g_object_unref), menu);
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), tray->rows[7], GDK_GRAVITY_SOUTH_WEST,
                             GDK_GRAVITY_NORTH_WEST, NULL);
}

static void activate(void *context, int id)
{
    linux_tray *tray = context;
    if (id == -1) {
        tray->config_changed = tray->maps_changed = true;
        inspect_settings(tray);
        update(tray);
        return;
    }
    if (!id) {
        gtk_window_present(GTK_WINDOW(tray->window));
        return;
    }
    const tray_menu_item *item = tray_menu_find(&tray->menu, id);
    if (!item || !item->enabled)
        return;
    if (item->radio) { select_map(tray, item); return; }
    switch (id) {
    case TRAY_START: tray_controller_request(tray->controller, XP_SERVICE_START); break;
    case TRAY_STOP: tray_controller_request(tray->controller, XP_SERVICE_STOP); break;
    case TRAY_MAP_MENU: popup_maps(tray); break;
    case TRAY_EDIT: open_editor(tray); break;
    case TRAY_APPLY: case TRAY_DISCARD: confirm_editing(tray, id); break;
    case TRAY_DETAILS: show_details(tray); break;
    case TRAY_QUIT: tray->done = true; break;
    default: break;
    }
}

static void host_changed(linux_tray *tray)
{
    tray->host_available = tray->sni_available || tray->xembed_available;
    if (tray->host_available)
        gtk_widget_hide(tray->window);
    else
        gtk_widget_show_all(tray->window);
}

static void xembed_available(void *context, bool usable)
{
    linux_tray *tray = context;
    tray->xembed_available = usable;
    host_changed(tray);
}

static void available(void *context, bool usable)
{
    linux_tray *tray = context;
    tray->sni_available = usable;
    tray_xembed_enable(tray->xembed, !usable);
    host_changed(tray);
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
    (void)monitor; (void)other; (void)event;
    linux_tray *tray = context;
    char *name = g_file_get_basename(file);
    if (!strcmp(name, "xpilot-infinity-server") || !strcmp(name, ".xpilot-infinity-settings-result")) tray->config_changed = true;
    g_free(name);
}

static void maps_changed(GFileMonitor *monitor, GFile *file, GFile *other,
                          GFileMonitorEvent event, void *context)
{
    (void)monitor; (void)file; (void)other; (void)event;
    linux_tray *tray = context;
    tray->maps_changed = true;
}

static void settings_inspected(GObject *object, GAsyncResult *result, void *context)
{
    linux_tray *tray = context;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    tray->settings_pending--;
    if (!tray->done) {
        gboolean supported = FALSE;
        const char *detail = error ? error->message : "";
        if (reply) g_variant_get(reply, "(b&s)", &supported, &detail);
        tray->settings_available = supported;
        if (!supported) { g_free(tray->settings_detail); tray->settings_detail = g_strdup(detail); }
    }
    g_clear_pointer(&reply, g_variant_unref); g_clear_error(&error);
}

static void inspect_settings(linux_tray *tray)
{
    if (!tray->settings_bus || tray->settings_pending || tray->done) return;
    tray->settings_pending++;
    g_dbus_connection_call(tray->settings_bus, XP_SETTINGS_BUS, XP_SETTINGS_PATH,
        XP_SETTINGS_BUS, "Inspect", NULL, G_VARIANT_TYPE("(bs)"), G_DBUS_CALL_FLAGS_NONE,
        10000, tray->settings_cancel, settings_inspected, tray);
}

static gboolean reconnect_settings(void *context)
{
    linux_tray *tray = context;
    tray->settings_retry = 0;
    if (!tray->done) {
        tray->settings_pending++;
        g_bus_get(G_BUS_TYPE_SYSTEM, tray->settings_cancel, settings_bus_ready, tray);
    }
    return G_SOURCE_REMOVE;
}

static void retry_settings(linux_tray *tray)
{
    if (tray->done || tray->settings_retry) return;
    tray->settings_retry_delay = MIN(tray->settings_retry_delay ? tray->settings_retry_delay * 2 : 250, 30000);
    tray->settings_retry = g_timeout_add(tray->settings_retry_delay, reconnect_settings, tray);
}

static void settings_bus_closed(GDBusConnection *bus, gboolean remote, GError *error, void *context)
{
    (void)remote; (void)error;
    linux_tray *tray = context;
    tray->settings_available = false;
    g_signal_handler_disconnect(bus, tray->settings_closed);
    tray->settings_closed = 0;
    g_clear_object(&tray->settings_bus);
    retry_settings(tray);
}

static void settings_bus_ready(GObject *object, GAsyncResult *result, void *context)
{
    (void)object;
    linux_tray *tray = context;
    GError *error = NULL;
    tray->settings_bus = g_bus_get_finish(result, &error);
    tray->settings_pending--;
    if (!tray->done) {
        if (error) {
            g_free(tray->settings_detail); tray->settings_detail = g_strdup(error->message);
            retry_settings(tray);
        } else {
            tray->settings_retry_delay = 0;
            g_dbus_connection_set_exit_on_close(tray->settings_bus, FALSE);
            tray->settings_closed = g_signal_connect(tray->settings_bus, "closed", G_CALLBACK(settings_bus_closed), tray);
            inspect_settings(tray);
        }
    }
    g_clear_error(&error);
}

static void update(linux_tray *tray)
{
    if (tray->config_changed) {
        tray->config_changed = false;
        tray->editor_changed = true;
        g_clear_pointer(&tray->configuration, g_free);
        char *text = NULL;
        tray->map_supported = false;
        g_clear_pointer(&tray->last_saved_detail, g_free);
        g_free(tray->generation); tray->generation = NULL;
        free(tray->configured_map);
        tray->configured_map = NULL;
        GError *error = NULL;
        gsize length;
        if (g_file_get_contents(CONFIG_FILE, &text, &length, &error)
            && service_config_valid(text, length)) {
            tray->configuration = g_strdup(text);
            tray->configured_map = service_config_map(text, true);
            tray->generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, text, -1);
            char *check = service_config_select_map(text, XPILOT_MAP_DIRECTORY "/ndh.xp2", true);
            tray->map_supported = check != NULL;
            free(check);
        } else if (g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            tray->generation = g_strdup("absent");
            tray->map_supported = true;
        }
        char *record = NULL;
        gsize record_size = 0;
        if (tray->generation && g_file_get_contents(XP_SETTINGS_RESULT, &record, &record_size, NULL)
            && record_size < 2048 && service_config_valid(record, record_size)) {
            char *line = strchr(record, '\n');
            if (line) {
                *line++ = 0;
                if (!strcmp(record, tray->generation)) {
                    tray->last_saved_detail = g_strdup(line);
                }
            }
        }
        g_free(record);
        g_free(text);
        g_clear_error(&error);
    }
    if (tray->maps_changed) {
        tray->maps_changed = false;
        map_catalog_clear(&tray->catalog);
        map_catalog_read(XPILOT_MAP_DIRECTORY, &tray->catalog);
    }
    if (tray->editor_changed) {
        tray->editor_changed = false;
        if (tray->editor && tray->configuration)
            config_editor_reconcile(tray->editor, tray->configuration, tray->generation);
        tray->editing_state = tray->editor ? config_editor_status(tray->editor, tray->generation) : EDITOR_NONE;
    }
    unsigned revision = tray->menu.revision;
    tray_status status = *tray_controller_status(tray->controller);
    if (tray->settings_busy) { status.busy = true; status.can_start = status.can_stop = false; }
    tray_menu_update(&tray->menu, &status,
        tray->configured_map ? tray->configured_map : "Unavailable or managed by other service settings");
    const char *selected = tray->configured_map;
    size_t prefix = strlen(XPILOT_MAP_DIRECTORY);
    if (selected && !strncmp(selected, XPILOT_MAP_DIRECTORY "/", prefix + 1)) selected += prefix + 1;
    bool stable = status.service.state == XP_SERVICE_STOPPED || status.service.state == XP_SERVICE_RUNNING
        || status.service.state == XP_SERVICE_FAILED;
    tray_menu_set_maps(&tray->menu, &tray->catalog, selected,
        stable && !status.busy && tray->generation && tray->settings_available && tray->map_supported,
        status.service.state == XP_SERVICE_RUNNING);
    tray_menu_set_editor(&tray->menu, tray->editor && tray->configuration && tray->settings_available,
        tray->editing_state, status.busy || tray->editor_launching, status.service.state == XP_SERVICE_RUNNING);
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
    directory = g_file_new_for_path(XPILOT_MAP_DIRECTORY);
    tray.maps_monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, NULL);
    g_object_unref(directory);
    if (tray.maps_monitor) g_signal_connect(tray.maps_monitor, "changed", G_CALLBACK(maps_changed), &tray);
    tray.maps_changed = true;
    char editor_failure[256];
    char *editing_directory = g_build_filename(g_get_user_config_dir(), "xpilot-infinity-server-editor", NULL);
    if (g_mkdir_with_parents(g_get_user_config_dir(), 0700) == 0)
        tray.editor = config_editor_open(editing_directory, editor_failure);
    else snprintf(editor_failure, sizeof(editor_failure), "Cannot create the user configuration directory");
    if (tray.editor) {
        directory = g_file_new_for_path(editing_directory);
        tray.editor_monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, NULL);
        g_object_unref(directory);
        if (tray.editor_monitor) g_signal_connect(tray.editor_monitor, "changed", G_CALLBACK(editor_changed), &tray);
    } else tray.settings_detail = g_strdup(editor_failure);
    g_free(editing_directory);
    tray.editor_changed = true;
    tray.settings_cancel = g_cancellable_new();
    tray.settings_pending++;
    g_bus_get(G_BUS_TYPE_SYSTEM, tray.settings_cancel, settings_bus_ready, &tray);
    update(&tray);
    tray.xembed = tray_xembed_create(gdk_display_get_default(), &tray.menu, XPILOT_TRAY_ICON,
        activate, xembed_available, &tray);
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
    tray_xembed_destroy(tray.xembed);
    if (tray.settings_retry) g_source_remove(tray.settings_retry);
    if (tray.settings_closed) g_signal_handler_disconnect(tray.settings_bus, tray.settings_closed);
    g_cancellable_cancel(tray.settings_cancel);
    while (tray.settings_pending) g_main_context_iteration(NULL, TRUE);
    g_clear_object(&tray.settings_cancel);
    g_clear_object(&tray.settings_bus);
    g_clear_object(&tray.config_monitor);
    g_clear_object(&tray.maps_monitor);
    g_clear_object(&tray.editor_monitor);
    config_editor_close(tray.editor);
    g_free(tray.configuration);
    tray_controller_destroy(tray.controller);
    service_native_destroy(tray.native);
    free(tray.configured_map);
    g_free(tray.generation); g_free(tray.settings_detail); g_free(tray.last_saved_detail);
    map_catalog_clear(&tray.catalog); tray_menu_clear(&tray.menu);
    gtk_widget_destroy(tray.window);
    if (bus)
        g_dbus_connection_flush_sync(bus, NULL, NULL);
    g_object_unref(tray.application);
    return 0;
}
