/* Backend-neutral native text layout and rasterization. */

#ifndef NATIVE_TEXT_H
#define NATIVE_TEXT_H

#include <stddef.h>
#include <stdint.h>

/** Opaque native text service using the platform's installed fonts. */
typedef struct XpTextSystem XpTextSystem;

/** Opaque ordered font set containing a primary face and fallbacks. */
typedef struct XpTextFont XpTextFont;

/** Opaque immutable UTF-8 text layout. */
typedef struct XpTextLayout XpTextLayout;

/** Result returned by the backend-neutral text API. */
typedef enum XpTextStatus {
    /** The operation completed successfully. */
    XP_TEXT_STATUS_OK,
    /** An argument did not satisfy the documented contract. */
    XP_TEXT_STATUS_INVALID_ARGUMENT,
    /** The supplied byte range is not complete, well-formed UTF-8. */
    XP_TEXT_STATUS_INVALID_UTF8,
    /** The requested text direction is reserved for a later implementation. */
    XP_TEXT_STATUS_UNSUPPORTED_DIRECTION,
    /** Memory required by the operation could not be allocated. */
    XP_TEXT_STATUS_OUT_OF_MEMORY,
    /** No usable installed or application font could be opened. */
    XP_TEXT_STATUS_FONT_UNAVAILABLE,
    /** The native shaping or rasterization provider failed. */
    XP_TEXT_STATUS_BACKEND_ERROR
} XpTextStatus;

/** Logical text flow passed through to a native or browser layout provider. */
typedef enum XpTextDirection {
    /** Let the provider infer direction when supported. */
    XP_TEXT_DIRECTION_AUTO,
    /** Lay text out from left to right. */
    XP_TEXT_DIRECTION_LTR,
    /** Lay text out from right to left. Reserved in the current release. */
    XP_TEXT_DIRECTION_RTL
} XpTextDirection;

/** Requested font weight independent of a platform font API. */
typedef enum XpTextWeight {
    /** Normal font weight. */
    XP_TEXT_WEIGHT_NORMAL = 400,
    /** Bold font weight. */
    XP_TEXT_WEIGHT_BOLD = 700
} XpTextWeight;

/** Requested font slant independent of a platform font API. */
typedef enum XpTextSlant {
    /** Upright glyphs. */
    XP_TEXT_SLANT_NORMAL,
    /** Italic or oblique glyphs. */
    XP_TEXT_SLANT_ITALIC
} XpTextSlant;

/** Preferred spacing used to select the final generic fallback. */
typedef enum XpTextSpacing {
    /** A proportionally spaced sans-serif fallback. */
    XP_TEXT_SPACING_PROPORTIONAL,
    /** A fixed-width or closest available monospace fallback. */
    XP_TEXT_SPACING_MONOSPACE
} XpTextSpacing;

/** Description used to resolve an ordered set of installed font faces. */
typedef struct XpTextFontRequest {
    /** Comma-separated preferred family names, in priority order. */
    const char *family_list;
    /** Desired logical pixel height; must be finite and positive. */
    float pixel_height;
    /** Desired font weight. */
    XpTextWeight weight;
    /** Desired font slant. */
    XpTextSlant slant;
    /** Desired spacing and generic fallback category. */
    XpTextSpacing spacing;
} XpTextFontRequest;

/** Description of one immutable UTF-8 layout operation. */
typedef struct XpTextLayoutRequest {
    /** UTF-8 bytes to lay out; may be NULL only when byte_length is zero. */
    const char *utf8;
    /** Exact number of UTF-8 bytes to consume. */
    size_t byte_length;
    /** Logical text flow. RTL is explicitly unsupported in this release. */
    XpTextDirection direction;
    /** Optional BCP 47 language tag retained for provider selection/shaping. */
    const char *language_bcp47;
} XpTextLayoutRequest;

/** Integer layout measurements in logical pixels. */
typedef struct XpTextMetrics {
    /** Width of the complete laid-out block. */
    int width;
    /** Height of the complete laid-out block. */
    int height;
    /** Baseline distance below the first line's top. */
    int ascent;
    /** Distance between adjacent line baselines. */
    int line_spacing;
} XpTextMetrics;

/** Owned top-left-origin RGBA8 raster produced from one layout. */
typedef struct XpTextBitmap {
    /** Owned pixel bytes, or NULL for an empty raster. */
    unsigned char *pixels;
    /** Pixel width. */
    int width;
    /** Pixel height. */
    int height;
    /** Number of bytes between adjacent pixel rows. */
    size_t pitch;
    /** Physical pixels per logical pixel; native providers use one. */
    float scale;
    /** Bitmap left relative to the logical layout origin. */
    float offset_x;
    /** Bitmap top relative to the logical layout origin (not baseline). */
    float offset_y;
} XpTextBitmap;

/**
 * Create a native text service.
 *
 * @param application_font_directory Optional directory containing XPilot's
 *        own legacy fonts. Faces in this directory are registered privately
 *        and participate in the same family-name resolution as system fonts.
 * @param system Empty destination receiving the service.
 * @return Operation status.
 *
 * @remarks The service initializes and owns one balanced SDL_ttf reference.
 * It must outlive every font, layout, and bitmap created through it.
 */
