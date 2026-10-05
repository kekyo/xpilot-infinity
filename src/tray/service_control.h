/* XPilot Infinity service management. */
#ifndef XPILOT_SERVICE_CONTROL_H
#define XPILOT_SERVICE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/** State reported by the operating system, not game readiness. */
typedef enum {
    XP_SERVICE_UNKNOWN, /**< The manager has not supplied a state. */
    XP_SERVICE_NOT_INSTALLED, /**< The fixed product service is absent. */
    XP_SERVICE_STOPPED, /**< The service is inactive. */
    XP_SERVICE_STARTING, /**< Startup or automatic recovery is pending. */
    XP_SERVICE_RUNNING, /**< The manager reports an active service. */
    XP_SERVICE_STOPPING, /**< Normal shutdown is pending. */
    XP_SERVICE_FAILED /**< The manager reports a failure. */
} service_state;

/** Reason an operation or observation could not be completed. */
typedef enum {
    XP_SERVICE_OK, /**< No error. */
    XP_SERVICE_UNAVAILABLE, /**< The manager cannot be reached. */
    XP_SERVICE_FORBIDDEN, /**< The caller lacks permission. */
    XP_SERVICE_CANCELLED, /**< Authorization or the job was cancelled. */
    XP_SERVICE_DISABLED, /**< Manual start is prohibited. */
    XP_SERVICE_ERROR /**< An operating system operation failed. */
} service_error;

/** Operations accepted by the fixed product service backend. */
typedef enum {
    XP_SERVICE_START, /**< Start using the registered configuration. */
    XP_SERVICE_STOP /**< Request normal shutdown. */
} service_action;

/** A value snapshot; it owns no resources. */
typedef struct {
    service_state state; /**< Last observed operating system state. */
    service_error error; /**< Observation error, if any. */
    bool start_allowed; /**< Manual start is permitted by configuration. */
    bool stop_allowed; /**< Normal stop is supported by configuration. */
    char detail[256]; /**< UTF-8 status/error description, always terminated. */
} service_snapshot;

/** Event receiver. All calls run on the owning event-loop thread. */
typedef struct {
    void *context; /**< Borrowed callback context. */
    /** Supply a state, with monotonically increasing revision per connection.
     * @param context Borrowed receiver context.
     * @param generation Connection identifier supplied to connect.
     * @param revision Observation order, starting at one.
     * @param snapshot Borrowed value, copied by the receiver.
     */
    void (*observe)(void *context, uint64_t generation, uint64_t revision,
                    const service_snapshot *snapshot);
    /** Report request acceptance or failure; acceptance is not completion.
     * @param context Borrowed receiver context.
     * @param generation Connection identifier.
     * @param operation Request identifier.
     * @param error XP_SERVICE_OK for acceptance, or an operation failure.
     * @param detail Borrowed UTF-8 description.
     */
    void (*result)(void *context, uint64_t generation, uint64_t operation,
                   service_error error, const char *detail);
} service_receiver;

/** Backend function table. The caller owns and destroys the backend. */
typedef struct {
    void *context; /**< Backend instance. */
    /** Subscribe before reading current state. Does not start the service.
     * @param context Backend instance.
     * @param generation New connection identifier.
     * @param receiver Event receiver, copied by the backend.
     */
    void (*connect)(void *context, uint64_t generation,
                    service_receiver receiver);
    /** Request one operation on the fixed service.
     * @param context Backend instance.
     * @param operation Unique request identifier.
     * @param action Normal start or stop.
     */
    void (*request)(void *context, uint64_t operation, service_action action);
    /** Detach callbacks and release subscriptions without stopping the service.
     * @param context Backend instance. May be reused for a later connection.
     */
    void (*disconnect)(void *context);
} service_control;

#endif
