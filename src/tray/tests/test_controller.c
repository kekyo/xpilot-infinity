#include "tray_controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    service_receiver receiver;
    uint64_t generation;
    uint64_t revision;
    uint64_t operation;
    unsigned requests;
    unsigned disconnects;
    service_action action;
} test_service;

static void connect_service(void *context, uint64_t generation,
                            service_receiver receiver)
{
    test_service *service = context;
    service->generation = generation;
    service->revision = 0;
    service->receiver = receiver;
}

static void request_service(void *context, uint64_t operation,
                            service_action action)
{
    test_service *service = context;
    service->operation = operation;
    service->action = action;
    service->requests++;
}

static void disconnect_service(void *context)
{
    test_service *service = context;
    service->disconnects++;
}

static void observe(test_service *service, service_state state,
                    service_error error, bool start_allowed)
{
    service_snapshot snapshot = {state, error, start_allowed, true, ""};
    service->receiver.observe(service->receiver.context, service->generation,
                              ++service->revision, &snapshot);
}

static void result(test_service *service, service_error error)
{
    service->receiver.result(service->receiver.context, service->generation,
                             service->operation, error, "request result");
}

int main(void)
{
    test_service service = {0};
    service_control backend = {
        &service, connect_service, request_service, disconnect_service
    };
    tray_controller *controller = tray_controller_create(backend);
    assert(controller != NULL);
    assert(tray_controller_status(controller)->service.state == XP_SERVICE_UNKNOWN);
    tray_controller_connect(controller);
    assert(service.requests == 0);
    assert(service.receiver.observe != NULL);
    assert(!tray_controller_request(controller, XP_SERVICE_START));

    /* Attaching to a running service is read-only. External changes win. */
    observe(&service, XP_SERVICE_RUNNING, XP_SERVICE_OK, true);
    assert(tray_controller_status(controller)->can_stop);
    assert(!tray_controller_status(controller)->can_start);
    assert(service.requests == 0);
    observe(&service, XP_SERVICE_STOPPED, XP_SERVICE_OK, true);
    assert(tray_controller_request(controller, XP_SERVICE_START));
    assert(service.requests == 1 && service.action == XP_SERVICE_START);
    assert(!tray_controller_request(controller, XP_SERVICE_START));
    result(&service, XP_SERVICE_OK);
    assert(tray_controller_status(controller)->busy);
    assert(tray_controller_status(controller)->service.state == XP_SERVICE_STOPPED);
    observe(&service, XP_SERVICE_STARTING, XP_SERVICE_OK, true);
    assert(!tray_controller_request(controller, XP_SERVICE_STOP));
    observe(&service, XP_SERVICE_RUNNING, XP_SERVICE_OK, true);
    assert(!tray_controller_status(controller)->busy);

    assert(tray_controller_request(controller, XP_SERVICE_STOP));
    observe(&service, XP_SERVICE_STOPPING, XP_SERVICE_OK, true);
    result(&service, XP_SERVICE_OK);
    assert(tray_controller_status(controller)->busy);
    assert(!tray_controller_request(controller, XP_SERVICE_START));
    observe(&service, XP_SERVICE_STOPPED, XP_SERVICE_OK, true);
    assert(!tray_controller_status(controller)->busy);

    /* A notification can arrive before the method reply. */
    assert(tray_controller_request(controller, XP_SERVICE_START));
    observe(&service, XP_SERVICE_RUNNING, XP_SERVICE_OK, true);
    assert(tray_controller_status(controller)->busy);
    result(&service, XP_SERVICE_OK);
    assert(!tray_controller_status(controller)->busy);

    observe(&service, XP_SERVICE_STOPPED, XP_SERVICE_OK, false);
    assert(!tray_controller_request(controller, XP_SERVICE_START));
    observe(&service, XP_SERVICE_NOT_INSTALLED, XP_SERVICE_OK, true);
    assert(!tray_controller_request(controller, XP_SERVICE_START));
    observe(&service, XP_SERVICE_UNKNOWN, XP_SERVICE_UNAVAILABLE, false);
    assert(!tray_controller_status(controller)->can_stop);

    observe(&service, XP_SERVICE_STOPPED, XP_SERVICE_OK, true);
    assert(tray_controller_request(controller, XP_SERVICE_START));
    result(&service, XP_SERVICE_FORBIDDEN);
    assert(!tray_controller_status(controller)->busy);
    assert(tray_controller_status(controller)->operation_error == XP_SERVICE_FORBIDDEN);
    assert(tray_controller_status(controller)->service.state == XP_SERVICE_STOPPED);
    assert(tray_controller_request(controller, XP_SERVICE_START));
    result(&service, XP_SERVICE_CANCELLED);
    assert(tray_controller_status(controller)->operation_error == XP_SERVICE_CANCELLED);
    assert(tray_controller_request(controller, XP_SERVICE_START));
    result(&service, XP_SERVICE_OK);
    observe(&service, XP_SERVICE_FAILED, XP_SERVICE_OK, true);
    assert(!tray_controller_status(controller)->busy);
    assert(tray_controller_status(controller)->operation_error == XP_SERVICE_ERROR);

    /* Reconnection invalidates both delayed observations and old requests. */
    uint64_t old_generation = service.generation;
    uint64_t old_operation = service.operation;
    tray_controller_connect(controller);
    observe(&service, XP_SERVICE_RUNNING, XP_SERVICE_OK, true);
    service_snapshot stale = {XP_SERVICE_STOPPED, XP_SERVICE_OK, true, true, "stale"};
    service.receiver.observe(service.receiver.context, old_generation, 999, &stale);
    service.receiver.observe(service.receiver.context, service.generation, 0, &stale);
    assert(tray_controller_status(controller)->service.state == XP_SERVICE_RUNNING);
    assert(tray_controller_request(controller, XP_SERVICE_STOP));
    service.receiver.result(service.receiver.context, old_generation,
                            old_operation, XP_SERVICE_ERROR, "stale");
    service.receiver.result(service.receiver.context, service.generation,
                            old_operation, XP_SERVICE_ERROR, "stale");
    assert(tray_controller_status(controller)->busy);
    observe(&service, XP_SERVICE_UNKNOWN, XP_SERVICE_UNAVAILABLE, false);
    assert(!tray_controller_status(controller)->busy);
    assert(tray_controller_status(controller)->operation_error == XP_SERVICE_UNAVAILABLE);
    unsigned requests = service.requests;
    tray_controller_destroy(controller);
    assert(service.requests == requests);
    assert(service.disconnects == 2);
    puts("service controller: all behavioral checks passed");
    return 0;
}
