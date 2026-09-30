#include "service_native.h"
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#define MANAGER "org.freedesktop.systemd1"
#define MANAGER_PATH "/org/freedesktop/systemd1"
#define MANAGER_INTERFACE MANAGER ".Manager"
#define UNIT_INTERFACE MANAGER ".Unit"
#define SERVICE_UNIT "xpilot-infinity-server.service"

/* A session survives cancellation until every asynchronous callback releases
 * its reference. The receiver is detached before controller destruction. */
typedef struct {
    unsigned references;
    bool alive;
    service_receiver receiver;
    uint64_t generation;
    uint64_t revision;
    uint64_t epoch;
    uint64_t query;
    uint64_t operation;
    bool loading;
    GDBusConnection *bus;
    GCancellable *cancel;
    guint watch;
    guint properties;
    guint manager_signals;
    gulong closed;
    guint retry;
    unsigned retry_delay;
    char *owner;
    char *unit_path;
    char *job;
    GHashTable *early_jobs;
} systemd_session;

struct service_native { systemd_session *session; };

typedef struct {
    systemd_session *session;
    uint64_t epoch;
    uint64_t query;
    uint64_t operation;
} systemd_call;

static void refresh(systemd_session *session);
static void bus_ready(GObject *object, GAsyncResult *result, void *data);

static systemd_session *retain(systemd_session *session)
{
    session->references++;
    return session;
}

static void release(void *data)
{
    systemd_session *session = data;
    if (--session->references != 0)
        return;
    g_clear_object(&session->bus);
    g_clear_object(&session->cancel);
    g_free(session->owner);
    g_free(session->unit_path);
    g_free(session->job);
    g_hash_table_unref(session->early_jobs);
    g_free(session);
}

static systemd_call *new_call(systemd_session *session)
{
    systemd_call *call = g_new0(systemd_call, 1);
    call->session = retain(session);
    call->epoch = session->epoch;
    call->query = session->query;
    call->operation = session->operation;
    return call;
}

static void free_call(systemd_call *call)
{
    release(call->session);
    g_free(call);
}

static bool current(const systemd_call *call)
{
    return call->session->alive && call->epoch == call->session->epoch;
}

static service_error classify_error(GError *error)
{
    char *name = g_dbus_error_get_remote_error(error);
    service_error result = XP_SERVICE_ERROR;
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)
        || (name && g_str_has_suffix(name, ".JobCanceled")))
        result = XP_SERVICE_CANCELLED;
    else if (g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED)
             || (name && (g_str_has_suffix(name, ".AccessDenied")
                          || g_str_has_suffix(name, ".InteractiveAuthorizationRequired"))))
        result = XP_SERVICE_FORBIDDEN;
    else if (name && g_str_has_suffix(name, ".UnitMasked"))
        result = XP_SERVICE_DISABLED;
    else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CLOSED)
             || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)
             || g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))
        result = XP_SERVICE_UNAVAILABLE;
    g_free(name);
    return result;
}

static void publish(systemd_session *session, const service_snapshot *snapshot)
{
    if (session->alive)
        session->receiver.observe(session->receiver.context, session->generation,
                                  ++session->revision, snapshot);
}

static void publish_error(systemd_session *session, GError *error)
{
    service_snapshot snapshot = {XP_SERVICE_UNKNOWN, classify_error(error), false, false, ""};
    char *name = g_dbus_error_get_remote_error(error);
    if (name && (g_str_has_suffix(name, ".NoSuchUnit")
                 || g_str_has_suffix(name, ".LoadFailed"))) {
        snapshot.state = XP_SERVICE_NOT_INSTALLED;
        snapshot.error = XP_SERVICE_OK;
    }
    g_free(name);
    g_strlcpy(snapshot.detail, error->message, sizeof(snapshot.detail));
    publish(session, &snapshot);
}

static void operation_result(systemd_session *session, uint64_t operation,
                             service_error error, const char *detail)
{
    if (session->alive)
        session->receiver.result(session->receiver.context, session->generation,
                                 operation, error, detail);
}

