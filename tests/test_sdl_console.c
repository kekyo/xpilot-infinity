#include "test_helpers.h"

#include "SDL_console.h"
#include "DT_drawtext.h"

#include <string.h>

static BitFont test_font;

/* SDL_console.c owns input editing but also contains its rendering path.  The
 * rendering collaborators are irrelevant to this test and are replaced by
 * inert implementations so the production input code can be linked alone. */
void DT_DrawText(const char *string, SDL_Surface *surface, int font_type,
                 int x, int y)
{
    (void)string;
    (void)surface;
    (void)font_type;
    (void)x;
    (void)y;
}

int DT_LoadFont(const char *bitmap_name, int flags)
{
    (void)bitmap_name;
    (void)flags;
    test_font.CharWidth = 8;
    test_font.CharHeight = 8;
    test_font.FontNumber = 0;
    return 0;
}

int DT_FontHeight(int font_number)
{
    return font_number == 0 ? test_font.CharHeight : 0;
}

int DT_FontWidth(int font_number)
{
    return font_number == 0 ? test_font.CharWidth : 0;
}

BitFont *DT_FontPointer(int font_number)
{
    return font_number == 0 ? &test_font : NULL;
}

int DT_UnloadFont(int font_number)
{
    return font_number == 0 ? 0 : -1;
}

void DT_DestroyDrawText(void)
{
}

void Talk_set_state(bool on)
{
    (void)on;
}

static int send_key(SDL_Keycode key, SDL_Keymod modifiers)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = key;
    event.key.mod = modifiers;
    return CON_Events(&event) == NULL;
}

static int send_key_up(SDL_Keycode key, SDL_Keymod modifiers)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_UP;
    event.key.key = key;
    event.key.mod = modifiers;
    return CON_Events(&event) == NULL;
}

static int send_text(const char *text)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    return CON_Events(&event) == NULL;
}

typedef struct {
    SDL_Surface *output_screen;
    SDL_Surface *console_surface;
    SDL_Surface *input_background;
    SDL_Surface *background_image;
    int disp_x;
    int disp_y;
    int visible_characters;
    int scroll_back;
    int background_x;
    int background_y;
} console_state_t;

static console_state_t capture_console_state(ConsoleInformation *console)
{
    console_state_t state;

    state.output_screen = console->OutputScreen;
    state.console_surface = console->ConsoleSurface;
    state.input_background = console->InputBackground;
    state.background_image = console->BackgroundImage;
    state.disp_x = console->DispX;
    state.disp_y = console->DispY;
    state.visible_characters = console->VChars;
    state.scroll_back = console->ConsoleScrollBack;
    state.background_x = console->BackX;
    state.background_y = console->BackY;
    return state;
}

static int console_state_matches(ConsoleInformation *console,
                                 const console_state_t *state)
{
    return console->OutputScreen == state->output_screen
        && console->ConsoleSurface == state->console_surface
        && console->InputBackground == state->input_background
        && console->BackgroundImage == state->background_image
        && console->DispX == state->disp_x
        && console->DispY == state->disp_y
        && console->VChars == state->visible_characters
        && console->ConsoleScrollBack == state->scroll_back
        && console->BackX == state->background_x
        && console->BackY == state->background_y;
}

