/* Small browser driver for the XPilot triangle renderer. */

#include "driver.h"

#include "renderer.h"
#include "renderer_gl_es.h"

#include <SDL3/SDL.h>

#include <math.h>
#include <stddef.h>

typedef struct BrowserText {
    RendererTexture *texture;
    float logical_width;
    float logical_height;
    float offset_x;
    float offset_y;
} BrowserText;

typedef struct BrowserDriver {
    SDL_Window *window;
    SDL_GLContext context;
    Renderer *renderer;
    RendererTexture *checker;
    BrowserText text[2];
    int initialized;
    int logical_width;
    int logical_height;
    unsigned int keys;
    float pointer_x;
    float pointer_y;
    float scene_x;
    float scene_y;
} BrowserDriver;

static BrowserDriver browser;

static void *Browser_load_gl_proc(void *userdata, const char *name)
{
    (void)userdata;
    return SDL_GL_GetProcAddress(name);
}

static unsigned int Browser_key_bit(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_LEFT:
        return XP_BROWSER_KEY_LEFT;
    case SDL_SCANCODE_RIGHT:
        return XP_BROWSER_KEY_RIGHT;
    case SDL_SCANCODE_UP:
        return XP_BROWSER_KEY_UP;
    case SDL_SCANCODE_DOWN:
        return XP_BROWSER_KEY_DOWN;
    default:
        return 0;
    }
}

static float Browser_clamp(float value, float maximum)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > maximum)
        return maximum;
    return value;
}

int xp_browser_init(int width, int height)
{
    static const uint8_t checker_pixels[] = {
        255, 192, 36, 255, 32, 125, 233, 255,
         32, 125, 233, 255, 255, 192, 36, 255
    };
    const RendererTextureDesc checker_desc = {
        2, 2, RENDERER_TEXTURE_FILTER_NEAREST,
        RENDERER_TEXTURE_WRAP_CLAMP
    };

    if (browser.initialized || width <= 0 || height <= 0)
        return 1;
    /* The SDL Emscripten backend binds events before window creation. */
    if (!SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#game")
        || !SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#game")
        || !SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0")) {
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 1;
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)
        || !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                SDL_GL_CONTEXT_PROFILE_ES)
        || !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
        goto failure;
    }
    browser.window = SDL_CreateWindow(
        "XPilot Infinity WebGL driver", width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (browser.window == NULL)
        goto failure;
    browser.context = SDL_GL_CreateContext(browser.window);
    if (browser.context == NULL
        || !SDL_GL_MakeCurrent(browser.window, browser.context)) {
        goto failure;
    }
    if (Renderer_gl_es_create(Browser_load_gl_proc, NULL,
                              &browser.renderer) != RENDERER_STATUS_OK) {
        goto failure;
    }
    if (Renderer_texture_create_with_desc(
            browser.renderer, &checker_desc, checker_pixels, 8,
            &browser.checker) != RENDERER_STATUS_OK) {
        goto failure;
    }
    browser.logical_width = width;
    browser.logical_height = height;
    browser.pointer_x = (float)width * 0.5f;
    browser.pointer_y = (float)height * 0.5f;
    browser.scene_x = (float)width * 0.5f;
    browser.scene_y = (float)height * 0.5f;
    browser.initialized = 1;
    return 0;

failure:
    xp_browser_shutdown();
    return 1;
}

int xp_browser_resize(int width, int height)
{
    if (!browser.initialized || width <= 0 || height <= 0)
        return 1;
    if (!SDL_SetWindowSize(browser.window, width, height))
        return 1;
    browser.logical_width = width;
    browser.logical_height = height;
    browser.scene_x = Browser_clamp(browser.scene_x, (float)width);
    browser.scene_y = Browser_clamp(browser.scene_y, (float)height);
    return 0;
}

