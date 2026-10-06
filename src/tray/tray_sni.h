#ifndef XPILOT_TRAY_SNI_H
#define XPILOT_TRAY_SNI_H
#include "tray_menu.h"
#include <gio/gio.h>

/** Session-bus StatusNotifierItem and DBusMenu exporter. */
typedef struct tray_sni tray_sni;
/** Create an exporter and discover the session's tray host asynchronously.
 * @param bus Borrowed session connection; retained until destruction.
 * @param menu Borrowed menu, which must outlive the exporter.
 * @param icon_path UTF-8 path to the product PNG icon.
 * @param activate Callback for enabled command IDs (zero presents the window,
 * minus one requests a configuration refresh before showing the menu).
 * @param available Callback after registration and host availability changes.
 * @param context Borrowed callback context, detached on destruction.
 * @param error Optional output for an export error.
 * @return Owned exporter or NULL on failure.
 */
tray_sni *tray_sni_create(GDBusConnection *bus, const tray_menu *menu,
    const char *icon_path, void (*activate)(void *, int),
    void (*available)(void *, bool), void *context, GError **error);
/** Announce changed menu properties/layout, at most once per menu revision.
 * @param sni Live exporter.
 */
void tray_sni_update(tray_sni *sni);
/** Unexport and cancel callbacks. Does not affect any server service.
 * @param sni Exporter or NULL.
 */
void tray_sni_destroy(tray_sni *sni);
#endif
