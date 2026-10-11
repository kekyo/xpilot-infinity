/* UTF-8 text drawing through the backend-neutral native text service. */

#include "unicode_text_renderer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UNICODE_TEXT_CACHE_LIMIT 128

typedef struct UnicodeTextEntry {
    char *utf8;
    size_t byte_length;
    XpTextLayout *layout;
    XpTextMetrics metrics;
    float bitmap_width, bitmap_height, offset_x, offset_y;
    RendererTexture *texture;
    uint64_t last_use;
    struct UnicodeTextEntry *next;
} UnicodeTextEntry;

struct UnicodeTextRenderer {
    SdlRenderer *sdl_renderer;
    Renderer *frontend;
    XpTextFont *font;
    UnicodeTextEntry *entries;
    size_t entry_count;
    uint64_t use_clock;
};

static RendererStatus Unicode_text_status(XpTextStatus status)
{
    switch (status) {
    case XP_TEXT_STATUS_OK:
        return RENDERER_STATUS_OK;
    case XP_TEXT_STATUS_OUT_OF_MEMORY:
        return RENDERER_STATUS_OUT_OF_MEMORY;
    case XP_TEXT_STATUS_INVALID_ARGUMENT:
    case XP_TEXT_STATUS_INVALID_UTF8:
    case XP_TEXT_STATUS_UNSUPPORTED_DIRECTION:
        return RENDERER_STATUS_INVALID_ARGUMENT;
    case XP_TEXT_STATUS_FONT_UNAVAILABLE:
    case XP_TEXT_STATUS_BACKEND_ERROR:
        return RENDERER_STATUS_BACKEND_ERROR;
    default:
        return RENDERER_STATUS_BACKEND_ERROR;
    }
}

static RendererStatus Unicode_text_retain(
    UnicodeTextRenderer *text_renderer, RendererStatus status)
{
    if (status == RENDERER_STATUS_OK)
        return status;
    return Sdl_renderer_track_frame_result(text_renderer->sdl_renderer,
                                           status);
}