int xp_browser_upload_text(int slot, const uint8_t *rgba,
                           int width, int height, int stride,
                           float logical_width, float logical_height,
                           float offset_x, float offset_y)
{
    RendererTextureDesc desc;
    RendererTexture *replacement = NULL;
    BrowserText *text;

    if (!browser.initialized || slot < 0 || slot >= 2 || rgba == NULL
        || width <= 0 || height <= 0 || width > INT32_MAX / 4
        || stride < width * 4 || !isfinite(logical_width)
        || !isfinite(logical_height) || !isfinite(offset_x)
        || !isfinite(offset_y) || logical_width <= 0.0f
        || logical_height <= 0.0f) {
        return 1;
    }
    desc.width = width;
    desc.height = height;
    desc.filter = RENDERER_TEXTURE_FILTER_LINEAR;
    desc.wrap = RENDERER_TEXTURE_WRAP_CLAMP;
    if (Renderer_texture_create_with_desc(
            browser.renderer, &desc, rgba, (size_t)stride,
            &replacement) != RENDERER_STATUS_OK) {
        return 1;
    }
    text = &browser.text[slot];
    if (text->texture != NULL
        && Renderer_texture_destroy(browser.renderer, text->texture)
               != RENDERER_STATUS_OK) {
        Renderer_texture_destroy(browser.renderer, replacement);
        return 1;
    }
    text->texture = replacement;
    text->logical_width = logical_width;
    text->logical_height = logical_height;
    text->offset_x = offset_x;
    text->offset_y = offset_y;
    return 0;
}

static void Browser_poll_input(unsigned int *pressed)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            unsigned int bit = Browser_key_bit(event.key.scancode);

            if ((browser.keys & bit) == 0 && !event.key.repeat)
                *pressed |= bit;
            browser.keys |= bit;
        } else if (event.type == SDL_EVENT_KEY_UP) {
            browser.keys &= ~Browser_key_bit(event.key.scancode);
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            browser.keys = 0;
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            browser.pointer_x = event.motion.x;
            browser.pointer_y = event.motion.y;
        }
    }
}

static RendererStatus Browser_draw_text(float scale_x, float scale_y)
{
    RendererStatus status = Renderer_set_blend(
        browser.renderer, RENDERER_BLEND_ALPHA);
    int slot;

    if (status != RENDERER_STATUS_OK)
        return status;
    for (slot = 0; slot < 2; slot++) {
        BrowserText *text = &browser.text[slot];
        float x = 24.0f;
        float y = 58.0f + (float)slot * 54.0f;
        if (text->texture == NULL)
            continue;
        status = Renderer_fill_rect(
            browser.renderer, x * scale_x,
            (y + 3.0f) * scale_y,
            text->logical_width * scale_x, 2.0f * scale_y,
            (RendererColor){90, 150, 210, 255});
        if (status != RENDERER_STATUS_OK)
            return status;
        status = Renderer_draw_sprite(
            browser.renderer, text->texture,
            (x + text->offset_x) * scale_x,
            (y + text->offset_y) * scale_y,
            (x + text->offset_x + text->logical_width) * scale_x,
            (y + text->offset_y + text->logical_height) * scale_y,
            0.0f, 0.0f, 1.0f, 1.0f,
            (RendererColor){255, 255, 255, 255});
        if (status != RENDERER_STATUS_OK)
            return status;
    }
    return RENDERER_STATUS_OK;
}

