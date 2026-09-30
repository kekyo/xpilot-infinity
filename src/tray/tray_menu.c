#include "tray_menu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

bool tray_menu_set_maps(tray_menu *menu, const map_catalog *catalog, const char *selected,
                        bool enabled, bool running)
{
    tray_menu_item *maps = catalog->count ? calloc(catalog->count, sizeof(*maps)) : NULL;
    if (catalog->count && !maps) return false;
    if (!menu->next_map_id) menu->next_map_id = 100;
    bool changed = menu->map_count != catalog->count;
    for (size_t i = 0; i < catalog->count; i++) {
        for (size_t j = 0; j < menu->map_count; j++)
            if (!strcmp(menu->maps[j].label, catalog->names[i])) maps[i].id = menu->maps[j].id;
        if (!maps[i].id) {
            if (menu->next_map_id == INT_MAX) { free(maps); return false; }
            maps[i].id = menu->next_map_id++;
        }
        maps[i].enabled = enabled;
        maps[i].radio = true;
        maps[i].checked = selected && !strcmp(selected, catalog->names[i]);
        snprintf(maps[i].label, sizeof(maps[i].label), "%s", catalog->names[i]);
        if (i >= menu->map_count || maps[i].id != menu->maps[i].id
            || maps[i].enabled != menu->maps[i].enabled || maps[i].checked != menu->maps[i].checked)
            changed = true;
    }
    free(menu->maps); menu->maps = maps; menu->map_count = catalog->count;
    tray_menu_item *parent = &menu->items[7];
    const char *label = running ? "Maps (changing map restarts the service)" : "Maps";
    if (parent->id != TRAY_MAP_MENU || parent->enabled != (enabled && catalog->count > 0)
        || strcmp(parent->label, label)) changed = true;
    parent->id = TRAY_MAP_MENU;
    parent->enabled = enabled && catalog->count > 0;
    snprintf(parent->label, sizeof(parent->label), "%s", label);
    if (changed) menu->revision++;
    return true;
}

void tray_menu_clear(tray_menu *menu)
{
    free(menu->maps);
    memset(menu, 0, sizeof(*menu));
}

void tray_menu_update(tray_menu *menu, const tray_status *status, const char *configured_map)
{
    const char *states[] = {"Checking service", "Service not installed", "Service stopped",
        "Service starting", "Service running", "Service stopping", "Service failed"};
    tray_menu_item items[] = {
        {TRAY_STATUS, false, "", false, false}, {TRAY_MAP, false, "", false, false},
        {TRAY_SEPARATOR, false, "", false, false}, {TRAY_START, status->can_start, "Start server", false, false},
        {TRAY_STOP, status->can_stop, "Stop server", false, false},
        menu->items[5], menu->items[6], menu->items[7], menu->items[8],
        {TRAY_DETAILS, true, "Error details / log location", false, false},
        {TRAY_QUIT, true, "Quit tray (server keeps running)", false, false}
    };
    if (!items[5].id) items[5] = (tray_menu_item){TRAY_EDIT, false, "Edit configuration copy...", false, false};
    if (!items[6].id) items[6] = (tray_menu_item){TRAY_APPLY, false, "Apply saved changes...", false, false};
    if (!items[7].id) items[7] = (tray_menu_item){TRAY_MAP_MENU, false, "Maps", false, false};
    if (!items[8].id) items[8] = (tray_menu_item){TRAY_DISCARD, false, "Discard editing copy...", false, false};
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
    for (size_t i = 0; i < menu->map_count; i++)
        if (menu->maps[i].id == id)
            return &menu->maps[i];
    return NULL;
}

void tray_menu_set_editor(tray_menu *menu, bool available, editor_state state, bool busy, bool running)
{
    bool copy = state != EDITOR_NONE;
    const char *apply = state == EDITOR_CONFLICT ? "Cannot apply: shared configuration changed"
        : state == EDITOR_INVALID ? "Cannot apply: saved copy is invalid"
        : state != EDITOR_CHANGED ? "Apply saved changes (no saved changes)"
        : running ? "Apply saved changes (restarts service)..." : "Apply saved changes...";
    tray_menu_item items[] = {
        {TRAY_EDIT, !busy && (available || copy), "", false, false},
        {TRAY_APPLY, !busy && available && state == EDITOR_CHANGED, "", false, false},
        {TRAY_DISCARD, !busy && copy, "Discard editing copy...", false, false}
    };
    snprintf(items[0].label, sizeof(items[0].label), "%s", copy ? "Resume configuration copy..." : "Edit configuration copy...");
    snprintf(items[1].label, sizeof(items[1].label), "%s", apply);
    size_t positions[] = {5, 6, 8};
    bool changed = false;
    for (size_t i = 0; i < 3; i++) {
        tray_menu_item *row = &menu->items[positions[i]];
        if (row->id != items[i].id || row->enabled != items[i].enabled || strcmp(row->label, items[i].label)) {
            *row = items[i]; changed = true;
        }
    }
    if (changed) menu->revision++;
}
