#define _GNU_SOURCE
#include "settings_protocol.h"
#include "settings_file_linux.h"
#include "service_config.h"
#include "map_catalog.h"
#include "service_native.h"
#include "tray_controller.h"
#include <polkit/polkit.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define SERVER_UNIT "xpilot-infinity-server.service"
#define MANAGER_PATH "/org/freedesktop/systemd1"
#define UNIT_PATH MANAGER_PATH "/unit/xpilot_2dinfinity_2dserver_2eservice"

typedef struct {
    GDBusMethodInvocation *invocation;
    char *generation;
    char *map;
    char *text;
    char *identity;
    char *saved_generation;
    unsigned authorization;
    unsigned phase;
    bool running;
    bool authorized;
    bool saved;
    gint64 deadline;
} settings_request;

typedef struct {
    GDBusConnection *bus;
    PolkitAuthority *authority;
    service_native *native;
    tray_controller *controller;
    settings_request *request;
    bool done;
    bool unit_referenced;
} settings_helper;

static gboolean fail(GError **error, GIOErrorEnum code, const char *message)
{
    g_set_error_literal(error, G_IO_ERROR, code, message);
    return FALSE;
}

static GVariant *properties(settings_helper *helper, const char *interface, GError **error)
{
    if (!helper->unit_referenced) {
        GVariant *reference = g_dbus_connection_call_sync(helper->bus, "org.freedesktop.systemd1",
            MANAGER_PATH, "org.freedesktop.systemd1.Manager", "RefUnit",
            g_variant_new("(s)", SERVER_UNIT), G_VARIANT_TYPE_UNIT,
            G_DBUS_CALL_FLAGS_NONE, 10000, NULL, error);
        if (!reference) return NULL;
        g_variant_unref(reference);
        helper->unit_referenced = true;
    }
    GVariant *reply = g_dbus_connection_call_sync(helper->bus, "org.freedesktop.systemd1",
        UNIT_PATH, "org.freedesktop.DBus.Properties", "GetAll", g_variant_new("(s)", interface),
        G_VARIANT_TYPE("(a{sv})"), G_DBUS_CALL_FLAGS_NONE, 10000, NULL, error);
    if (!reply) return NULL;
    GVariant *result = g_variant_get_child_value(reply, 0);
    g_variant_unref(reply);
    return result;
}

static char *service_identity(settings_helper *helper, bool *running, GError **error)
{
    GVariant *unit = properties(helper, "org.freedesktop.systemd1.Unit", error);
    if (!unit) return NULL;
    const char *active = "";
    g_variant_lookup(unit, "ActiveState", "&s", &active);
    *running = !strcmp(active, "active");
    char *identity = NULL;
    if (!strcmp(active, "active") || !strcmp(active, "inactive") || !strcmp(active, "failed")) {
        GVariant *invocation = g_variant_lookup_value(unit, "InvocationID", G_VARIANT_TYPE_BYTESTRING);
        if (invocation) {
            identity = g_variant_print(invocation, FALSE);
            g_variant_unref(invocation);
        }
    }
    g_variant_unref(unit);
    if (!identity) fail(error, G_IO_ERROR_BUSY, "Service is transitioning or cannot be identified");
    return identity;
}