int main(void)
{
    ConsoleInformation *console;
    ConsoleInformation *prompt_console;
    ConsoleInformation *zero_sized_console;
    SDL_Surface *output_screen;
    SDL_Surface *small_output_screen;
    SDL_Surface *old_input_background;
    SDL_Rect console_rect;
    SDL_Rect invalid_rect;
    SDL_Rect zero_sized_rect;
    console_state_t saved_state;
    SDL_Event unhandled;
    int saved_font_width;

    output_screen = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_RGBA32);
    TEST_CHECK(output_screen != NULL);
    console_rect.x = 11;
    console_rect.y = 13;
    console_rect.w = 320;
    console_rect.h = 160;

    zero_sized_rect = console_rect;
    zero_sized_rect.w = 0;
    zero_sized_rect.h = 0;
    zero_sized_console = CON_Init("test-font", output_screen, 4,
                                  zero_sized_rect);
    TEST_CHECK(zero_sized_console != NULL);
    TEST_CHECK(zero_sized_console->ConsoleSurface->w == output_screen->w);
    TEST_CHECK(zero_sized_console->ConsoleSurface->h == output_screen->h);
    CON_Destroy(zero_sized_console);

    prompt_console = CON_Init("test-font", output_screen, 4, console_rect);
    TEST_CHECK(prompt_console != NULL);
    CON_SetPrompt(prompt_console, "first> ");
    TEST_CHECK(strcmp(prompt_console->Prompt, "first> ") == 0);
    CON_SetPrompt(prompt_console, "second> ");
    TEST_CHECK(strcmp(prompt_console->Prompt, "second> ") == 0);
    prompt_console->BackgroundImage =
        SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
    TEST_CHECK(prompt_console->BackgroundImage != NULL);
    CON_Free(prompt_console);

    console = CON_Init("test-font", output_screen, 4, console_rect);
    TEST_CHECK(console != NULL);
    console->Visible = CON_OPEN;
    console->CommandScrollBack = -1;
    CON_Topmost(console);

    CON_SetPrompt(console, "long> ");
    char full_command[CON_CHARS_PER_LINE];
    memset(full_command, 'x', sizeof(full_command) - 1);
    full_command[sizeof(full_command) - 1] = '\0';
    TEST_CHECK(send_text(full_command));
    TEST_CHECK(strcmp(console->Command, full_command) == 0);
    TEST_CHECK(send_key(SDLK_HOME, SDL_KMOD_NONE));
    TEST_CHECK(strcmp(console->RCommand, full_command) == 0);
    strcpy(console->BackupCommand, "preserved history");
    DrawCommandLine();
    TEST_CHECK(strlen(console->VCommand) <= CON_CHARS_PER_LINE);
    TEST_CHECK(strncmp(console->VCommand, console->Prompt,
                       strlen(console->Prompt)) == 0);
    TEST_CHECK(strcmp(console->BackupCommand, "preserved history") == 0);
    TEST_CHECK(send_key(SDLK_END, SDL_KMOD_NONE));
    TEST_CHECK(strcmp(console->LCommand, full_command) == 0);
    TEST_CHECK(console->RCommand[0] == '\0');
    Clear_Command(console);

    TEST_CHECK(send_text("abc"));
    TEST_CHECK(strcmp(console->Command, "abc") == 0);

    TEST_CHECK(send_key(SDLK_LEFT, SDL_KMOD_NONE));
    TEST_CHECK(send_key(SDLK_BACKSPACE, SDL_KMOD_NONE));
    TEST_CHECK(strcmp(console->Command, "ac") == 0);
    TEST_CHECK(send_key(SDLK_DELETE, SDL_KMOD_NONE));
    TEST_CHECK(strcmp(console->Command, "a") == 0);

    TEST_CHECK(send_text("Z"));
    TEST_CHECK(send_key(SDLK_HOME, SDL_KMOD_NONE));
    TEST_CHECK(send_text("X"));
    TEST_CHECK(strcmp(console->Command, "XaZ") == 0);
    TEST_CHECK(send_key(SDLK_END, SDL_KMOD_NONE));
    TEST_CHECK(send_key(SDLK_BACKSPACE, SDL_KMOD_NONE));
    TEST_CHECK(strcmp(console->Command, "Xa") == 0);

    TEST_CHECK(send_key(SDLK_U, SDL_KMOD_CTRL));
    TEST_CHECK(console->Command[0] == '\0');

    TEST_CHECK(!send_key(SDLK_V, SDL_KMOD_CTRL));
    TEST_CHECK(send_text("v"));
    TEST_CHECK(console->Command[0] == '\0');
    TEST_CHECK(!send_key_up(SDLK_V, SDL_KMOD_NONE));
    TEST_CHECK(send_text("x"));
    TEST_CHECK(strcmp(console->Command, "x") == 0);
    TEST_CHECK(send_key(SDLK_U, SDL_KMOD_CTRL));
    TEST_CHECK(!send_key_up(SDLK_U, SDL_KMOD_NONE));

    TEST_CHECK(send_text("~\001\303\251Q"));
    TEST_CHECK(strcmp(console->Command, "~Q") == 0);
    Add_String_to_Console("\tOK\377");
    TEST_CHECK(strcmp(console->Command, "~QOK") == 0);

    memset(&unhandled, 0, sizeof(unhandled));
    unhandled.type = SDL_EVENT_KEY_DOWN;
    unhandled.key.key = SDLK_F1;
    TEST_CHECK(CON_Events(&unhandled) == &unhandled);

    console->Visible = CON_CLOSED;
    TEST_CHECK(!send_text("ignored"));
    TEST_CHECK(strcmp(console->Command, "~QOK") == 0);

    console->BackgroundImage =
        SDL_CreateSurface(32, 32, SDL_PIXELFORMAT_RGBA32);
    TEST_CHECK(console->BackgroundImage != NULL);
    console->BackX = 17;
    console->BackY = 19;
    old_input_background = console->InputBackground;
    TEST_CHECK(CON_Background(console, NULL, 0, 0) == 0);
    TEST_CHECK(console->BackgroundImage == NULL);
    TEST_CHECK(console->InputBackground != old_input_background);
    TEST_CHECK(console->BackX == 17);
    TEST_CHECK(console->BackY == 19);

    console->ConsoleScrollBack = 2;
    saved_state = capture_console_state(console);
    invalid_rect.x = 31;
    invalid_rect.y = 37;
    invalid_rect.w = 320;
    invalid_rect.h = 120;
    saved_font_width = console->FontWidth;
    console->FontWidth = 0;
    TEST_CHECK(CON_Resize(console, invalid_rect) == 1);
    console->FontWidth = saved_font_width;
    TEST_CHECK(console_state_matches(console, &saved_state));
    TEST_CHECK(CON_Transfer(console, NULL, console_rect) == 1);
    TEST_CHECK(console_state_matches(console, &saved_state));

    invalid_rect.w = 0;
    invalid_rect.h = 0;
    TEST_CHECK(CON_Resize(console, invalid_rect) == 0);
    TEST_CHECK(console->ConsoleSurface->w == output_screen->w);
    TEST_CHECK(console->ConsoleSurface->h == output_screen->h);

    saved_state = capture_console_state(console);
    TEST_CHECK(CON_Transfer(console, console->ConsoleSurface,
                            console_rect) == 1);
    TEST_CHECK(console_state_matches(console, &saved_state));

    small_output_screen = SDL_CreateSurface(8, 4, SDL_PIXELFORMAT_RGBA32);
    TEST_CHECK(small_output_screen != NULL);
    TEST_CHECK(CON_Transfer(console, small_output_screen, invalid_rect) == 1);
    TEST_CHECK(console_state_matches(console, &saved_state));
    SDL_DestroySurface(small_output_screen);

    CON_Destroy(console);
    TEST_CHECK(!send_text("after-destroy"));
    SDL_DestroySurface(output_screen);

    return 0;
}