int xp_browser_frame(double delta_seconds)
{
    static const RendererVertex2D triangle[] = {
        {0.0f, 0.0f, 0.0f, 0.0f, {255, 82, 108, 255}},
        {64.0f, 0.0f, 0.0f, 0.0f, {255, 82, 108, 255}},
        {32.0f, 48.0f, 0.0f, 0.0f, {255, 82, 108, 255}}
    };
    RendererVertex2D scaled_triangle[3];
    unsigned int pressed = 0;
    float scale_x;
    float scale_y;
    float speed;
    int pixel_width;
    int pixel_height;
    int index;
    RendererStatus status;

    if (!browser.initialized || !isfinite(delta_seconds)
        || delta_seconds < 0.0) {
        return 1;
    }
    Browser_poll_input(&pressed);
    speed = (float)(delta_seconds > 0.1 ? 0.1 : delta_seconds) * 180.0f;
    if (browser.keys & XP_BROWSER_KEY_LEFT)
        browser.scene_x -= speed;
    if (browser.keys & XP_BROWSER_KEY_RIGHT)
        browser.scene_x += speed;
    if (browser.keys & XP_BROWSER_KEY_UP)
        browser.scene_y -= speed;
    if (browser.keys & XP_BROWSER_KEY_DOWN)
        browser.scene_y += speed;
    if (pressed & XP_BROWSER_KEY_LEFT)
        browser.scene_x -= 4.0f;
    if (pressed & XP_BROWSER_KEY_RIGHT)
        browser.scene_x += 4.0f;
    if (pressed & XP_BROWSER_KEY_UP)
        browser.scene_y -= 4.0f;
    if (pressed & XP_BROWSER_KEY_DOWN)
        browser.scene_y += 4.0f;
    browser.scene_x = Browser_clamp(
        browser.scene_x, (float)browser.logical_width);
    browser.scene_y = Browser_clamp(
        browser.scene_y, (float)browser.logical_height);

    if (!SDL_GetWindowSizeInPixels(
            browser.window, &pixel_width, &pixel_height)
        || pixel_width <= 0 || pixel_height <= 0
        || !SDL_GL_MakeCurrent(browser.window, browser.context)) {
        return 1;
    }
    scale_x = (float)pixel_width / (float)browser.logical_width;
    scale_y = (float)pixel_height / (float)browser.logical_height;
    for (index = 0; index < 3; index++) {
        scaled_triangle[index] = triangle[index];
        scaled_triangle[index].x = (triangle[index].x + 180.0f) * scale_x;
        scaled_triangle[index].y = (triangle[index].y + 165.0f) * scale_y;
    }

    status = Renderer_begin_frame(browser.renderer, pixel_width,
                                  pixel_height,
                                  (RendererColor){10, 20, 40, 255});
    if (status != RENDERER_STATUS_OK)
        return 1;
    status = Renderer_fill_rect(
        browser.renderer, 20.0f * scale_x, 150.0f * scale_y,
        120.0f * scale_x, 80.0f * scale_y,
        (RendererColor){35, 135, 105, 255});
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_draw_triangles(
            browser.renderer, NULL, scaled_triangle, 3);
    }
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_draw_sprite(
            browser.renderer, browser.checker,
            300.0f * scale_x, 155.0f * scale_y,
            390.0f * scale_x, 245.0f * scale_y,
            0.0f, 0.0f, 1.0f, 1.0f,
            (RendererColor){255, 255, 255, 255});
    }
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_fill_rect(
            browser.renderer,
            (browser.scene_x - 8.0f) * scale_x,
            (browser.scene_y - 8.0f) * scale_y,
            16.0f * scale_x, 16.0f * scale_y,
            (RendererColor){255, 64, 160, 255});
    }
    if (status == RENDERER_STATUS_OK) {
        status = Renderer_fill_rect(
            browser.renderer,
            (browser.pointer_x - 2.0f) * scale_x,
            (browser.pointer_y - 2.0f) * scale_y,
            4.0f * scale_x, 4.0f * scale_y,
            (RendererColor){130, 235, 255, 255});
    }
    if (status == RENDERER_STATUS_OK)
        status = Browser_draw_text(scale_x, scale_y);
    if (status == RENDERER_STATUS_OK)
        status = Renderer_end_frame(browser.renderer);
    if (status != RENDERER_STATUS_OK)
        return 1;
    return SDL_GL_SwapWindow(browser.window) ? 0 : 1;
}

void xp_browser_shutdown(void)
{
    if (browser.renderer != NULL && browser.context != NULL)
        SDL_GL_MakeCurrent(browser.window, browser.context);
    Renderer_destroy(browser.renderer);
    if (browser.context != NULL)
        SDL_GL_DestroyContext(browser.context);
    if (browser.window != NULL)
        SDL_DestroyWindow(browser.window);
    if (SDL_WasInit(SDL_INIT_VIDEO))
        SDL_Quit();
    browser = (BrowserDriver){0};
}

unsigned int xp_browser_key_mask(void)
{
    return browser.keys;
}

float xp_browser_pointer_x(void)
{
    return browser.pointer_x;
}

float xp_browser_pointer_y(void)
{
    return browser.pointer_y;
}

float xp_browser_scene_x(void)
{
    return browser.scene_x;
}

float xp_browser_scene_y(void)
{
    return browser.scene_y;
}
