#include "tray_menu.h"
#include <stdio.h>
#include <string.h>

void tray_menu_update(tray_menu *menu, const tray_status *status, const char *configured_map)
{
    const char *states[] = {"Checking service", "Service not installed", "Service stopped",
        "Service starting", "Service running", "Service stopping", "Service failed"};
    tray_menu_item items[] = {
        {TRAY_STATUS, false, ""}, {TRAY_MAP, false, ""},
        {TRAY_SEPARATOR, false, ""}, {TRAY_START, status->can_start, "Start server"},
        {TRAY_STOP, status->can_stop, "Stop server"},
        {TRAY_DETAILS, true, "Error details / log location"},
        {TRAY_QUIT, true, "Quit tray (server keeps running)"}
    };
    const char *state = status->service.error == XP_SERVICE_UNAVAILABLE
        ? "Service manager unavailable" : states[status->service.state];
    const char *operation = "";
    if (status->busy) operation = " (operation pending)";
    else if (status->operation_error == XP_SERVICE_FORBIDDEN)
#ifdef _WIN32
        operation = " (operation not authorized)";
#else
        /* systemd does not expose polkit's dismissal detail for manage-units. */
        operation = " (operation not authorized: denied or canceled)";
#endif
    else if (status->operation_error == XP_SERVICE_CANCELLED) operation = " (operation canceled)";
    else if (status->operation_error != XP_SERVICE_OK) operation = " (operation failed; see details)";
    snprintf(items[0].label, sizeof(items[0].label), "XPilot Infinity: %s%s",
             state, operation);
    snprintf(items[1].label, sizeof(items[1].label), "Configured map: %s", configured_map);
    bool changed = false;
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        if (menu->items[i].id != items[i].id || menu->items[i].enabled != items[i].enabled
            || strcmp(menu->items[i].label, items[i].label)) {
            menu->items[i] = items[i];
            changed = true;
        }
    }
    if (changed)
        menu->revision++;
}

const tray_menu_item *tray_menu_find(const tray_menu *menu, int id)
{
    for (size_t i = 0; i < sizeof(menu->items) / sizeof(menu->items[0]); i++)
        if (menu->items[i].id == id)
            return &menu->items[i];
    return NULL;
}
