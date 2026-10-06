#include "tray_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tray_controller {
    tray_status status;
    service_control backend;
    uint64_t generation;
    uint64_t revision;
    uint64_t operation;
    uint64_t request_revision;
    service_action action;
    bool accepted;
    bool connected;
};

static void update_actions(tray_controller *controller)
{
    tray_status *status = &controller->status;
    bool available = !status->busy && status->service.error == XP_SERVICE_OK;
    status->can_start = available && status->service.start_allowed
        && (status->service.state == XP_SERVICE_STOPPED
            || status->service.state == XP_SERVICE_FAILED);
    status->can_stop = available && status->service.stop_allowed
        && status->service.state == XP_SERVICE_RUNNING;
}

static void settle(tray_controller *controller)
{
    tray_status *status = &controller->status;
    if (status->busy && controller->accepted
        && controller->revision > controller->request_revision
        && ((controller->action == XP_SERVICE_START
             && status->service.state == XP_SERVICE_RUNNING)
            || (controller->action == XP_SERVICE_STOP
                && status->service.state == XP_SERVICE_STOPPED)))
        status->busy = false;
    update_actions(controller);
}

static void observe(void *context, uint64_t generation, uint64_t revision,
                    const service_snapshot *snapshot)
{
    tray_controller *controller = context;
    if (generation != controller->generation || revision <= controller->revision)
        return;
    controller->revision = revision;
    controller->status.service = *snapshot;
    controller->status.service.detail[sizeof(snapshot->detail) - 1] = '\0';
    if (controller->status.busy
        && (snapshot->error != XP_SERVICE_OK || snapshot->state == XP_SERVICE_FAILED
            || snapshot->state == XP_SERVICE_NOT_INSTALLED)) {
        controller->status.busy = false;
        controller->status.operation_error = snapshot->error != XP_SERVICE_OK
            ? snapshot->error : XP_SERVICE_ERROR;
        snprintf(controller->status.operation_detail,
                 sizeof(controller->status.operation_detail), "%s", snapshot->detail);
    }
    settle(controller);
}

static void result(void *context, uint64_t generation, uint64_t operation,
                   service_error error, const char *detail)
{
    tray_controller *controller = context;
    if (generation != controller->generation || operation != controller->operation
        || !controller->status.busy)
        return;
    if (error != XP_SERVICE_OK) {
        controller->status.busy = false;
        controller->status.operation_error = error;
        snprintf(controller->status.operation_detail,
                 sizeof(controller->status.operation_detail), "%s", detail);
    } else {
        controller->accepted = true;
    }
    settle(controller);
}

tray_controller *tray_controller_create(service_control backend)
{
    tray_controller *controller = calloc(1, sizeof(*controller));
    if (controller != NULL)
        controller->backend = backend;
    return controller;
}

void tray_controller_connect(tray_controller *controller)
{
    controller->generation++;
    if (controller->connected)
        controller->backend.disconnect(controller->backend.context);
    memset(&controller->status, 0, sizeof(controller->status));
    controller->revision = 0;
    controller->connected = true;
    service_receiver receiver = {controller, observe, result};
    controller->backend.connect(controller->backend.context,
                                controller->generation, receiver);
}

const tray_status *tray_controller_status(const tray_controller *controller)
{
    return &controller->status;
}

bool tray_controller_request(tray_controller *controller, service_action action)
{
    if ((action == XP_SERVICE_START && !controller->status.can_start)
        || (action == XP_SERVICE_STOP && !controller->status.can_stop)
        || (action != XP_SERVICE_START && action != XP_SERVICE_STOP))
        return false;
    controller->status.busy = true;
    controller->status.operation_error = XP_SERVICE_OK;
    controller->status.operation_detail[0] = '\0';
    controller->accepted = false;
    controller->action = action;
    controller->request_revision = controller->revision;
    controller->operation++;
    update_actions(controller);
    controller->backend.request(controller->backend.context,
                                controller->operation, action);
    return true;
}

void tray_controller_destroy(tray_controller *controller)
{
    if (controller == NULL)
        return;
    if (controller->connected)
        controller->backend.disconnect(controller->backend.context);
    free(controller);
}
