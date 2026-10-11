/* UTF-8 text drawing through the backend-neutral native text service. */

#ifndef UNICODE_TEXT_RENDERER_H
#define UNICODE_TEXT_RENDERER_H

#include "native_text.h"
#include "text_renderer.h"

/** Opaque SDL texture cache for backend-neutral UTF-8 layouts. */
typedef struct UnicodeTextRenderer UnicodeTextRenderer;

/**
 * Create a UTF-8 renderer borrowing one SDL renderer and native font set.
 *
 * @param sdl_renderer SDL renderer used for texture uploads and drawing.
 * @param font Native font set used for layout and rasterization.
 * @param text_renderer Empty destination receiving the renderer.
 * @return Operation status.
 *
 * @remarks Creation is CPU-only. Both borrowed objects must outlive the
 * returned renderer. Layout requests retain direction and BCP 47 language at
 * this boundary so a later bidi-capable provider can implement RTL without
 * changing callers; RTL itself is not supported in this release.
 */
RendererStatus Unicode_text_renderer_create(
    SdlRenderer *sdl_renderer, XpTextFont *font,
    UnicodeTextRenderer **text_renderer);

/**
 * Destroy all cached layouts and textures.
 *
 * @param text_renderer Address of a renderer pointer; NULL is valid.
 * @return Operation status.
 *
 * @remarks Destruction must occur outside an active renderer frame. A state
 * failure leaves the renderer available for retry.
 */
RendererStatus Unicode_text_renderer_destroy(
    UnicodeTextRenderer **text_renderer);

/**
 * Evict cached layouts and bitmaps after a raster-scale change.
 * @param text_renderer Cache to clear; NULL is accepted.
 * @return Operation status. Call outside an active renderer frame.
 */
RendererStatus Unicode_text_renderer_clear(UnicodeTextRenderer *text_renderer);

/**
 * Test whether a renderer uses an exact SDL renderer and native font pair.
 *
 * @param text_renderer UTF-8 renderer.
 * @param sdl_renderer Expected SDL renderer.
 * @param font Expected native font set.
 * @return Nonzero only for the original pair.
 */
int Unicode_text_renderer_matches(
    const UnicodeTextRenderer *text_renderer,
    const SdlRenderer *sdl_renderer, const XpTextFont *font);

/**
 * Measure one UTF-8 layout request and retain it in the bounded cache.
 *
 * @param text_renderer UTF-8 renderer used by the active frame.
 * @param request Exact text, direction, and language request.
 * @param metrics Receives measurements on success and remains unchanged on
 * failure.
 * @return Operation status.
 */
RendererStatus Unicode_text_renderer_measure(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, XpTextMetrics *metrics);

/**
 * Draw one UTF-8 layout request.
 *
 * @param text_renderer UTF-8 renderer used by the active frame.
 * @param request Exact text, direction, and language request.
 * @param anchor Alignment anchor in the selected coordinate space.
 * @param horizontal Horizontal alignment of the laid-out block.
 * @param vertical Vertical alignment of the laid-out block.
 * @param color Unpremultiplied glyph tint.
 * @param space Coordinate convention for generated vertices.
 * @return Operation status.
 *
 * @remarks A raster texture is created lazily and cached. Pending drawing is
 * flushed before texture lifetime changes and after this draw, preserving
 * renderer command ordering.
 */
RendererStatus Unicode_text_renderer_draw(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, RendererPoint2D anchor,
    TextGeometryHorizontalAlign horizontal,
    TextGeometryVerticalAlign vertical, RendererColor color,
    TextRendererSpace space);

#endif /* UNICODE_TEXT_RENDERER_H */