static void properties_ready(GObject *object, GAsyncResult *result, void *data)
{
    systemd_call *call = data;
    systemd_session *session = call->session;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call) && call->query == session->query) {
        if (!reply) {
            publish_error(session, error);
        } else {
            GVariant *properties;
            const char *active = "", *sub = "", *load = "", *file_state = "";
            gboolean can_start = FALSE, can_stop = FALSE;
            g_variant_get(reply, "(@a{sv})", &properties);
            g_variant_lookup(properties, "ActiveState", "&s", &active);
            g_variant_lookup(properties, "SubState", "&s", &sub);
            g_variant_lookup(properties, "LoadState", "&s", &load);
            g_variant_lookup(properties, "UnitFileState", "&s", &file_state);
            g_variant_lookup(properties, "CanStart", "b", &can_start);
            g_variant_lookup(properties, "CanStop", "b", &can_stop);
            service_snapshot snapshot = {XP_SERVICE_UNKNOWN, XP_SERVICE_OK, false, false, ""};
            if (strcmp(load, "not-found") == 0)
                snapshot.state = XP_SERVICE_NOT_INSTALLED;
            else if (strcmp(active, "active") == 0 || strcmp(active, "reloading") == 0)
                snapshot.state = XP_SERVICE_RUNNING;
            else if (strcmp(active, "activating") == 0)
                snapshot.state = XP_SERVICE_STARTING;
            else if (strcmp(active, "deactivating") == 0)
                snapshot.state = XP_SERVICE_STOPPING;
            else if (strcmp(active, "failed") == 0)
                snapshot.state = XP_SERVICE_FAILED;
            else if (strcmp(active, "inactive") == 0)
                snapshot.state = XP_SERVICE_STOPPED;
            /* disabled is an autostart setting, unlike masked. */
            snapshot.start_allowed = can_start && strcmp(load, "masked") != 0
                && !g_str_has_prefix(file_state, "masked")
                && strcmp(load, "loaded") == 0;
            snapshot.stop_allowed = can_stop;
            snprintf(snapshot.detail, sizeof(snapshot.detail), "%s/%s; %s; %s",
                     active, sub, load, file_state);
            publish(session, &snapshot);
            g_variant_unref(properties);
        }
    }
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void load_ready(GObject *object, GAsyncResult *result, void *data)
{
    systemd_call *call = data;
    systemd_session *session = call->session;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call) && call->query == session->query) {
        session->loading = false;
        if (!reply) {
            publish_error(session, error);
        } else {
            g_free(session->unit_path);
            g_variant_get(reply, "(o)", &session->unit_path);
            g_dbus_connection_call(session->bus, session->owner, session->unit_path,
                "org.freedesktop.DBus.Properties", "GetAll",
                g_variant_new("(s)", UNIT_INTERFACE), G_VARIANT_TYPE("(a{sv})"),
                G_DBUS_CALL_FLAGS_NONE, -1, session->cancel, properties_ready, new_call(session));
        }
    }
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void refresh(systemd_session *session)
{
    if (!session->alive || !session->owner || session->loading)
        return;
    session->query++;
    if (session->unit_path) {
        g_dbus_connection_call(session->bus, session->owner, session->unit_path,
            "org.freedesktop.DBus.Properties", "GetAll",
            g_variant_new("(s)", UNIT_INTERFACE), G_VARIANT_TYPE("(a{sv})"),
            G_DBUS_CALL_FLAGS_NONE, -1, session->cancel, properties_ready, new_call(session));
        return;
    }
    session->loading = true;
    g_dbus_connection_call(session->bus, session->owner, MANAGER_PATH,
        MANAGER_INTERFACE, "LoadUnit", g_variant_new("(s)", SERVICE_UNIT),
        G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE, -1, session->cancel,
        load_ready, new_call(session));
}

static void job_result(systemd_session *session, const char *outcome)
{
    if (strcmp(outcome, "done") != 0)
        operation_result(session, session->operation,
                         strcmp(outcome, "canceled") == 0 ? XP_SERVICE_CANCELLED : XP_SERVICE_ERROR,
                         outcome);
    refresh(session);
}

