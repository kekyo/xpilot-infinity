#ifndef XPILOT_TRAY_CONTROLLER_H
#define XPILOT_TRAY_CONTROLLER_H

#include "service_control.h"

/** Event-loop-owned controller. The backend must outlive this object. */
typedef struct tray_controller tray_controller;

/** UI-facing state, borrowed until the next controller call. */
typedef struct {
    service_snapshot service; /**< Operating system state, never predicted. */
    bool busy; /**< An accepted/requested operation is not yet observed. */
    bool can_start; /**< Start is currently a valid user action. */
    bool can_stop; /**< Stop is currently a valid user action. */
    service_error operation_error; /**< Most recent request failure. */
    char operation_detail[256]; /**< UTF-8 request failure description. */
} tray_status;

/** Allocate a controller without connecting or performing an operation.
 * @param backend Borrowed backend function table.
 * @return New controller, or NULL if allocation failed.
 */
tray_controller *tray_controller_create(service_control backend);
/** Attach/reconnect, invalidating events from the previous connection.
 * @param controller Live controller.
 */
void tray_controller_connect(tray_controller *controller);
/** Read the current status.
 * @param controller Live controller.
 * @return Borrowed status, valid until the next controller call.
 */
const tray_status *tray_controller_status(const tray_controller *controller);
/** Submit a user operation if permitted by the latest status.
 * @param controller Live controller.
 * @param action Start or stop.
 * @return true if submitted; false for duplicate or unavailable operations.
 */
bool tray_controller_request(tray_controller *controller, service_action action);
/** Detach and free. Never requests service shutdown.
 * @param controller Controller to destroy; NULL is allowed.
 */
void tray_controller_destroy(tray_controller *controller);

#endif