static gboolean service_layout(settings_helper *helper, GError **error)
{
    GVariant *service = properties(helper, "org.freedesktop.systemd1.Service", error);
    if (!service) return FALSE;
    gboolean valid = TRUE;
    GVariant *commands = g_variant_lookup_value(service, "ExecStartEx", G_VARIANT_TYPE("a(sasasttttuii)"));
    if (!commands || g_variant_n_children(commands) != 1) valid = FALSE;
    if (valid) {
        GVariant *command = g_variant_get_child_value(commands, 0);
        GVariant *executable = g_variant_get_child_value(command, 0);
        GVariant *arguments = g_variant_get_child_value(command, 1);
        GVariant *flags = g_variant_get_child_value(command, 2);
        const char **argv = g_variant_get_strv(arguments, NULL);
        valid = !strcmp(g_variant_get_string(executable, NULL), "/usr/games/xpilot-infinity-server")
            && g_variant_n_children(arguments) == 2
            && !strcmp(argv[0], "/usr/games/xpilot-infinity-server")
            && !strcmp(argv[1], "$XPILOT_SERVER_OPTIONS") && !g_variant_n_children(flags);
        g_free(argv);
        g_variant_unref(executable); g_variant_unref(arguments); g_variant_unref(flags);
        g_variant_unref(command);
    }
    g_clear_pointer(&commands, g_variant_unref);
    GVariant *files = g_variant_lookup_value(service, "EnvironmentFiles", G_VARIANT_TYPE("a(sb)"));
    if (!files || g_variant_n_children(files) != 1) valid = FALSE;
    else {
        const char *path;
        gboolean optional;
        g_variant_get_child(files, 0, "(&sb)", &path, &optional);
        if (strcmp(path, XP_SETTINGS_FILE)) valid = FALSE;
    }
    g_clear_pointer(&files, g_variant_unref);
    const char *strings[] = {"RootDirectory", "RootImage"};
    for (size_t i = 0; i < G_N_ELEMENTS(strings); i++) {
        const char *value = "";
        g_variant_lookup(service, strings[i], "&s", &value);
        if (*value) valid = FALSE;
    }
    GVariant *unset = g_variant_lookup_value(service, "UnsetEnvironment", G_VARIANT_TYPE_STRING_ARRAY);
    if (unset) {
        GVariantIter iter;
        const char *value;
        g_variant_iter_init(&iter, unset);
        while (g_variant_iter_next(&iter, "&s", &value))
            if (!strncmp(value, "XPILOT_SERVER_OPTIONS", 21)) valid = FALSE;
        g_variant_unref(unset);
    }
    GVariant *environment = g_variant_lookup_value(service, "Environment", G_VARIANT_TYPE_STRING_ARRAY);
    bool default_options = false;
    if (environment) {
        GVariantIter iter;
        const char *value;
        g_variant_iter_init(&iter, environment);
        while (g_variant_iter_next(&iter, "&s", &value)) {
            if (!strncmp(value, "XPILOT_SERVER_OPTIONS=", 22))
                default_options = !strcmp(value, "XPILOT_SERVER_OPTIONS=-noQuit +reportMeta -map ndh.xp2");
        }
        g_variant_unref(environment);
    }
    valid = valid && default_options;
    g_variant_unref(service);
    return valid || fail(error, G_IO_ERROR_NOT_SUPPORTED,
        "Service overrides do not use the supported shared configuration and executable");
}

static void finish(settings_helper *helper, bool applied, const char *error_name, const char *detail)
{
    settings_request *request = helper->request;
    if (request->saved) {
        GError *record_error = NULL;
        settings_file *file = settings_file_open(XP_SETTINGS_FILE, request->saved_generation, &record_error);
        if (file) settings_file_record(file, request->saved_generation, detail, &record_error);
        if (record_error) g_warning("Could not persist operation result: %s", record_error->message);
        settings_file_close(file); g_clear_error(&record_error);
    }
    if (request->saved || !error_name)
        g_dbus_method_invocation_return_value(request->invocation,
            g_variant_new("(bbss)", request->saved, applied,
                request->saved_generation ? request->saved_generation : request->generation, detail));
    else {
        char *name = g_strconcat(XP_SETTINGS_BUS ".", error_name, NULL);
        g_dbus_method_invocation_return_dbus_error(request->invocation, name, detail);
        g_free(name);
    }
    g_object_unref(request->invocation);
    g_free(request->generation); g_free(request->map); g_free(request->text); g_free(request->identity);
    g_free(request->saved_generation); g_free(request);
    helper->request = NULL;
}

static void authorize(settings_helper *helper);

static void authorization_ready(GObject *object, GAsyncResult *result, void *context)
{
    settings_helper *helper = context;
    GError *error = NULL;
    PolkitAuthorizationResult *authorization = polkit_authority_check_authorization_finish(
        POLKIT_AUTHORITY(object), result, &error);
    if (!authorization) {
        finish(helper, false, "AuthorizationUnavailable", error->message);
        g_clear_error(&error);
        return;
    }
    bool allowed = polkit_authorization_result_get_is_authorized(authorization);
    bool dismissed = polkit_authorization_result_get_dismissed(authorization);
    bool challenge = polkit_authorization_result_get_is_challenge(authorization);
    g_object_unref(authorization);
    if (!allowed) {
        finish(helper, false, dismissed ? "Canceled" : challenge ? "AuthenticationRequired" : "Denied",
            dismissed ? "Authentication was canceled; nothing changed"
            : challenge ? "Authentication could not be completed; check the desktop authentication agent"
            : "Permission denied; nothing changed");
        return;
    }
    helper->request->authorization++;
    authorize(helper);
}

static void authorize(settings_helper *helper)
{
    settings_request *request = helper->request;
    if (request->authorization == (request->running ? 3u : 1u)) {
        request->authorized = true;
        return;
    }
    PolkitSubject *subject = polkit_system_bus_name_new(
        g_dbus_method_invocation_get_sender(request->invocation));
    PolkitDetails *details = polkit_details_new();
    polkit_details_insert(details, "unit", SERVER_UNIT);
    const char *action = request->text ? XP_SETTINGS_EDIT_ACTION : XP_SETTINGS_MAP_ACTION;
    if (request->authorization) {
        action = "org.freedesktop.systemd1.manage-units";
        polkit_details_insert(details, "verb", request->authorization == 1 ? "stop" : "start");
    }
    polkit_authority_check_authorization(helper->authority, subject, action, details,
        POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, NULL, authorization_ready, helper);
    g_object_unref(details); g_object_unref(subject);
}

