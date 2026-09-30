#ifndef XPILOT_TRAY_MENU_H
#define XPILOT_TRAY_MENU_H
#include "tray_controller.h"
#include "map_catalog.h"
#include "config_editor.h"
#include <stddef.h>

/** Stable IDs for service management actions. */
typedef enum {
    TRAY_STATUS = 1, /**< Read-only service state. */
    TRAY_MAP, /**< Read-only configured map. */
    TRAY_SEPARATOR, /**< Visual separator. */
    TRAY_START, /**< Start the fixed product service. */
    TRAY_STOP, /**< Normally stop the fixed product service. */
    TRAY_DETAILS, /**< Display errors and the log location. */
    TRAY_QUIT, /**< Exit the tray, leaving the service running. */
    TRAY_MAP_MENU, /**< Single-selection map submenu. */
    TRAY_EDIT, /**< Open the private editing copy with normal user privileges. */
    TRAY_APPLY, /**< Explicitly authorize application of saved changes. */
    TRAY_DISCARD /**< Discard only the private editing session. */
} tray_action;

/** Menu row, owned by the menu instance. */
typedef struct {
    int id; /**< Stable command identifier. */
    bool enabled; /**< Whether user activation is currently permitted. */
    char label[1024]; /**< UTF-8 plain text without mnemonic escaping. */
    bool radio; /**< Selectable map entry. */
    bool checked; /**< Currently configured map, not proof of game readiness. */
} tray_menu_item;

/** Shared menu representation used by both native frontends. */
typedef struct {
    tray_menu_item items[11]; /**< Ordered top-level items. */
    tray_menu_item *maps; /**< Owned submenu entries with stable IDs. */
    size_t map_count; /**< Number of map entries. */
    int next_map_id; /**< Never reuses an ID removed during this menu lifetime. */
    unsigned revision; /**< Changes only when displayed content changes. */
} tray_menu;

/** Synchronize the menu with observed service state.
 * @param menu Zero-initialized menu or a previously updated instance.
 * @param status Borrowed controller state.
 * @param configured_map Configured map label, or an explanation if unknown.
 */
void tray_menu_update(tray_menu *menu, const tray_status *status,
                      const char *configured_map);
/** Find a currently available menu row.
 * @param menu Current menu.
 * @param id Command identifier supplied by the native menu.
 * @return Borrowed row, or NULL for an unknown identifier.
 */
const tray_menu_item *tray_menu_find(const tray_menu *menu, int id);
/** Synchronize the map submenu and single selection.
 * @param menu Live menu.
 * @param catalog Current installed maps.
 * @param selected Selected catalog filename, or NULL for a custom/unknown map.
 * @param enabled Whether shared configuration can be updated now.
 * @param running Whether selecting another map will restart the service.
 * @return true on success; false on allocation failure or identifier exhaustion.
 */
bool tray_menu_set_maps(tray_menu *menu, const map_catalog *catalog, const char *selected,
                        bool enabled, bool running);
/** Update editing actions without changing the service.
 * @param menu Live menu.
 * @param available Shared configuration is readable and supported by the helper.
 * @param state Current saved-copy state.
 * @param busy A service/settings operation is pending.
 * @param running Applying saved changes will restart a running service.
 */
void tray_menu_set_editor(tray_menu *menu, bool available, editor_state state, bool busy, bool running);
/** Release menu-owned storage without taking any service action.
 * @param menu Live or zero-initialized menu.
 */
void tray_menu_clear(tray_menu *menu);
#endif
