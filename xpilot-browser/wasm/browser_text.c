/* Canvas supplies whole-line Unicode fallback; SDL_ttf supplies the bundled
 * face for exact glyph-availability queries and the existing ASCII atlas. */
#include "xpcommon.h"
#include "native_text.h"
#include "native_text_internal.h"
#include <emscripten.h>

struct XpTextSystem { int active; };
struct XpTextFont { TTF_Font *primary; float size; int mono; };
struct XpTextLayout {
    XpTextFont *font;
    char *text;
    char *language;
    XpTextDirection direction;
    XpTextMetrics metrics;
};

EM_JS(int, canvas_text, (const char *text, int mono, float size, float *info, unsigned char *pixels, int capacity), {
    try {
        const raster = Module.xpilotText.rasterize({text: UTF8ToString(text), face: mono ? 'mono' : 'oblique', size, scale: 1});
        if (info) HEAPF32.set([raster.advance, raster.pixelWidth, raster.pixelHeight, raster.offsetX, raster.offsetY, raster.scale], info >> 2);
        if (pixels) {
            if (raster.pixels.length > capacity) return -1;
            HEAPU8.set(raster.pixels, pixels);
        }
        return raster.pixels.length;
    } catch (error) { console.error('Text rendering failed', error); return -1; }
});
XpTextStatus Xp_text_system_create(const char *directory, XpTextSystem **system)
{
    (void)directory;
    if (system == NULL || *system != NULL) return XP_TEXT_STATUS_INVALID_ARGUMENT;
    *system = calloc(1, sizeof(**system));
    return *system ? XP_TEXT_STATUS_OK : XP_TEXT_STATUS_OUT_OF_MEMORY;
}
void Xp_text_system_destroy(XpTextSystem **system) { if (system) { free(*system); *system = NULL; } }
XpTextStatus Xp_text_font_open(XpTextSystem *system, const XpTextFontRequest *request, XpTextFont **font)
{
    if (!system || !request || !font || *font || !isfinite(request->pixel_height)
        || request->pixel_height <= 0 || request->pixel_height > 512) return XP_TEXT_STATUS_INVALID_ARGUMENT;
    XpTextFont *created = calloc(1, sizeof(*created));
    if (!created) return XP_TEXT_STATUS_OUT_OF_MEMORY;
    created->size = request->pixel_height;
    created->mono = request->spacing == XP_TEXT_SPACING_MONOSPACE;
    created->primary = TTF_OpenFont(created->mono ? CONF_FONTDIR "VeraMoBd.ttf"
                                                : CONF_FONTDIR "FreeSansBoldOblique.ttf", created->size);
    if (!created->primary) { free(created); return XP_TEXT_STATUS_FONT_UNAVAILABLE; }
    *font = created;
    return XP_TEXT_STATUS_OK;
}
void Xp_text_font_close(XpTextFont **font)
{
    if (!font || !*font) return;
    TTF_CloseFont((*font)->primary); free(*font); *font = NULL;
}
size_t Xp_text_font_resolved_count(const XpTextFont *font) { return font ? 1 : 0; }
const char *Xp_text_font_resolved_family(const XpTextFont *font, size_t index)
{
    return font && index == 0 ? TTF_GetFontFamilyName(font->primary) : NULL;
}
int Xp_text_font_has_glyph(const XpTextFont *font, uint32_t codepoint)
{
    /* Canvas fallback is not a glyph enumeration API. Only claim glyphs from
     * the actual bundled face here; line rendering still uses browser fallback. */
    return font && TTF_FontHasGlyph(font->primary, codepoint);
}
TTF_Font *Xp_text_font_primary_ttf(XpTextFont *font) { return font ? font->primary : NULL; }
XpTextStatus Xp_text_layout_create(XpTextFont *font, const XpTextLayoutRequest *request, XpTextLayout **layout)
{
    if (!font || !request || !layout || *layout || (!request->utf8 && request->byte_length))
        return XP_TEXT_STATUS_INVALID_ARGUMENT;
    if (request->direction == XP_TEXT_DIRECTION_RTL) return XP_TEXT_STATUS_UNSUPPORTED_DIRECTION;
    size_t complete;
    XpTextStatus status = Xp_text_complete_utf8_prefix(request->utf8, request->byte_length, &complete);
    if (status != XP_TEXT_STATUS_OK || complete != request->byte_length) return XP_TEXT_STATUS_INVALID_UTF8;
    if (request->byte_length > 8192) return XP_TEXT_STATUS_INVALID_ARGUMENT;
    XpTextLayout *created = calloc(1, sizeof(*created));
    if (!created) return XP_TEXT_STATUS_OUT_OF_MEMORY;
    created->text = malloc(request->byte_length + 1);
    created->language = strdup(request->language_bcp47 ? request->language_bcp47 : "");
    if (!created->text || !created->language) { Xp_text_layout_destroy(&created); return XP_TEXT_STATUS_OUT_OF_MEMORY; }
    memcpy(created->text, request->utf8, request->byte_length);
    created->text[request->byte_length] = '\0';
    created->font = font;
    created->direction = request->direction;
    float info[6];
    if (canvas_text(created->text, font->mono, font->size, info, NULL, 0) < 0) {
        Xp_text_layout_destroy(&created); return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    created->metrics.width = (int)ceilf(info[0]);
    created->metrics.ascent = TTF_GetFontAscent(font->primary);
    created->metrics.height = TTF_GetFontHeight(font->primary);
    created->metrics.line_spacing = TTF_GetFontLineSkip(font->primary);
    *layout = created;
    return XP_TEXT_STATUS_OK;
}
void Xp_text_layout_destroy(XpTextLayout **layout)
{
    if (!layout || !*layout) return;
    free((*layout)->text); free((*layout)->language); free(*layout); *layout = NULL;
}
XpTextDirection Xp_text_layout_direction(const XpTextLayout *layout)
{ return layout ? layout->direction : XP_TEXT_DIRECTION_AUTO; }
const char *Xp_text_layout_language(const XpTextLayout *layout) { return layout ? layout->language : ""; }
XpTextStatus Xp_text_layout_measure(const XpTextLayout *layout, XpTextMetrics *metrics)
{
    if (!layout || !metrics) return XP_TEXT_STATUS_INVALID_ARGUMENT;
    *metrics = layout->metrics; return XP_TEXT_STATUS_OK;
}
XpTextStatus Xp_text_layout_rasterize(const XpTextLayout *layout, XpTextBitmap *bitmap)
{
    if (!layout || !bitmap || bitmap->pixels) return XP_TEXT_STATUS_INVALID_ARGUMENT;
    float info[6];
    int size = canvas_text(layout->text, layout->font->mono, layout->font->size, info, NULL, 0);
    if (size <= 0) return XP_TEXT_STATUS_BACKEND_ERROR;
    bitmap->pixels = malloc((size_t)size);
    if (!bitmap->pixels) return XP_TEXT_STATUS_OUT_OF_MEMORY;
    if (canvas_text(layout->text, layout->font->mono, layout->font->size, NULL, bitmap->pixels, size) < 0) {
        Xp_text_bitmap_destroy(bitmap); return XP_TEXT_STATUS_BACKEND_ERROR;
    }
    bitmap->width = (int)info[1]; bitmap->height = (int)info[2]; bitmap->pitch = (size_t)bitmap->width * 4;
    bitmap->offset_x = info[3]; bitmap->offset_y = info[4] + layout->metrics.ascent;
    bitmap->scale = info[5];
    return XP_TEXT_STATUS_OK;
}
void Xp_text_bitmap_destroy(XpTextBitmap *bitmap)
{ if (bitmap) { free(bitmap->pixels); memset(bitmap, 0, sizeof(*bitmap)); } }
const char *Xp_text_status_string(XpTextStatus status)
{ return status == XP_TEXT_STATUS_OK ? "ok" : "browser text error"; }