static RendererStatus Unicode_text_preflight(
    UnicodeTextRenderer *text_renderer)
{
    if (text_renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    return Sdl_renderer_track_frame_result(text_renderer->sdl_renderer,
                                           RENDERER_STATUS_OK);
}

static const char *Unicode_text_language(
    const XpTextLayoutRequest *request)
{
    return request->language_bcp47 != NULL ? request->language_bcp47 : "";
}

static int Unicode_text_request_key_valid(
    const XpTextLayoutRequest *request)
{
    return request != NULL
        && (request->utf8 != NULL || request->byte_length == 0)
        && request->byte_length != SIZE_MAX
        && (request->direction == XP_TEXT_DIRECTION_AUTO
            || request->direction == XP_TEXT_DIRECTION_LTR
            || request->direction == XP_TEXT_DIRECTION_RTL);
}

static int Unicode_text_entry_matches(
    const UnicodeTextEntry *entry, const XpTextLayoutRequest *request)
{
    return entry->byte_length == request->byte_length
        && Xp_text_layout_direction(entry->layout) == request->direction
        && strcmp(Xp_text_layout_language(entry->layout),
                  Unicode_text_language(request)) == 0
        && (request->byte_length == 0
            || memcmp(entry->utf8, request->utf8,
                      request->byte_length) == 0);
}

static void Unicode_text_entry_release_cpu(UnicodeTextEntry *entry)
{
    if (entry == NULL)
        return;
    Xp_text_layout_destroy(&entry->layout);
    free(entry->utf8);
    free(entry);
}

static void Unicode_text_touch(UnicodeTextRenderer *text_renderer,
                               UnicodeTextEntry *entry)
{
    UnicodeTextEntry *cursor;

    if (text_renderer->use_clock == UINT64_MAX) {
        uint64_t stamp = 1;

        for (cursor = text_renderer->entries; cursor != NULL;
             cursor = cursor->next) {
            cursor->last_use = stamp++;
        }
        text_renderer->use_clock = stamp;
    } else {
        text_renderer->use_clock++;
    }
    entry->last_use = text_renderer->use_clock;
}

static RendererStatus Unicode_text_evict_oldest(
    UnicodeTextRenderer *text_renderer)
{
    UnicodeTextEntry **link;
    UnicodeTextEntry **oldest_link = NULL;
    UnicodeTextEntry *oldest = NULL;
    RendererStatus status;

    for (link = &text_renderer->entries; *link != NULL;
         link = &(*link)->next) {
        if (oldest == NULL || (*link)->last_use < oldest->last_use) {
            oldest = *link;
            oldest_link = link;
        }
    }
    if (oldest == NULL || oldest_link == NULL)
        return RENDERER_STATUS_BACKEND_ERROR;
    if (oldest->texture != NULL) {
        status = Renderer_texture_destroy_streaming(
            text_renderer->frontend, oldest->texture);
        if (status != RENDERER_STATUS_OK)
            return Unicode_text_retain(text_renderer, status);
        oldest->texture = NULL;
    }
    *oldest_link = oldest->next;
    text_renderer->entry_count--;
    Unicode_text_entry_release_cpu(oldest);
    return RENDERER_STATUS_OK;
}

static RendererStatus Unicode_text_entry_create(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, UnicodeTextEntry **entry)
{
    XpTextLayoutRequest normalized_request;
    UnicodeTextEntry *candidate;
    XpTextStatus text_status;
    RendererStatus status;

    candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL)
        return Unicode_text_retain(text_renderer,
                                   RENDERER_STATUS_OUT_OF_MEMORY);
    candidate->utf8 = malloc(request->byte_length + 1);
    if (candidate->utf8 == NULL) {
        Unicode_text_entry_release_cpu(candidate);
        return Unicode_text_retain(text_renderer,
                                   RENDERER_STATUS_OUT_OF_MEMORY);
    }
    if (request->byte_length > 0)
        memcpy(candidate->utf8, request->utf8, request->byte_length);
    candidate->utf8[request->byte_length] = '\0';
    candidate->byte_length = request->byte_length;
    normalized_request = *request;
    normalized_request.utf8 = candidate->utf8;
    normalized_request.language_bcp47 = Unicode_text_language(request);
    text_status = Xp_text_layout_create(
        text_renderer->font, &normalized_request, &candidate->layout);
    if (text_status != XP_TEXT_STATUS_OK) {
        Unicode_text_entry_release_cpu(candidate);
        return Unicode_text_retain(
            text_renderer, Unicode_text_status(text_status));
    }
    text_status = Xp_text_layout_measure(candidate->layout,
                                         &candidate->metrics);
    if (text_status != XP_TEXT_STATUS_OK) {
        Unicode_text_entry_release_cpu(candidate);
        return Unicode_text_retain(
            text_renderer, Unicode_text_status(text_status));
    }
    if (text_renderer->entry_count >= UNICODE_TEXT_CACHE_LIMIT) {
        status = Unicode_text_evict_oldest(text_renderer);
        if (status != RENDERER_STATUS_OK) {
            Unicode_text_entry_release_cpu(candidate);
            return status;
        }
    }
    Unicode_text_touch(text_renderer, candidate);
    candidate->next = text_renderer->entries;
    text_renderer->entries = candidate;
    text_renderer->entry_count++;
    *entry = candidate;
    return RENDERER_STATUS_OK;
}

static RendererStatus Unicode_text_entry_acquire(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, UnicodeTextEntry **entry)
{
    UnicodeTextEntry *cursor;

    if (!Unicode_text_request_key_valid(request) || entry == NULL)
        return Unicode_text_retain(text_renderer,
                                   RENDERER_STATUS_INVALID_ARGUMENT);
    for (cursor = text_renderer->entries; cursor != NULL;
         cursor = cursor->next) {
        if (Unicode_text_entry_matches(cursor, request)) {
            Unicode_text_touch(text_renderer, cursor);
            *entry = cursor;
            return RENDERER_STATUS_OK;
        }
    }
    return Unicode_text_entry_create(text_renderer, request, entry);
}

RendererStatus Unicode_text_renderer_create(
    SdlRenderer *sdl_renderer, XpTextFont *font,
    UnicodeTextRenderer **text_renderer)
{
    UnicodeTextRenderer *candidate;
    Renderer *frontend;

    if (sdl_renderer == NULL || font == NULL || text_renderer == NULL
        || *text_renderer != NULL) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    frontend = Sdl_renderer_frontend(sdl_renderer);
    if (frontend == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL)
        return RENDERER_STATUS_OUT_OF_MEMORY;
    candidate->sdl_renderer = sdl_renderer;
    candidate->frontend = frontend;
    candidate->font = font;
    *text_renderer = candidate;
    return RENDERER_STATUS_OK;
}

