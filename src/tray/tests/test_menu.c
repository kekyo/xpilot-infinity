#include "tray_menu.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    tray_menu menu = {0};
    tray_status status = {0};
    tray_menu_update(&menu, &status, "a_&_map.xp2");
    assert(tray_menu_find(&menu, TRAY_EDIT) != NULL);
    assert(tray_menu_find(&menu, TRAY_APPLY) != NULL);
    assert(tray_menu_find(&menu, TRAY_START) != NULL);
    assert(!tray_menu_find(&menu, TRAY_START)->enabled);
    assert(tray_menu_find(&menu, TRAY_QUIT)->enabled);
    assert(strstr(tray_menu_find(&menu, TRAY_MAP)->label, "a_&_map.xp2"));
    unsigned revision = menu.revision;
    tray_menu_update(&menu, &status, "a_&_map.xp2");
    assert(menu.revision == revision);
    status.service.state = XP_SERVICE_RUNNING;
    status.can_stop = true;
    tray_menu_update(&menu, &status, "ndh.xp2");
    assert(menu.revision != revision);
    assert(tray_menu_find(&menu, TRAY_STOP)->enabled);
    assert(!tray_menu_find(&menu, TRAY_START)->enabled);
    assert(strstr(tray_menu_find(&menu, TRAY_STATUS)->label, "Service running"));
    status.busy = true;
    status.can_stop = false;
    tray_menu_update(&menu, &status, "ndh.xp2");
    assert(!tray_menu_find(&menu, TRAY_STOP)->enabled);
    assert(tray_menu_find(&menu, TRAY_QUIT)->enabled);
    assert(tray_menu_find(&menu, 999) == NULL);
    status.busy = false;
    status.operation_error = XP_SERVICE_FORBIDDEN;
    tray_menu_update(&menu, &status, "ndh.xp2");
    assert(strstr(tray_menu_find(&menu, TRAY_STATUS)->label, "not authorized"));
#ifndef _WIN32
    assert(strstr(tray_menu_find(&menu, TRAY_STATUS)->label, "denied or canceled"));
#endif
    char *names[] = {"first.xp2", "second.xp2"};
    map_catalog maps = {names, 2};
    assert(tray_menu_set_maps(&menu, &maps, "second.xp2", true, true));
    assert(menu.map_count == 2 && menu.maps[1].checked && !menu.maps[0].checked);
    assert(strstr(tray_menu_find(&menu, TRAY_MAP_MENU)->label, "restarts"));
    int first_id = menu.maps[0].id, second_id = menu.maps[1].id;
    assert(tray_menu_find(&menu, second_id)->enabled);
    maps.names++; maps.count--;
    assert(tray_menu_set_maps(&menu, &maps, NULL, false, false));
    assert(menu.maps[0].id == second_id && !menu.maps[0].checked);
    assert(!tray_menu_find(&menu, second_id)->enabled);
    assert(!tray_menu_find(&menu, first_id));
    maps.names--; maps.count++;
    assert(tray_menu_set_maps(&menu, &maps, "first.xp2", true, false));
    assert(menu.maps[0].id != first_id);
    tray_menu_set_editor(&menu, true, EDITOR_UNCHANGED, false, false);
    assert(tray_menu_find(&menu, TRAY_EDIT)->enabled);
    assert(!tray_menu_find(&menu, TRAY_APPLY)->enabled);
    tray_menu_set_editor(&menu, true, EDITOR_CHANGED, false, true);
    assert(tray_menu_find(&menu, TRAY_APPLY)->enabled);
    assert(strstr(tray_menu_find(&menu, TRAY_APPLY)->label, "restarts"));
    tray_menu_set_editor(&menu, true, EDITOR_CONFLICT, false, true);
    assert(!tray_menu_find(&menu, TRAY_APPLY)->enabled);
    assert(tray_menu_find(&menu, TRAY_EDIT)->enabled);
    assert(tray_menu_find(&menu, TRAY_DISCARD)->enabled);
    tray_menu_set_editor(&menu, true, EDITOR_CHANGED, true, true);
    assert(!tray_menu_find(&menu, TRAY_APPLY)->enabled);
    tray_menu_clear(&menu);
    return 0;
}
