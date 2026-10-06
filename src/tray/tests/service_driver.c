#ifdef _WIN32
#define _WIN32_WINNT 0x0600
#endif
#include "service_native.h"
#include "tray_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static uint64_t now_ms(void) { return GetTickCount64(); }
#else
#include <gio/gio.h>
static uint64_t now_ms(void) { return (uint64_t)(g_get_monotonic_time() / 1000); }
#endif

static const char *state_name(service_state state)
{
    const char *names[] = {"unknown", "not-installed", "stopped", "starting",
                           "running", "stopping", "failed"};
    return names[state];
}

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "status") && strcmp(argv[1], "start")
        && strcmp(argv[1], "stop") && strcmp(argv[1], "watch")
        && strcmp(argv[1], "wait-running") && strcmp(argv[1], "wait-stopped")
#ifndef _WIN32
        && strcmp(argv[1], "reconnect")
#endif
        )) {
        fprintf(stderr, "Usage: %s status|start|stop|watch|wait-running|wait-stopped|reconnect\n", argv[0]);
        return 2;
    }
    service_native *native = service_native_create();
    if (!native)
        return 1;
    tray_controller *controller = tray_controller_create(service_native_control(native));
    if (!controller) {
        service_native_destroy(native);
        return 1;
    }
    tray_controller_connect(controller);
    bool start = strcmp(argv[1], "start") == 0;
    bool stop = strcmp(argv[1], "stop") == 0;
    bool submitted = false;
#ifndef _WIN32
    bool reconnect = !strcmp(argv[1], "reconnect"), disconnected = false;
    service_state expected = XP_SERVICE_UNKNOWN;
#endif
    bool printed = false;
    tray_status previous = {0};
    unsigned timeout = 120000;
    const char *setting = getenv("XPILOT_SERVICE_TEST_TIMEOUT_MS");
    if (setting && strtoul(setting, NULL, 10) > 0)
        timeout = (unsigned)strtoul(setting, NULL, 10);
    uint64_t deadline = now_ms() + timeout;
    int exit_code = 1;
    while (now_ms() < deadline) {
        const tray_status *status = tray_controller_status(controller);
        if (!printed || memcmp(status, &previous, sizeof(previous))) {
            printf("%s busy=%d start=%d stop=%d error=%d operation-error=%d %s %s\n",
                   state_name(status->service.state), status->busy,
                   status->can_start, status->can_stop, status->service.error,
                   status->operation_error, status->service.detail, status->operation_detail);
            fflush(stdout);
            previous = *status;
            printed = true;
        }
#ifndef _WIN32
        if (reconnect) {
            if (status->service.error == XP_SERVICE_UNAVAILABLE) disconnected = true;
            if (disconnected && status->service.state == expected) { exit_code = 0; break; }
            if (!submitted && status->service.state != XP_SERVICE_UNKNOWN) {
                expected = status->service.state;
                GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
                if (!bus) break;
                g_dbus_connection_close_sync(bus, NULL, NULL);
                g_object_unref(bus);
                submitted = true;
            }
            service_native_dispatch(native, 1000);
            continue;
        }
#endif
        if (status->service.error != XP_SERVICE_OK || status->operation_error != XP_SERVICE_OK)
            break;
        if (status->service.state != XP_SERVICE_UNKNOWN) {
            if (!strcmp(argv[1], "status")) {
                exit_code = 0;
                break;
            }
            service_state target = start || !strcmp(argv[1], "wait-running")
                ? XP_SERVICE_RUNNING : XP_SERVICE_STOPPED;
            if (strcmp(argv[1], "watch") && !status->busy && status->service.state == target) {
                exit_code = 0;
                break;
            }
            if ((start || stop) && !submitted) {
                if (!tray_controller_request(controller, start ? XP_SERVICE_START : XP_SERVICE_STOP))
                    break;
                submitted = true;
                continue;
            }
        }
        uint64_t now = now_ms();
        if (now >= deadline)
            break;
        uint64_t remaining = deadline - now;
        service_native_dispatch(native, remaining > 60000 ? 60000 : (unsigned)remaining);
    }
    if (now_ms() >= deadline)
        fprintf(stderr, "Timed out waiting for an observed service state\n");
    tray_controller_destroy(controller);
    service_native_destroy(native);
    return exit_code;
}