RendererStatus Unicode_text_renderer_destroy(
    UnicodeTextRenderer **text_renderer)
{
    UnicodeTextRenderer *renderer;
    UnicodeTextEntry *entry;
    RendererStatus status;

    if (text_renderer == NULL || *text_renderer == NULL)
        return RENDERER_STATUS_OK;
    renderer = *text_renderer;
    for (entry = renderer->entries; entry != NULL; entry = entry->next) {
        if (entry->texture == NULL)
            continue;
        status = Renderer_texture_destroy(renderer->frontend,
                                          entry->texture);
        if (status != RENDERER_STATUS_OK)
            return status;
        entry->texture = NULL;
    }
    while (renderer->entries != NULL) {
        entry = renderer->entries;
        renderer->entries = entry->next;
        Unicode_text_entry_release_cpu(entry);
    }
    free(renderer);
    *text_renderer = NULL;
    return RENDERER_STATUS_OK;
}

int Unicode_text_renderer_matches(
    const UnicodeTextRenderer *text_renderer,
    const SdlRenderer *sdl_renderer, const XpTextFont *font)
{
    return text_renderer != NULL && sdl_renderer != NULL && font != NULL
        && text_renderer->sdl_renderer == sdl_renderer
        && text_renderer->font == font;
}

RendererStatus Unicode_text_renderer_measure(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, XpTextMetrics *metrics)
{
    UnicodeTextEntry *entry = NULL;
    RendererStatus status;

    status = Unicode_text_preflight(text_renderer);
    if (status != RENDERER_STATUS_OK)
        return status;
    if (metrics == NULL)
        return Unicode_text_retain(text_renderer,
                                   RENDERER_STATUS_INVALID_ARGUMENT);
    status = Unicode_text_entry_acquire(text_renderer, request, &entry);
    if (status != RENDERER_STATUS_OK)
        return status;
    *metrics = entry->metrics;
    return RENDERER_STATUS_OK;
}

static RendererStatus Unicode_text_ensure_texture(
    UnicodeTextRenderer *text_renderer, UnicodeTextEntry *entry)
{
    RendererTextureDesc description;
    XpTextBitmap bitmap;
    XpTextStatus text_status;
    RendererStatus status;

    if (entry->texture != NULL || entry->metrics.width == 0)
        return RENDERER_STATUS_OK;
    memset(&bitmap, 0, sizeof(bitmap));
    text_status = Xp_text_layout_rasterize(entry->layout, &bitmap);
    if (text_status != XP_TEXT_STATUS_OK) {
        return Unicode_text_retain(
            text_renderer, Unicode_text_status(text_status));
    }
    if (bitmap.pixels == NULL || bitmap.width <= 0 || bitmap.height <= 0
        || !isfinite(bitmap.scale) || bitmap.scale <= 0
        || !isfinite(bitmap.offset_x) || !isfinite(bitmap.offset_y)) {
        Xp_text_bitmap_destroy(&bitmap);
        return Unicode_text_retain(text_renderer,
                                   RENDERER_STATUS_BACKEND_ERROR);
    }
    entry->bitmap_width = (float)bitmap.width / bitmap.scale;
    entry->bitmap_height = (float)bitmap.height / bitmap.scale;
    entry->offset_x = bitmap.offset_x;
    entry->offset_y = bitmap.offset_y;
    description.width = bitmap.width;
    description.height = bitmap.height;
    description.filter = RENDERER_TEXTURE_FILTER_LINEAR;
    description.wrap = RENDERER_TEXTURE_WRAP_CLAMP;
    status = Sdl_renderer_flush(text_renderer->sdl_renderer);
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_texture_create_streaming(
            text_renderer->frontend, &description, bitmap.pixels,
            bitmap.pitch, &entry->texture);
    }
    Xp_text_bitmap_destroy(&bitmap);
    if (status != RENDERER_STATUS_OK)
        return Unicode_text_retain(text_renderer, status);
    return RENDERER_STATUS_OK;
}

