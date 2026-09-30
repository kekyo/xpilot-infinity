#ifndef XPILOT_TRAY_XEMBED_H
#define XPILOT_TRAY_XEMBED_H
#include "tray_menu.h"
#include <gtk/gtk.h>
/** X11 tray attachment; unavailable on other GDK backends. */
typedef struct tray_xembed tray_xembed;
/** Watch the display's system tray selection and request embedding when enabled.
 * @param display Borrowed GDK display.
 * @param menu Borrowed menu kept alive until destruction.
 * @param icon Absolute icon path.
 * @param activate Action callback, with -1 requesting a menu refresh.
 * @param available Callback reporting actual embedding, never a mere dock request.
 * @param context Borrowed callback context.
 * @return Owned attachment, or NULL when X11/XFixes is unavailable.
 */
tray_xembed *tray_xembed_create(GdkDisplay *display, const tray_menu *menu, const char *icon,
    void (*activate)(void *, int), void (*available)(void *, bool), void *context);
/** Enable fallback or remove its icon before using an SNI host.
 * @param tray Live attachment, or NULL.
 * @param enabled Whether an XEmbed icon is wanted.
 */
void tray_xembed_enable(tray_xembed *tray, bool enabled);
/** Remove the icon and all event subscriptions.
 * @param tray Owned attachment, or NULL.
 */
void tray_xembed_destroy(tray_xembed *tray);
#endif
