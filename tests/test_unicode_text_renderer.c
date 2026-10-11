#include "test_helpers.h"

#include "unicode_text_renderer.h"

#include <stdlib.h>
#include <string.h>

#define FLOAT_TOLERANCE 0.0001f

struct Renderer {
    int frame_active;
};

struct RendererTexture {
    int identity;
};

struct SdlRenderer {
    Renderer *frontend;
    RendererStatus frame_result;
};

struct XpTextFont {
    int identity;
};

struct XpTextLayout {
    char *utf8;
    size_t byte_length;
    XpTextDirection direction;
    char *language;
};

static Renderer frontend;
static SdlRenderer sdl_renderer = {&frontend, RENDERER_STATUS_OK};
static XpTextFont font = {1};
static RendererTexture texture = {1};
static RendererVertex2D captured_vertices[6];
static XpTextDirection captured_direction;
static char captured_language[16];
static int layout_create_calls;
static int layout_destroy_calls;
static int rasterize_calls;
static int texture_create_calls;
static int texture_destroy_calls;
static int draw_calls;
static int flush_calls;
static int ink_bitmap;

static int float_equal(float actual, float expected)
{
    const float difference = actual - expected;

    return difference >= -FLOAT_TOLERANCE
        && difference <= FLOAT_TOLERANCE;
}

static void reset_fixture(void)
{
    memset(&frontend, 0, sizeof(frontend));
    sdl_renderer.frame_result = RENDERER_STATUS_OK;
    memset(captured_vertices, 0, sizeof(captured_vertices));
    captured_direction = XP_TEXT_DIRECTION_AUTO;
    memset(captured_language, 0, sizeof(captured_language));
    layout_create_calls = 0;
    layout_destroy_calls = 0;
    rasterize_calls = 0;
    texture_create_calls = 0;
    texture_destroy_calls = 0;
    draw_calls = 0;
    flush_calls = 0;
    ink_bitmap = 0;
}

static void begin_frame(void)
{
    frontend.frame_active = 1;
    sdl_renderer.frame_result = RENDERER_STATUS_OK;
}

Renderer *Sdl_renderer_frontend(SdlRenderer *renderer)
{
    return renderer != NULL ? renderer->frontend : NULL;
}

RendererStatus Sdl_renderer_track_frame_result(
    SdlRenderer *renderer, RendererStatus status)
{
    if (renderer == NULL || renderer->frontend == NULL
        || !renderer->frontend->frame_active) {
        return RENDERER_STATUS_INVALID_STATE;
    }
    if (renderer->frame_result == RENDERER_STATUS_OK
        && status != RENDERER_STATUS_OK) {
        renderer->frame_result = status;
    }
    return renderer->frame_result;
}