XpTextStatus Xp_text_system_create(const char *application_font_directory,
				   XpTextSystem **system);

/**
 * Destroy a native text service.
 *
 * @param system Address of a service pointer; an already-NULL value is valid.
 */
void Xp_text_system_destroy(XpTextSystem **system);

/**
 * Resolve and open an ordered font set by family name.
 *
 * @param system Native text service.
 * @param request Family, size, style, and spacing request.
 * @param font Empty destination receiving the font set.
 * @return Operation status.
 *
 * @remarks The preferred families are followed by built-in Noto Sans Mono
 * CJK JP, Noto Sans Mono, and platform generic candidates. Missing faces are
 * skipped, and later system fallbacks continue to supply new glyphs.
 */
XpTextStatus Xp_text_font_open(XpTextSystem *system,
			       const XpTextFontRequest *request,
			       XpTextFont **font);

/**
 * Close an ordered font set.
 *
 * @param font Address of a font pointer; an already-NULL value is valid.
 */
void Xp_text_font_close(XpTextFont **font);

/**
 * Return the number of native faces opened for a font set.
 *
 * @param font Font set.
 * @return Face count, or zero for NULL.
 */
size_t Xp_text_font_resolved_count(const XpTextFont *font);

/**
 * Return a resolved native family name.
 *
 * @param font Font set.
 * @param index Zero-based face index in fallback order.
 * @return Borrowed family name, or NULL when the index is unavailable.
 */
const char *Xp_text_font_resolved_family(const XpTextFont *font,
					 size_t index);

/**
 * Test whether any face in a font set supplies a Unicode code point.
 *
 * @param font Font set.
 * @param codepoint Unicode scalar value.
 * @return Nonzero when a glyph is available.
 */
int Xp_text_font_has_glyph(const XpTextFont *font, uint32_t codepoint);

/**
 * Create an immutable UTF-8 layout.
 *
 * @param font Font set used for shaping and fallback.
 * @param request Text bytes and provider-neutral shaping metadata.
 * @param layout Empty destination receiving the layout.
 * @return Operation status.
 *
 * @remarks AUTO and LTR are supported. RTL returns
 * XP_TEXT_STATUS_UNSUPPORTED_DIRECTION without creating a layout. The BCP 47
 * language is retained even where the current native provider does not need
 * it, keeping the boundary usable by a later bidi or browser provider.
 */
XpTextStatus Xp_text_layout_create(XpTextFont *font,
				   const XpTextLayoutRequest *request,
				   XpTextLayout **layout);

/**
 * Destroy a text layout.
 *
 * @param layout Address of a layout pointer; an already-NULL value is valid.
 */
void Xp_text_layout_destroy(XpTextLayout **layout);

/**
 * Return a layout's requested logical direction.
 *
 * @param layout Text layout.
 * @return Requested direction, or AUTO for NULL.
 */
XpTextDirection Xp_text_layout_direction(const XpTextLayout *layout);

/**
 * Return a layout's retained BCP 47 language.
 *
 * @param layout Text layout.
 * @return Borrowed tag; an unspecified tag is represented by an empty string.
 */
const char *Xp_text_layout_language(const XpTextLayout *layout);

/**
 * Measure a text layout.
 *
 * @param layout Text layout.
 * @param metrics Destination receiving metrics.
 * @return Operation status.
 */
XpTextStatus Xp_text_layout_measure(const XpTextLayout *layout,
				    XpTextMetrics *metrics);

/**
 * Rasterize a text layout as white RGBA8 glyphs on transparency.
 *
 * @param layout Text layout.
 * @param bitmap Zero-initialized destination receiving owned pixels.
 * @return Operation status.
 *
 * @remarks Backends tint this neutral raster at draw time. This native
 * adapter is intentionally below the opaque layout API; a browser provider
 * can instead draw its layout directly through Canvas without loading font
 * bytes into the client core.
 */
XpTextStatus Xp_text_layout_rasterize(const XpTextLayout *layout,
				      XpTextBitmap *bitmap);

/**
 * Release a rasterized bitmap.
 *
 * @param bitmap Bitmap to clear; an already-empty bitmap is valid.
 */
void Xp_text_bitmap_destroy(XpTextBitmap *bitmap);

/**
 * Find the largest complete UTF-8 prefix of a byte range.
 *
 * @param utf8 Byte range, or NULL when byte_length is zero.
 * @param byte_length Number of available bytes.
 * @param prefix_length Destination receiving the complete prefix length.
 * @return OK for valid text or an incomplete final sequence; INVALID_UTF8
 *         for an invalid sequence before the end.
 *
 * @remarks This supports legacy byte-counted reveal and clipping callers
 * without allowing a renderer to consume half of a UTF-8 sequence.
 */
XpTextStatus Xp_text_complete_utf8_prefix(const char *utf8,
					  size_t byte_length,
					  size_t *prefix_length);

/**
 * Return a stable diagnostic name for a text status.
 *
 * @param status Text operation status.
 * @return Static ASCII status name.
 */
const char *Xp_text_status_string(XpTextStatus status);

#endif /* NATIVE_TEXT_H */