static char *selected_path(const char *id, GError **error)
{
    if (!map_catalog_name(id)) {
        fail(error, G_IO_ERROR_INVALID_ARGUMENT, "Invalid map identifier"); return NULL;
    }
    int directory = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    char **components = g_strsplit(XPILOT_MAP_DIRECTORY, "/", -1);
    struct stat st;
    for (size_t i = 0; directory >= 0 && components[i]; i++) {
        if (!components[i][0]) continue;
        int next = openat(directory, components[i], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        close(directory); directory = next;
        if (directory >= 0 && (fstat(directory, &st) || st.st_uid || (st.st_mode & 0022))) {
            close(directory); directory = -1;
        }
    }
    g_strfreev(components);
    if (directory < 0) {
        if (directory >= 0) close(directory);
        fail(error, G_IO_ERROR_PERMISSION_DENIED, "Shared map directory is not administrator-managed"); return NULL;
    }
    int map = openat(directory, id, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    close(directory);
    if (map < 0) { fail(error, G_IO_ERROR_NOT_FOUND, "Selected map is unavailable"); return NULL; }
    bool valid = !fstat(map, &st) && S_ISREG(st.st_mode) && !st.st_uid
        && !(st.st_mode & 0022) && (st.st_mode & 0004);
    close(map);
    if (!valid) { fail(error, G_IO_ERROR_PERMISSION_DENIED, "Selected map must be protected and readable by the service"); return NULL; }
    return g_build_filename(XPILOT_MAP_DIRECTORY, id, NULL);
}

static void advance(settings_helper *helper)
{
    settings_request *request = helper->request;
    if (!request || !request->authorized) return;
    GError *error = NULL;
    if (request->phase == 0) {
        bool running;
        char *identity = service_identity(helper, &running, &error);
        if (!identity || strcmp(identity, request->identity) || running != request->running
            || !service_layout(helper, &error)) {
            finish(helper, false, "Conflict", error ? error->message : "Service changed during authorization");
            g_free(identity); g_clear_error(&error); return;
        }
        g_free(identity);
        settings_file *file = settings_file_open(XP_SETTINGS_FILE, request->generation, &error);
        char *path = file && !request->text ? selected_path(request->map, &error) : NULL;
        char *replacement = file && request->text ? g_strdup(request->text)
            : path ? service_config_select_map(settings_file_text(file), path, true) : NULL;
        g_free(path);
        if (!file || !replacement) {
            finish(helper, false, "Configuration", error ? error->message : "Configuration uses unsupported or ambiguous options");
            settings_file_close(file); free(replacement); g_clear_error(&error); return;
        }
        if (!strcmp(replacement, settings_file_text(file))) {
            finish(helper, !running, NULL, "Configuration is unchanged; no restart requested");
            settings_file_close(file); free(replacement); return;
        }
        request->saved_generation = g_compute_checksum_for_string(G_CHECKSUM_SHA256, replacement, -1);
        if (!settings_file_record(file, request->saved_generation,
            "Configuration save/restart is pending or was interrupted; active settings are unverified", &error)
            || !settings_file_commit(file, replacement, &error)) {
            finish(helper, false, "SaveFailed", error->message);
            settings_file_close(file); free(replacement); g_clear_error(&error); return;
        }
        request->saved = true;
        settings_file_close(file); free(replacement);
        if (!running) { finish(helper, true, NULL, "Configuration saved; service remains stopped"); return; }
        request->deadline = g_get_monotonic_time() + 180 * G_TIME_SPAN_SECOND;
        request->phase = 1;
        if (!tray_controller_request(helper->controller, XP_SERVICE_STOP))
            finish(helper, false, NULL, "Configuration saved; service changed before it could be stopped");
        return;
    }
    const tray_status *status = tray_controller_status(helper->controller);
    if (status->operation_error || status->service.error || g_get_monotonic_time() >= request->deadline) {
        finish(helper, false, NULL, "Configuration saved; service restart failed or exceeded its deadline. See the service journal");
        return;
    }
    if (status->busy) return;
    if (request->phase == 1 && status->service.state == XP_SERVICE_STOPPED) {
        bool running;
        char *identity = service_identity(helper, &running, &error);
        settings_file *file = identity && !running && !strcmp(identity, request->identity)
            ? settings_file_open(XP_SETTINGS_FILE, request->saved_generation, &error) : NULL;
        if (!file) {
            finish(helper, false, NULL, "Configuration saved; an external change interrupted the restart");
            g_free(identity); g_clear_error(&error); return;
        }
        settings_file_close(file); g_free(identity);
        request->phase = 2;
        if (!tray_controller_request(helper->controller, XP_SERVICE_START))
            finish(helper, false, NULL, "Configuration saved; service could not be started");
    } else if (request->phase == 2 && status->service.state == XP_SERVICE_RUNNING)
        finish(helper, true, NULL, "Configuration saved; service running (game readiness is not verified)");
    else
        finish(helper, false, NULL, "Configuration saved; unexpected service state interrupted the restart");
}

static void method_call(GDBusConnection *bus, const char *sender, const char *path,
    const char *interface, const char *method, GVariant *parameters,
    GDBusMethodInvocation *invocation, void *context)
{
    settings_helper *helper = context;
    (void)bus; (void)sender; (void)path; (void)interface;
    GError *error = NULL;
    if (!strcmp(method, "Inspect")) {
        bool supported = service_layout(helper, &error);
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(bs)", supported, error ? error->message : ""));
        g_clear_error(&error); return;
    }
    if (helper->request) {
        g_dbus_method_invocation_return_dbus_error(invocation, XP_SETTINGS_BUS ".Busy", "Another configuration operation is active"); return;
    }
    const char *generation, *value;
    bool edit = !strcmp(method, "Apply");
    g_variant_get(parameters, "(&s&s)", &generation, &value);
    if ((strlen(generation) != 64 && strcmp(generation, "absent"))
        || (edit ? !service_config_valid(value, strlen(value)) : !map_catalog_name(value))) {
        g_dbus_method_invocation_return_dbus_error(invocation, XP_SETTINGS_BUS ".Invalid", "Invalid generation, UTF-8 snapshot or map identifier"); return;
    }
    settings_request *request = g_new0(settings_request, 1);
    request->invocation = g_object_ref(invocation);
    request->generation = g_strdup(generation);
    if (edit) request->text = g_strdup(value); else request->map = g_strdup(value);
    helper->request = request;
    request->identity = service_identity(helper, &request->running, &error);
    if (!request->identity || !service_layout(helper, &error)) {
        finish(helper, false, "Unsupported", error->message); g_clear_error(&error); return;
    }
    authorize(helper);
}

static void name_lost(GDBusConnection *bus, const char *name, void *context)
{
    (void)bus; (void)name;
    settings_helper *helper = context;
    helper->done = true;
}

int main(void)
{
    if (geteuid() != 0) { g_printerr("This helper is activated by the system bus as root.\n"); return 1; }
    settings_helper helper = {0};
    GError *error = NULL;
    helper.bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (!helper.bus) { g_printerr("%s\n", error->message); g_error_free(error); return 1; }
    helper.authority = polkit_authority_get_sync(NULL, &error);
    if (!helper.authority) { g_printerr("%s\n", error->message); g_error_free(error); g_object_unref(helper.bus); return 1; }
    const char *xml = "<node><interface name='" XP_SETTINGS_BUS "'>"
        "<method name='Inspect'><arg type='b' direction='out'/><arg type='s' direction='out'/></method>"
        "<method name='SelectMap'><arg type='s' direction='in'/><arg type='s' direction='in'/>"
        "<arg type='b' direction='out'/><arg type='b' direction='out'/>"
        "<arg type='s' direction='out'/><arg type='s' direction='out'/></method>"
        "<method name='Apply'><arg type='s' direction='in'/><arg type='s' direction='in'/>"
        "<arg type='b' direction='out'/><arg type='b' direction='out'/>"
        "<arg type='s' direction='out'/><arg type='s' direction='out'/></method></interface></node>";
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, &error);
    const GDBusInterfaceVTable vtable = {method_call, NULL, NULL, {0}};
    guint registration = g_dbus_connection_register_object(helper.bus, XP_SETTINGS_PATH,
        info->interfaces[0], &vtable, &helper, NULL, &error);
    if (!registration) { g_printerr("%s\n", error->message); return 1; }
    guint owner = g_bus_own_name_on_connection(helper.bus, XP_SETTINGS_BUS,
        G_BUS_NAME_OWNER_FLAGS_NONE, NULL, name_lost, &helper, NULL);
    helper.native = service_native_create();
    helper.controller = tray_controller_create(service_native_control(helper.native));
    tray_controller_connect(helper.controller);
    while (!helper.done) {
        service_native_dispatch(helper.native, 1000 * 60);
        advance(&helper);
    }
    /* Accepted updates outlive their UI. Losing the system bus is an interrupted
     * operation; atomic replacement still prevents a partially written file. */
    g_bus_unown_name(owner);
    g_dbus_connection_unregister_object(helper.bus, registration);
    tray_controller_destroy(helper.controller);
    service_native_destroy(helper.native);
    g_dbus_node_info_unref(info);
    g_object_unref(helper.authority); g_object_unref(helper.bus);
    return 0;
}
