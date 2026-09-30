#include "tray_menu.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    tray_menu menu = {0};
    tray_status status = {0};
    tray_menu_update(&menu, &status, "a_&_map.xp2");
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
    return 0;
}