static void signal_received(GDBusConnection *bus, const char *sender,
                            const char *path, const char *interface,
                            const char *signal, GVariant *parameters, void *data)
{
    systemd_session *session = data;
    (void)bus;
    if (!session->alive || !session->owner || strcmp(sender, session->owner) != 0)
        return;
    if (strcmp(interface, "org.freedesktop.DBus.Properties") == 0) {
        if (session->unit_path && strcmp(path, session->unit_path) == 0)
            refresh(session);
    } else if (strcmp(signal, "JobRemoved") == 0) {
        guint id;
        const char *job, *unit, *outcome;
        g_variant_get(parameters, "(u&o&s&s)", &id, &job, &unit, &outcome);
        if (strcmp(unit, SERVICE_UNIT) != 0)
            return;
        if (session->job && strcmp(session->job, job) == 0)
            job_result(session, outcome);
        else if (session->operation && !session->job)
            g_hash_table_replace(session->early_jobs, g_strdup(job), g_strdup(outcome));
        refresh(session);
    } else if (strcmp(signal, "UnitNew") == 0) {
        const char *unit, *unit_path;
        g_variant_get(parameters, "(&s&o)", &unit, &unit_path);
        /* An inactive unit may be garbage-collected and reloaded by GetAll.
         * UnitNew alone is not a change of service state. */
        if (strcmp(unit, SERVICE_UNIT) == 0 && !session->unit_path)
            refresh(session);
    } else if (strcmp(signal, "UnitFilesChanged") == 0 || strcmp(signal, "Reloading") == 0) {
        refresh(session);
    }
}

static void subscribed(GObject *object, GAsyncResult *result, void *data)
{
    systemd_call *call = data;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call)) {
        char *name = error ? g_dbus_error_get_remote_error(error) : NULL;
        if (reply || (name && g_str_has_suffix(name, ".AlreadySubscribed")))
            refresh(call->session);
        else
            publish_error(call->session, error);
        g_free(name);
    }
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void appeared(GDBusConnection *bus, const char *name, const char *owner, void *data)
{
    systemd_session *session = data;
    (void)name;
    session->epoch++;
    session->loading = false;
    g_clear_pointer(&session->unit_path, g_free);
    g_free(session->owner);
    session->owner = g_strdup(owner);
    g_dbus_connection_call(bus, owner, MANAGER_PATH, MANAGER_INTERFACE,
        "Subscribe", NULL, G_VARIANT_TYPE_UNIT, G_DBUS_CALL_FLAGS_NONE,
        -1, session->cancel, subscribed, new_call(session));
}

static void vanished(GDBusConnection *bus, const char *name, void *data)
{
    systemd_session *session = data;
    (void)bus;
    (void)name;
    session->epoch++;
    g_clear_pointer(&session->owner, g_free);
    service_snapshot snapshot = {XP_SERVICE_UNKNOWN, XP_SERVICE_UNAVAILABLE, false, false,
                                 "The system service manager is unavailable"};
    publish(session, &snapshot);
}

static void detach_bus(systemd_session *session)
{
    if (session->watch) g_bus_unwatch_name(session->watch);
    if (session->properties) g_dbus_connection_signal_unsubscribe(session->bus, session->properties);
    if (session->manager_signals) g_dbus_connection_signal_unsubscribe(session->bus, session->manager_signals);
    if (session->closed) g_signal_handler_disconnect(session->bus, session->closed);
    session->watch = session->properties = session->manager_signals = 0;
    session->closed = 0;
    session->loading = false;
    g_clear_object(&session->bus);
    g_clear_pointer(&session->owner, g_free);
    g_clear_pointer(&session->unit_path, g_free);
    g_clear_pointer(&session->job, g_free);
    g_hash_table_remove_all(session->early_jobs);
}

static gboolean reconnect_bus(void *data)
{
    systemd_session *session = data;
    session->retry = 0;
    if (session->alive) g_bus_get(G_BUS_TYPE_SYSTEM, session->cancel, bus_ready, retain(session));
    return G_SOURCE_REMOVE;
}

static void retry_bus(systemd_session *session)
{
    if (!session->alive || session->retry) return;
    /* Back off only failed connections; service state is always event driven. */
    session->retry_delay = MIN(session->retry_delay ? session->retry_delay * 2 : 250, 30000);
    session->retry = g_timeout_add_full(G_PRIORITY_DEFAULT, session->retry_delay,
        reconnect_bus, retain(session), release);
}

static void bus_closed(GDBusConnection *bus, gboolean remote, GError *error, void *data)
{
    (void)remote;
    (void)error;
    systemd_session *session = data;
    vanished(bus, MANAGER, session);
    detach_bus(session);
    retry_bus(session);
}

