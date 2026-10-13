/* SDL window and OpenGL core renderer coordination. */

#include "sdlrenderer.h"

#include "renderer_gl_core.h"
#include "renderer_gl_es.h"

#include <stdint.h>
#include <stdlib.h>

struct SdlRenderer {
    SDL_Window *window;
    Renderer *frontend;
    int logical_width;
    int logical_height;
    int drawable_width;
    int drawable_height;
    int frame_active;
    RendererStatus frame_result;
};

static void *Sdl_renderer_load_proc(void *userdata, const char *name)
{
    (void)userdata;
    return SDL_GL_GetProcAddress(name);
}

RendererStatus Sdl_renderer_create(SDL_Window *window,
                                   SdlRenderer **renderer)
{
    SdlRenderer *created;
    RendererStatus status;

    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    *renderer = NULL;
    if (window == NULL || SDL_GL_GetCurrentContext() == NULL
        || SDL_GL_GetCurrentWindow() != window) {
        return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    created = calloc(1, sizeof(*created));
    if (created == NULL)
        return RENDERER_STATUS_OUT_OF_MEMORY;
    created->window = window;
#ifdef __EMSCRIPTEN__
    status = Renderer_gl_es_create(
#else
    status = Renderer_gl_core_create(
#endif
                                     Sdl_renderer_load_proc, NULL,
                                     &created->frontend);
    if (status != RENDERER_STATUS_OK) {
        free(created);
        return status;
    }

    *renderer = created;
    return RENDERER_STATUS_OK;
}

void Sdl_renderer_destroy(SdlRenderer *renderer)
{
    if (renderer == NULL)
        return;
    Renderer_destroy(renderer->frontend);
    free(renderer);
}

Renderer *Sdl_renderer_frontend(SdlRenderer *renderer)
{
    return renderer != NULL ? renderer->frontend : NULL;
}

RendererStatus Sdl_renderer_track_frame_result(
    SdlRenderer *renderer, RendererStatus status)
{
    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!renderer->frame_active)
        return RENDERER_STATUS_INVALID_STATE;
    if (renderer->frame_result == RENDERER_STATUS_OK
        && status != RENDERER_STATUS_OK) {
        renderer->frame_result = status;
    }
    return renderer->frame_result;
}

RendererStatus Sdl_renderer_frame_result(const SdlRenderer *renderer)
{
    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    return renderer->frame_result;
}

RendererStatus Sdl_renderer_get_drawable_size(SdlRenderer *renderer,
                                              int *width, int *height)
{
    int drawable_width;
    int drawable_height;

    if (renderer == NULL || width == NULL || height == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!SDL_GetWindowSizeInPixels(renderer->window, &drawable_width,
                                   &drawable_height)
        || drawable_width <= 0 || drawable_height <= 0) {
        return RENDERER_STATUS_BACKEND_ERROR;
    }
    *width = drawable_width;
    *height = drawable_height;
    return RENDERER_STATUS_OK;
}

RendererStatus Sdl_renderer_set_transform_2d(
    SdlRenderer *renderer, RendererTransform2D transform)
{
    RendererStatus status;

    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    status = Sdl_renderer_track_frame_result(renderer, RENDERER_STATUS_OK);
    if (status != RENDERER_STATUS_OK)
        return status;
    status = Renderer_set_transform_2d(renderer->frontend, transform);
    return Sdl_renderer_track_frame_result(renderer, status);
}

RendererStatus Sdl_renderer_begin_frame(SdlRenderer *renderer,
                                        int logical_width,
                                        int logical_height,
                                        RendererColor clear_color)
{
    RendererStatus status;
    int drawable_width;
    int drawable_height;

    if (renderer == NULL || logical_width <= 0 || logical_height <= 0)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (renderer->frame_active)
        return RENDERER_STATUS_INVALID_STATE;
    status = Sdl_renderer_get_drawable_size(renderer, &drawable_width,
                                            &drawable_height);
    if (status != RENDERER_STATUS_OK)
        return status;
    status = Renderer_begin_frame(renderer->frontend, drawable_width,
                                  drawable_height, clear_color);
    if (status != RENDERER_STATUS_OK)
        return status;

    renderer->logical_width = logical_width;
    renderer->logical_height = logical_height;
    renderer->drawable_width = drawable_width;
    renderer->drawable_height = drawable_height;
    renderer->frame_active = 1;
    renderer->frame_result = RENDERER_STATUS_OK;
    return RENDERER_STATUS_OK;
}

static int64_t Sdl_renderer_clamp_logical_edge(int64_t edge, int limit)
{
    if (edge < 0)
        return 0;
    if (edge > limit)
        return limit;
    return edge;
}

static int Sdl_renderer_scale_edge_floor(int64_t logical_edge,
                                         int logical_size,
                                         int drawable_size)
{
    return (int)((logical_edge * (int64_t)drawable_size)
                 / logical_size);
}

static int Sdl_renderer_scale_edge_ceil(int64_t logical_edge,
                                        int logical_size,
                                        int drawable_size)
{
    int64_t numerator = logical_edge * (int64_t)drawable_size;

    return (int)((numerator + logical_size - 1) / logical_size);
}

RendererStatus Sdl_renderer_set_logical_scissor(
    SdlRenderer *renderer, const RendererRect *scissor)
{
    RendererRect drawable_scissor;
    int64_t logical_left;
    int64_t logical_top;
    int64_t logical_right;
    int64_t logical_bottom;
    int drawable_left;
    int drawable_top;
    int drawable_right;
    int drawable_bottom;

    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!renderer->frame_active)
        return RENDERER_STATUS_INVALID_STATE;
    if (scissor == NULL)
        return Renderer_set_scissor(renderer->frontend, NULL);
    if (scissor->width <= 0 || scissor->height <= 0)
        return RENDERER_STATUS_INVALID_ARGUMENT;

    logical_left = Sdl_renderer_clamp_logical_edge(
        scissor->x, renderer->logical_width);
    logical_top = Sdl_renderer_clamp_logical_edge(
        scissor->y, renderer->logical_height);
    logical_right = Sdl_renderer_clamp_logical_edge(
        (int64_t)scissor->x + scissor->width,
        renderer->logical_width);
    logical_bottom = Sdl_renderer_clamp_logical_edge(
        (int64_t)scissor->y + scissor->height,
        renderer->logical_height);

    if (logical_right <= logical_left || logical_bottom <= logical_top) {
        drawable_scissor.x = renderer->drawable_width;
        drawable_scissor.y = renderer->drawable_height;
        drawable_scissor.width = 1;
        drawable_scissor.height = 1;
        return Renderer_set_scissor(renderer->frontend,
                                    &drawable_scissor);
    }

    /* Clamping bounds every factor by INT_MAX, so the int64_t products and
     * the ceil adjustment cannot overflow. */
    drawable_left = Sdl_renderer_scale_edge_floor(
        logical_left, renderer->logical_width,
        renderer->drawable_width);
    drawable_top = Sdl_renderer_scale_edge_floor(
        logical_top, renderer->logical_height,
        renderer->drawable_height);
    drawable_right = Sdl_renderer_scale_edge_ceil(
        logical_right, renderer->logical_width,
        renderer->drawable_width);
    drawable_bottom = Sdl_renderer_scale_edge_ceil(
        logical_bottom, renderer->logical_height,
        renderer->drawable_height);
    drawable_scissor.x = drawable_left;
    drawable_scissor.y = drawable_top;
    drawable_scissor.width = drawable_right - drawable_left;
    drawable_scissor.height = drawable_bottom - drawable_top;
    return Renderer_set_scissor(renderer->frontend, &drawable_scissor);
}

RendererStatus Sdl_renderer_flush(SdlRenderer *renderer)
{
    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!renderer->frame_active)
        return RENDERER_STATUS_INVALID_STATE;
    return Renderer_flush(renderer->frontend);
}

RendererStatus Sdl_renderer_end_frame(SdlRenderer *renderer)
{
    RendererStatus status;

    if (renderer == NULL)
        return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!renderer->frame_active)
        return RENDERER_STATUS_INVALID_STATE;
    status = Renderer_end_frame(renderer->frontend);
    if (status == RENDERER_STATUS_OK) {
        renderer->logical_width = 0;
        renderer->logical_height = 0;
        renderer->drawable_width = 0;
        renderer->drawable_height = 0;
        renderer->frame_active = 0;
    }
    return status;
}