static RendererStatus Unicode_text_build_vertices(
    const UnicodeTextEntry *entry, RendererPoint2D anchor,
    TextGeometryHorizontalAlign horizontal,
    TextGeometryVerticalAlign vertical, RendererColor color,
    TextRendererSpace space, RendererVertex2D vertices[6])
{
    static const size_t indices[6] = {0, 1, 2, 0, 2, 3};
    RendererVertex2D corners[4];
    float left;
    float top;
    float right;
    float bottom;
    size_t index;

    if (!isfinite(anchor.x) || !isfinite(anchor.y)
        || (horizontal != TEXT_GEOMETRY_ALIGN_LEFT
            && horizontal != TEXT_GEOMETRY_ALIGN_CENTER
            && horizontal != TEXT_GEOMETRY_ALIGN_RIGHT)
        || (vertical != TEXT_GEOMETRY_ALIGN_TOP
            && vertical != TEXT_GEOMETRY_ALIGN_MIDDLE
            && vertical != TEXT_GEOMETRY_ALIGN_BOTTOM)
        || (space != TEXT_RENDERER_SPACE_HUD
            && space != TEXT_RENDERER_SPACE_WORLD)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    left = anchor.x;
    if (horizontal == TEXT_GEOMETRY_ALIGN_CENTER)
        left -= (float)entry->metrics.width * 0.5f;
    else if (horizontal == TEXT_GEOMETRY_ALIGN_RIGHT)
        left -= (float)entry->metrics.width;
    top = anchor.y;
    if (vertical == TEXT_GEOMETRY_ALIGN_MIDDLE)
        top -= (float)entry->metrics.height * 0.5f;
    else if (vertical == TEXT_GEOMETRY_ALIGN_BOTTOM)
        top -= (float)entry->metrics.height;
    left += entry->offset_x;
    top += entry->offset_y;
    right = left + entry->bitmap_width;
    bottom = top + entry->bitmap_height;
    if (!isfinite(left) || !isfinite(top) || !isfinite(right)
        || !isfinite(bottom)) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    corners[0] = (RendererVertex2D){left, top, 0.0f, 0.0f, color};
    corners[1] = (RendererVertex2D){right, top, 1.0f, 0.0f, color};
    corners[2] = (RendererVertex2D){right, bottom, 1.0f, 1.0f, color};
    corners[3] = (RendererVertex2D){left, bottom, 0.0f, 1.0f, color};
    for (index = 0; index < 6; index++) {
        vertices[index] = corners[indices[index]];
        if (space == TEXT_RENDERER_SPACE_WORLD)
            vertices[index].y = 2.0f * anchor.y - vertices[index].y;
    }
    return RENDERER_STATUS_OK;
}

RendererStatus Unicode_text_renderer_draw(
    UnicodeTextRenderer *text_renderer,
    const XpTextLayoutRequest *request, RendererPoint2D anchor,
    TextGeometryHorizontalAlign horizontal,
    TextGeometryVerticalAlign vertical, RendererColor color,
    TextRendererSpace space)
{
    UnicodeTextEntry *entry = NULL;
    RendererVertex2D vertices[6];
    RendererStatus status;

    status = Unicode_text_preflight(text_renderer);
    if (status != RENDERER_STATUS_OK)
        return status;
    status = Unicode_text_entry_acquire(text_renderer, request, &entry);
    if (status != RENDERER_STATUS_OK)
        return status;
    if (entry->metrics.width == 0)
        return RENDERER_STATUS_OK;
    status = Unicode_text_ensure_texture(text_renderer, entry);
    if (status != RENDERER_STATUS_OK)
        return status;
    status = Unicode_text_build_vertices(
        entry, anchor, horizontal, vertical, color, space, vertices);
    if (status != RENDERER_STATUS_OK)
        return Unicode_text_retain(text_renderer, status);
    status = Renderer_set_blend(text_renderer->frontend,
                                RENDERER_BLEND_ALPHA);
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_draw_triangles(
            text_renderer->frontend, entry->texture, vertices, 6);
    }
    if (status == RENDERER_STATUS_OK)
        status = Sdl_renderer_flush(text_renderer->sdl_renderer);
    return status == RENDERER_STATUS_OK
        ? status : Unicode_text_retain(text_renderer, status);
}