RendererStatus Sdl_renderer_flush(SdlRenderer *renderer)
{
    if (renderer == NULL || renderer->frontend == NULL
        || !renderer->frontend->frame_active) {
        return RENDERER_STATUS_INVALID_STATE;
    }
    flush_calls++;
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_texture_create_streaming(
    Renderer *renderer, const RendererTextureDesc *desc,
    const uint8_t *pixels, size_t pitch, RendererTexture **created)
{
    if (renderer == NULL || !renderer->frame_active || desc == NULL
        || desc->width != (ink_bitmap ? 24 : 20) || desc->height != (ink_bitmap ? 8 : 10) || pixels == NULL
        || pitch != (ink_bitmap ? 96 : 80) || created == NULL || *created != NULL) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    texture_create_calls++;
    *created = &texture;
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_texture_destroy_streaming(
    Renderer *renderer, RendererTexture *destroyed)
{
    if (renderer == NULL || !renderer->frame_active
        || destroyed != &texture) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    texture_destroy_calls++;
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_texture_destroy(Renderer *renderer,
                                        RendererTexture *destroyed)
{
    if (renderer == NULL || renderer->frame_active || destroyed != &texture)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    texture_destroy_calls++;
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_set_blend(Renderer *renderer,
                                  RendererBlendMode blend)
{
    if (renderer == NULL || !renderer->frame_active
        || blend != RENDERER_BLEND_ALPHA) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

RendererStatus Renderer_draw_triangles(Renderer *renderer,
                                       RendererTexture *draw_texture,
                                       const RendererVertex2D *vertices,
                                       size_t vertex_count)
{
    if (renderer == NULL || !renderer->frame_active
        || draw_texture != &texture || vertices == NULL
        || vertex_count != 6) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    memcpy(captured_vertices, vertices, sizeof(captured_vertices));
    draw_calls++;
    return RENDERER_STATUS_OK;
}

XpTextStatus Xp_text_layout_create(XpTextFont *requested_font,
                                   const XpTextLayoutRequest *request,
                                   XpTextLayout **layout)
{
    XpTextLayout *created;

    if (requested_font != &font || request == NULL || layout == NULL
        || *layout != NULL || request->utf8 == NULL
        || request->direction == XP_TEXT_DIRECTION_RTL) {
        return request != NULL
                   && request->direction == XP_TEXT_DIRECTION_RTL
            ? XP_TEXT_STATUS_UNSUPPORTED_DIRECTION
            : XP_TEXT_STATUS_INVALID_ARGUMENT;
    }
    created = calloc(1, sizeof(*created));
    if (created == NULL)
        return XP_TEXT_STATUS_OUT_OF_MEMORY;
    created->utf8 = malloc(request->byte_length + 1);
    created->language = malloc(strlen(request->language_bcp47) + 1);
    if (created->utf8 == NULL || created->language == NULL) {
        free(created->utf8);
        free(created->language);
        free(created);
        return XP_TEXT_STATUS_OUT_OF_MEMORY;
    }
    memcpy(created->utf8, request->utf8, request->byte_length);
    created->utf8[request->byte_length] = '\0';
    strcpy(created->language, request->language_bcp47);
    created->byte_length = request->byte_length;
    created->direction = request->direction;
    captured_direction = request->direction;
    strcpy(captured_language, request->language_bcp47);
    layout_create_calls++;
    *layout = created;
    return XP_TEXT_STATUS_OK;
}

void Xp_text_layout_destroy(XpTextLayout **layout)
{
    if (layout == NULL || *layout == NULL)
        return;
    free((*layout)->utf8);
    free((*layout)->language);
    free(*layout);
    *layout = NULL;
    layout_destroy_calls++;
}

XpTextDirection Xp_text_layout_direction(const XpTextLayout *layout)
{
    return layout != NULL ? layout->direction : XP_TEXT_DIRECTION_AUTO;
}

const char *Xp_text_layout_language(const XpTextLayout *layout)
{
    return layout != NULL ? layout->language : "";
}

XpTextStatus Xp_text_layout_measure(const XpTextLayout *layout,
                                    XpTextMetrics *metrics)
{
    if (layout == NULL || metrics == NULL)
        return XP_TEXT_STATUS_INVALID_ARGUMENT;
    *metrics = (XpTextMetrics){20, 10, 8, 12};
    return XP_TEXT_STATUS_OK;
}

XpTextStatus Xp_text_layout_rasterize(const XpTextLayout *layout,
                                      XpTextBitmap *bitmap)
{
    if (layout == NULL || bitmap == NULL || bitmap->pixels != NULL)
        return XP_TEXT_STATUS_INVALID_ARGUMENT;
    bitmap->pixels = malloc(800);
    if (bitmap->pixels == NULL)
        return XP_TEXT_STATUS_OUT_OF_MEMORY;
    memset(bitmap->pixels, 255, 800);
    bitmap->width = 20;
    bitmap->height = 10;
    bitmap->pitch = 80;
    bitmap->scale = 1;
    if (ink_bitmap) { bitmap->width=24; bitmap->height=8; bitmap->pitch=96; bitmap->scale=2; bitmap->offset_x=-2; bitmap->offset_y=1; }
    rasterize_calls++;
    return XP_TEXT_STATUS_OK;
}

void Xp_text_bitmap_destroy(XpTextBitmap *bitmap)
{
    if (bitmap == NULL)
        return;
    free(bitmap->pixels);
    memset(bitmap, 0, sizeof(*bitmap));
}

static int check_utf8_layout_is_cached_and_drawn(void)
{
    static const char utf8[] = "A\xe3\x81\x91";
    const XpTextLayoutRequest request = {
        utf8, sizeof(utf8) - 1, XP_TEXT_DIRECTION_LTR, "ja"
    };
    const RendererColor color = {1, 2, 3, 4};
    UnicodeTextRenderer *renderer = NULL;
    XpTextMetrics metrics;

    reset_fixture();
    TEST_CHECK(Unicode_text_renderer_create(&sdl_renderer, &font, &renderer)
               == RENDERER_STATUS_OK);
    begin_frame();
    TEST_CHECK(Unicode_text_renderer_measure(renderer, &request, &metrics)
               == RENDERER_STATUS_OK);
    TEST_CHECK(metrics.width == 20 && metrics.height == 10);
    TEST_CHECK(layout_create_calls == 1);
    TEST_CHECK(captured_direction == XP_TEXT_DIRECTION_LTR);
    TEST_CHECK(strcmp(captured_language, "ja") == 0);

    TEST_CHECK(Unicode_text_renderer_draw(
                   renderer, &request, (RendererPoint2D){100.0f, 50.0f},
                   TEXT_GEOMETRY_ALIGN_CENTER, TEXT_GEOMETRY_ALIGN_BOTTOM,
                   color, TEXT_RENDERER_SPACE_WORLD)
               == RENDERER_STATUS_OK);
    TEST_CHECK(layout_create_calls == 1);
    TEST_CHECK(rasterize_calls == 1);
    TEST_CHECK(texture_create_calls == 1);
    TEST_CHECK(draw_calls == 1);
    TEST_CHECK(flush_calls == 2);
    TEST_CHECK(float_equal(captured_vertices[0].x, 90.0f));
    TEST_CHECK(float_equal(captured_vertices[0].y, 60.0f));
    TEST_CHECK(float_equal(captured_vertices[2].x, 110.0f));
    TEST_CHECK(float_equal(captured_vertices[2].y, 50.0f));
    TEST_CHECK(float_equal(captured_vertices[5].x, 90.0f));
    TEST_CHECK(float_equal(captured_vertices[5].y, 50.0f));
    TEST_CHECK(captured_vertices[0].color.red == 1);
    TEST_CHECK(captured_vertices[0].color.alpha == 4);

    TEST_CHECK(Unicode_text_renderer_draw(
                   renderer, &request, (RendererPoint2D){0.0f, 0.0f},
                   TEXT_GEOMETRY_ALIGN_LEFT, TEXT_GEOMETRY_ALIGN_TOP,
                   color, TEXT_RENDERER_SPACE_HUD)
               == RENDERER_STATUS_OK);
    TEST_CHECK(layout_create_calls == 1);
    TEST_CHECK(rasterize_calls == 1);
    TEST_CHECK(texture_create_calls == 1);
    TEST_CHECK(draw_calls == 2);

    frontend.frame_active = 0;
    TEST_CHECK(Unicode_text_renderer_destroy(&renderer)
               == RENDERER_STATUS_OK);
    TEST_CHECK(renderer == NULL);
    TEST_CHECK(texture_destroy_calls == 1);
    TEST_CHECK(layout_destroy_calls == 1);
    return 0;
}

static int check_rtl_is_forwarded_and_rejected(void)
{
    const XpTextLayoutRequest request = {
        "abc", 3, XP_TEXT_DIRECTION_RTL, "ar"
    };
    UnicodeTextRenderer *renderer = NULL;
    XpTextMetrics metrics = {91, 92, 93, 94};
    XpTextMetrics unchanged = metrics;

    reset_fixture();
    TEST_CHECK(Unicode_text_renderer_create(&sdl_renderer, &font, &renderer)
               == RENDERER_STATUS_OK);
    begin_frame();
    TEST_CHECK(Unicode_text_renderer_measure(renderer, &request, &metrics)
               == RENDERER_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(memcmp(&metrics, &unchanged, sizeof(metrics)) == 0);
    TEST_CHECK(sdl_renderer.frame_result == RENDERER_STATUS_INVALID_ARGUMENT);
    TEST_CHECK(layout_create_calls == 0);
    frontend.frame_active = 0;
    TEST_CHECK(Unicode_text_renderer_destroy(&renderer)
               == RENDERER_STATUS_OK);
    return 0;
}

static int check_bitmap_ink_is_independent_of_layout(void)
{
    UnicodeTextRenderer *renderer = NULL;
    const XpTextLayoutRequest request = {"fi", 2, XP_TEXT_DIRECTION_LTR, "en"};
    reset_fixture();
    ink_bitmap = 1;
    TEST_CHECK(Unicode_text_renderer_create(&sdl_renderer, &font, &renderer) == RENDERER_STATUS_OK);
    begin_frame();
    TEST_CHECK(Unicode_text_renderer_draw(renderer, &request,
        (RendererPoint2D){100, 50}, TEXT_GEOMETRY_ALIGN_CENTER,
        TEXT_GEOMETRY_ALIGN_BOTTOM, (RendererColor){255,255,255,255},
        TEXT_RENDERER_SPACE_HUD) == RENDERER_STATUS_OK);
    TEST_CHECK(float_equal(captured_vertices[0].x, 88));
    TEST_CHECK(float_equal(captured_vertices[0].y, 41));
    TEST_CHECK(float_equal(captured_vertices[2].x, 100));
    TEST_CHECK(float_equal(captured_vertices[2].y, 45));
    frontend.frame_active = 0;
    TEST_CHECK(Unicode_text_renderer_clear(renderer) == RENDERER_STATUS_OK);
    begin_frame();
    TEST_CHECK(Unicode_text_renderer_draw(renderer, &request,
        (RendererPoint2D){100, 50}, TEXT_GEOMETRY_ALIGN_CENTER,
        TEXT_GEOMETRY_ALIGN_BOTTOM, (RendererColor){255,255,255,255},
        TEXT_RENDERER_SPACE_HUD) == RENDERER_STATUS_OK);
    TEST_CHECK(rasterize_calls == 2 && texture_create_calls == 2);
    frontend.frame_active = 0;
    TEST_CHECK(Unicode_text_renderer_destroy(&renderer) == RENDERER_STATUS_OK);
    return 0;
}

int main(void)
{
    if (check_bitmap_ink_is_independent_of_layout() != 0) return 1;
    if (check_utf8_layout_is_cached_and_drawn() != 0)
        return 1;
    if (check_rtl_is_forwarded_and_rejected() != 0)
        return 1;
    return 0;
}
