#ifndef XPILOT_TRAY_MENU_H
#define XPILOT_TRAY_MENU_H
#include "tray_controller.h"
#include <stddef.h>

/** Stable IDs for service management actions. */
typedef enum {
    TRAY_STATUS = 1, /**< Read-only service state. */
    TRAY_MAP, /**< Read-only configured map. */
    TRAY_SEPARATOR, /**< Visual separator. */
    TRAY_START, /**< Start the fixed product service. */
    TRAY_STOP, /**< Normally stop the fixed product service. */
    TRAY_DETAILS, /**< Display errors and the log location. */
    TRAY_QUIT /**< Exit the tray, leaving the service running. */
} tray_action;

/** Menu row, owned by the menu instance. */
typedef struct {
    int id; /**< Stable command identifier. */
    bool enabled; /**< Whether user activation is currently permitted. */
    char label[1024]; /**< UTF-8 plain text without mnemonic escaping. */
} tray_menu_item;

/** Shared menu representation used by both native frontends. */
typedef struct {
    tray_menu_item items[7]; /**< Ordered top-level items. */
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
#endif
