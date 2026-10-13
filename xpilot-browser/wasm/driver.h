/* Browser drawing driver built from the XPilot renderer. */

#ifndef XP_BROWSER_DRIVER_H
#define XP_BROWSER_DRIVER_H

#include <stdint.h>

/** Direction bits returned by xp_browser_key_mask. */
enum XpBrowserKey {
    /** Move the scene marker left. */
    XP_BROWSER_KEY_LEFT = 1,
    /** Move the scene marker right. */
    XP_BROWSER_KEY_RIGHT = 2,
    /** Move the scene marker up. */
    XP_BROWSER_KEY_UP = 4,
    /** Move the scene marker down. */
    XP_BROWSER_KEY_DOWN = 8
};

/**
 * Initialize SDL and a WebGL 2 compatible OpenGL ES 3 context.
 * @param width Initial canvas width in logical CSS pixels.
 * @param height Initial canvas height in logical pixels.
 * @return Zero on success, nonzero on failure.
 */
int xp_browser_init(int width, int height);

/**
 * Poll input, update the scene, and present one frame.
 * @param delta_seconds Elapsed time in seconds, clamped to one tenth second.
 * @return Zero on success, nonzero on failure.
 */
int xp_browser_frame(double delta_seconds);

/**
 * Resize the SDL canvas window.
 * @param width New logical width in CSS pixels.
 * @param height New logical height in pixels.
 * @return Zero on success, nonzero on failure.
 */
int xp_browser_resize(int width, int height);

/**
 * Replace the baked Unicode text image drawn by subsequent frames.
 * @param slot Text line to replace, either zero or one.
 * @param rgba Unpremultiplied top-left-origin RGBA8 bytes.
 * @param width Positive bitmap width in pixels.
 * @param height Positive bitmap height in pixels.
 * @param stride Bytes between adjacent rows, at least width times four.
 * @param logical_width Bitmap draw width in logical CSS pixels.
 * @param logical_height Bitmap draw height in logical CSS pixels.
 * @param offset_x Pixel image origin relative to the logical left edge.
 * @param offset_y Bitmap top edge relative to the line baseline.
 * @return Zero on success, nonzero on failure.
 */
int xp_browser_upload_text(int slot, const uint8_t *rgba,
                           int width, int height,
                           int stride, float logical_width,
                           float logical_height, float offset_x,
                           float offset_y);

/** Release the driver and all graphics resources. */
void xp_browser_shutdown(void);

/** @return Bit mask of currently pressed direction keys. */
unsigned int xp_browser_key_mask(void);

/** @return Last mouse x coordinate in logical pixels. */
float xp_browser_pointer_x(void);

/** @return Last mouse y coordinate in logical pixels. */
float xp_browser_pointer_y(void);

/** @return Horizontal coordinate of the movable scene marker. */
float xp_browser_scene_x(void);

/** @return Vertical coordinate of the movable scene marker. */
float xp_browser_scene_y(void);

#endif /* XP_BROWSER_DRIVER_H */