static void bus_ready(GObject *object, GAsyncResult *result, void *data)
{
    systemd_session *session = data;
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_finish(result, &error);
    (void)object;
    if (session->alive) {
        if (!bus) {
            publish_error(session, error);
            retry_bus(session);
        } else {
            session->retry_delay = 0;
            session->bus = g_object_ref(bus);
            g_dbus_connection_set_exit_on_close(bus, FALSE);
            session->closed = g_signal_connect(bus, "closed", G_CALLBACK(bus_closed), session);
            session->properties = g_dbus_connection_signal_subscribe(bus, MANAGER,
                "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL, NULL,
                G_DBUS_SIGNAL_FLAGS_NONE, signal_received, retain(session), release);
            session->manager_signals = g_dbus_connection_signal_subscribe(bus, MANAGER,
                MANAGER_INTERFACE, NULL, MANAGER_PATH, NULL,
                G_DBUS_SIGNAL_FLAGS_NONE, signal_received, retain(session), release);
            session->watch = g_bus_watch_name_on_connection(bus, MANAGER,
                G_BUS_NAME_WATCHER_FLAGS_NONE, appeared, vanished, retain(session), release);
        }
    }
    g_clear_object(&bus);
    g_clear_error(&error);
    release(session);
}

static void disconnect_service(void *context)
{
    service_native *native = context;
    systemd_session *session = native->session;
    if (!session)
        return;
    native->session = NULL;
    session->alive = false;
    g_cancellable_cancel(session->cancel);
    if (session->retry) { g_source_remove(session->retry); session->retry = 0; }
    detach_bus(session);
    release(session);
}

static void connect_service(void *context, uint64_t generation, service_receiver receiver)
{
    service_native *native = context;
    disconnect_service(native);
    systemd_session *session = g_new0(systemd_session, 1);
    session->references = 1;
    session->alive = true;
    session->generation = generation;
    session->receiver = receiver;
    session->cancel = g_cancellable_new();
    session->early_jobs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    native->session = session;
    g_bus_get(G_BUS_TYPE_SYSTEM, session->cancel, bus_ready, retain(session));
}

static void requested(GObject *object, GAsyncResult *result, void *data)
{
    systemd_call *call = data;
    systemd_session *session = call->session;
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
    if (current(call) && call->operation == session->operation) {
        if (!reply) {
            operation_result(session, call->operation, classify_error(error), error->message);
        } else {
            g_free(session->job);
            g_variant_get(reply, "(o)", &session->job);
            operation_result(session, call->operation, XP_SERVICE_OK, "Service request accepted");
            const char *early = g_hash_table_lookup(session->early_jobs, session->job);
            if (early)
                job_result(session, early);
        }
        g_hash_table_remove_all(session->early_jobs);
        refresh(session);
    }
    g_clear_pointer(&reply, g_variant_unref);
    g_clear_error(&error);
    free_call(call);
}

static void request_service(void *context, uint64_t operation, service_action action)
{
    service_native *native = context;
    systemd_session *session = native->session;
    if (!session || !session->owner) {
        if (session)
            operation_result(session, operation, XP_SERVICE_UNAVAILABLE, "Service manager unavailable");
        return;
    }
    session->operation = operation;
    g_clear_pointer(&session->job, g_free);
    g_hash_table_remove_all(session->early_jobs);
    g_dbus_connection_call(session->bus, session->owner, MANAGER_PATH,
        MANAGER_INTERFACE, action == XP_SERVICE_START ? "StartUnit" : "StopUnit",
        g_variant_new("(ss)", SERVICE_UNIT, "fail"), G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, G_MAXINT,
        session->cancel, requested, new_call(session));
}

service_native *service_native_create(void)
{
    return g_new0(service_native, 1);
}

void service_native_enable_authorization(service_native *native)
{
    (void)native;
}

service_control service_native_control(service_native *native)
{
    service_control control = {native, connect_service, request_service, disconnect_service};
    return control;
}

static gboolean wake_dispatch(void *data)
{
    (void)data;
    return G_SOURCE_REMOVE;
}

void service_native_dispatch(service_native *native, unsigned timeout_ms)
{
    (void)native;
    if (!timeout_ms) {
        while (g_main_context_iteration(NULL, FALSE)) {}
        return;
    }
    GSource *deadline = g_timeout_source_new(timeout_ms);
    g_source_set_callback(deadline, wake_dispatch, NULL, NULL);
    g_source_attach(deadline, NULL);
    g_main_context_iteration(NULL, TRUE);
    g_source_destroy(deadline);
    g_source_unref(deadline);
}

void service_native_destroy(service_native *native)
{
    if (!native)
        return;
    disconnect_service(native);
    g_free(native);
}
