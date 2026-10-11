#include "driver.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>

#include <stdio.h>
#include <string.h>

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, \
                    #expression); \
            goto failure; \
        } \
    } while (0)

int main(void)
{
    static const uint8_t white_rgba[] = {
        255, 255, 255, 255, 255, 255, 255, 128,
        255, 255, 255, 128, 255, 255, 255, 255
    };
    SDL_Event event;
    uint8_t scaled_rgba[4 * 4 * 4];
    uint8_t outside_pixel[4];
    uint8_t blended_pixel[4];
    float initial_x;

    CHECK(xp_browser_init(800, 600) == 0);
    CHECK(xp_browser_upload_text(0, white_rgba, 2, 2, 8,
                                 5.0f, 5.0f, -1.0f, -1.0f) == 0);
    CHECK(xp_browser_upload_text(1, white_rgba, 2, 2, 8,
                                 5.0f, 5.0f, -1.0f, -1.0f) == 0);
    CHECK(xp_browser_upload_text(2, white_rgba, 2, 2, 8,
                                 5.0f, 5.0f, 0.0f, 0.0f) != 0);
    CHECK(xp_browser_upload_text(0, white_rgba, 2, 2, 8,
                                 0.0f, 2.0f, 0.0f, 0.0f) != 0);
    memset(scaled_rgba, 255, sizeof(scaled_rgba));
    CHECK(xp_browser_upload_text(0, scaled_rgba, 4, 4, 16,
                                 2.0f, 2.0f, -1.0f, -1.0f) == 0);
    CHECK(xp_browser_frame(0.016) == 0);
    glReadPixels(26, 600 - 1 - 57, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 outside_pixel);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(outside_pixel[0] == 10 && outside_pixel[1] == 20
          && outside_pixel[2] == 40);
    glReadPixels(24, 600 - 1 - 111, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 blended_pixel);
    CHECK(glGetError() == GL_NO_ERROR);
    CHECK(blended_pixel[0] > 10 && blended_pixel[0] < 255);
    CHECK(blended_pixel[1] > 20 && blended_pixel[1] < 255);
    CHECK(blended_pixel[2] > 40 && blended_pixel[2] < 255);
    initial_x = xp_browser_scene_x();

    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_RIGHT;
    CHECK(SDL_PushEvent(&event));
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_SCANCODE_RIGHT;
    CHECK(SDL_PushEvent(&event));
    CHECK(xp_browser_frame(0.016) == 0);
    CHECK(xp_browser_scene_x() >= initial_x + 4.0f);
    CHECK((xp_browser_key_mask() & XP_BROWSER_KEY_RIGHT) == 0);

    CHECK(xp_browser_resize(640, 480) == 0);
    CHECK(xp_browser_frame(0.016) == 0);
    xp_browser_shutdown();
    return 0;

failure:
    xp_browser_shutdown();
    return 1;
}
