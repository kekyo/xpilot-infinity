/*
 * XPilot Infinity/SDL, an SDL/OpenGL XPilot client. Copyright (C) 2003-2004 by
 *
 *      Juha Lindström       <juhal@users.sourceforge.net>
 *      Erik Andersson       <deity_at_home.se>
 *      Darel Cullen         <darelcullen@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xpclient_sdl.h"

#include "text.h"
#include "console.h"
#include "sdlkeys.h"
#include "glwidgets.h"
#include "sdlpaint.h"
#include "sdlinit.h"
#include "gl_diagnostics.h"
#include "sdlrenderer.h"
#include "images.h"
#include "transport_display.h"
#include "native_text.h"
#include "text_config.h"

/* These are only needed for the polygon tessellation */
/* I'd like to move them to Paint_init/cleanup but because it */
/* is called before the map is ready I need separate functions */
/* for now.. */
extern int Gui_init(void);
extern void Gui_cleanup(void);

static SDL_Window *main_window;
static SDL_GLContext main_gl_context;
static SdlRenderer *main_renderer;
static XpTextSystem *text_system;
static bool sdl_initialized;
static bool ttf_initialized;
static bool cleanup_registered;
static bool playing_windows_initialized;
static bool fullscreen;
static int windowed_width;
static int windowed_height;
static bool gamefont_initialized;
static bool mapfont_initialized;

font_data gamefont;
font_data mapfont;
int gameFontSize;
int mapFontSize;
char *gamefontname;

static void set_gameplay_window_title(void)
{
    char title[sizeof(TITLE) + MAX_CHARS + 32];

    if (!Transport_display_gameplay_title(
	    title, sizeof(title), TITLE, servername,
	    connectParam.game_transport)) {
	warn("Could not format the gameplay window title");
	return;
    }
    if (!SDL_SetWindowTitle(main_window, title)) {
	warn("Could not set the SDL gameplay window title: %s",
	     SDL_GetError());
    }
}

/* ugly kps hack */
static bool file_exists(const char *path) 
{ 
  FILE *fp;

  if (!path) {
    return false; 
  } else {
    fp = fopen(path ? path : "", "r");
    if (fp) { 
      fclose(fp); 
      return true;
    }
    return false; 
  }
}

int Init_playing_windows(void)
{
    SdlRenderer *sdl_renderer = Get_sdl_renderer();

    if (sdl_renderer == NULL
	|| Images_prepare(Sdl_renderer_frontend(sdl_renderer)) != 0) {
	error("image preparation failed");
	return -1;
    }
    set_gameplay_window_title();

    /*
    sdl_init_colors();
    Init_spark_colors();
    */
    if ( !AppendGLWidgetList(&MainWidget,Init_MainWidget(&gamefont)) ) {
	error("widget initialization failed");
	return -1;
    }
    if (Console_init(main_window)) {
	error("console initialization failed");
	Close_Widget(&MainWidget);
	return -1;
    }
    if (Gui_init()) {
	error("gui initialization failed");
	Console_cleanup();
	Close_Widget(&MainWidget);
	return -1;
    }

    playing_windows_initialized = true;

    return 0;
}

static void cleanup_window_system(void)
{
    if (gamefont_initialized || mapfont_initialized) {
	error("Refusing to destroy the window system while fonts remain initialized");
	return;
    }
    if (main_renderer != NULL) {
	Sdl_renderer_destroy(main_renderer);
	main_renderer = NULL;
    }
    if (main_gl_context != NULL) {
	SDL_GL_DestroyContext(main_gl_context);
	main_gl_context = NULL;
    }
    if (main_window != NULL) {
	SDL_DestroyWindow(main_window);
	main_window = NULL;
    }
    if (ttf_initialized) {
	Xp_text_system_destroy(&text_system);
	TTF_Quit();
	ttf_initialized = false;
    }
    if (sdl_initialized) {
	SDL_Quit();
	sdl_initialized = false;
    }
    fullscreen = false;
}

static void apply_window_size(int width, int height)
{
    SDL_Rect bounds = {0, 0, 0, 0};

    if (width <= 0 || height <= 0)
	return;

    bounds.w = draw_width = width;
    bounds.h = draw_height = height;
    if (MainWidget != NULL)
	SetBounds_GLWidget(MainWidget, &bounds);

}

static bool cleanup_fonts(void)
{
    bool cleaned = true;
    RendererStatus status;

    if (mapfont_initialized) {
	status = fontclean(&mapfont);
	if (status == RENDERER_STATUS_OK) {
	    mapfont_initialized = false;
	} else {
	    error("Could not clean up the map font (%d)", (int)status);
	    cleaned = false;
	}
    }
    if (gamefont_initialized) {
	status = fontclean(&gamefont);
	if (status == RENDERER_STATUS_OK) {
	    gamefont_initialized = false;
	} else {
	    error("Could not clean up the game font (%d)", (int)status);
	    cleaned = false;
	}
    }
    return cleaned;
}

static bool closest_display_mode(int width, int height, SDL_DisplayMode *mode)
{
    SDL_DisplayID display_id;

    display_id = SDL_GetDisplayForWindow(main_window);
    if (display_id == 0)
	return false;

    return SDL_GetClosestFullscreenDisplayMode(display_id, width, height,
					       0.0f, false, mode);
}

int Init_window(void)
{
    int value;
#ifdef __EMSCRIPTEN__
    const int minor_version = 0, context_profile = SDL_GL_CONTEXT_PROFILE_ES;
#else
    const int minor_version = 3, context_profile = SDL_GL_CONTEXT_PROFILE_CORE;
#endif
    RendererStatus renderer_status;
    char defaultfontname[] = CONF_FONTDIR "FreeSansBoldOblique.ttf";
    bool gf_exists = true,df_exists = true,gf_init = false, mf_init = false;

    Conf_print();

#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR, "#game");
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#game");
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        error("failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }
    sdl_initialized = true;
    if (!cleanup_registered) {
	atexit(Platform_specific_cleanup);
	cleanup_registered = true;
    }

    if (!TTF_Init()) {
	error("SDL_ttf initialization failed: %s", SDL_GetError());
	goto fail;
    }
    ttf_initialized = true;
    warn("SDL_ttf initialized.\n");

    num_spark_colors=8;

    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3)) {
	error("Could not request OpenGL context major version 3: %s",
	      SDL_GetError());
	goto fail;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_version)) {
        error("Could not request OpenGL context minor version %d: %s", minor_version, SDL_GetError());
        goto fail;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, context_profile)) {
        error("Could not request an OpenGL profile: %s", SDL_GetError());
        goto fail;
    }
    if (!SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
	error("Could not enable OpenGL double buffering: %s", SDL_GetError());
	goto fail;
    }

    main_window = SDL_CreateWindow(TITLE, draw_width, draw_height,
				   SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
#ifdef __EMSCRIPTEN__
                                   | SDL_WINDOW_HIGH_PIXEL_DENSITY
#endif
                                   );
    if (main_window == NULL) {
	error("Could not create an SDL3 OpenGL window: %s", SDL_GetError());
	goto fail;
    }
    if (!SDL_SetWindowPosition(main_window, SDL_WINDOWPOS_CENTERED,
			       SDL_WINDOWPOS_CENTERED)) {
	warn("Could not center the SDL window: %s", SDL_GetError());
    }

    main_gl_context = SDL_GL_CreateContext(main_window);
    if (main_gl_context == NULL) {
	error("Could not create an OpenGL context: %s", SDL_GetError());
	goto fail;
    }
    Gl_diagnostics_log_context();
    Gl_diagnostics_check("context creation");
    renderer_status = Sdl_renderer_create(main_window, &main_renderer);
    if (renderer_status != RENDERER_STATUS_OK) {
	error("Could not initialize the OpenGL core renderer (%d)",
	      (int)renderer_status);
	goto fail;
    }
    if (Xp_text_system_create(CONF_FONTDIR, &text_system)
	!= XP_TEXT_STATUS_OK) {
	error("Could not initialize system font resolution: %s",
	      SDL_GetError());
	goto fail;
    }
    SDL_StopTextInput(main_window);
    windowed_width = draw_width;
    windowed_height = draw_height;

    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &value);
    printf("RGB bpp %d/", value);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE,&value);
    printf("%d/", value);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &value);
    printf("%d ", value);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &value);
    printf("Bit Depth is %d\n",value);

    /* this prevents a freetype crash if you pass non existant fonts */
    if (!file_exists(gamefontname)) {
    	error("cannot find your game font '%s'.\n" \
            "Please check that it exists!",gamefontname);
    	xpprintf("Reverting to defaultfont '%s'\n",defaultfontname);
    	gf_exists = false;
    }
    if (!file_exists(defaultfontname)) {
    	error("cannot find the default font! '%s'" ,defaultfontname);
	df_exists = false;
    }
    
    if (!gf_exists && !df_exists) {
    	error("Failed to find any font files!\n" \
	     "Probably you forgot to run 'make install',use '-TTFont <font.ttf>' argument" \
		" until you do");
	goto fail;
    }
      
    if (gf_exists) {
	renderer_status = fontinit(&gamefont,
				   Sdl_renderer_frontend(main_renderer),
				   gamefontname, gameFontSize);
	if (renderer_status != RENDERER_STATUS_OK) {
	    error("Font initialization failed with %s", gamefontname);
	} else gf_init = true;
    }
    if (!gf_init && df_exists) {
	renderer_status = fontinit(&gamefont,
				   Sdl_renderer_frontend(main_renderer),
				   defaultfontname, gameFontSize);
	if (renderer_status != RENDERER_STATUS_OK) {
	    error("Default font initialization failed with %s", defaultfontname);
	} else gf_init = true;
    }
    
    if (!gf_init) {
    	error("Failed to initialize any game font! (quitting)");
	goto fail;
    }
    gamefont_initialized = true;
    
    if (gf_exists) {
	renderer_status = fontinit(&mapfont,
				   Sdl_renderer_frontend(main_renderer),
				   gamefontname, mapFontSize);
	if (renderer_status != RENDERER_STATUS_OK) {
	    error("Font initialization failed with %s", gamefontname);
	} else mf_init = true;
    }
    if (!mf_init && df_exists) {
	renderer_status = fontinit(&mapfont,
				   Sdl_renderer_frontend(main_renderer),
				   defaultfontname, mapFontSize);
	if (renderer_status != RENDERER_STATUS_OK) {
	    error("Default font initialization failed with %s", defaultfontname);
	} else mf_init = true;
    }

    if (!mf_init) {
    	error("Failed to initialize any map font! (quitting)");
	goto fail;
    }
    mapfont_initialized = true;

    renderer_status = font_text_renderer_attach(&gamefont, main_renderer);
    if (renderer_status == RENDERER_STATUS_OK) {
	renderer_status = font_text_renderer_attach(&mapfont, main_renderer);
    }
    if (renderer_status == RENDERER_STATUS_OK) {
	renderer_status = font_unicode_renderer_attach(
	    &gamefont, text_system, main_renderer,
	    Xp_text_config_families(XP_TEXT_SPACING_PROPORTIONAL));
    }
    if (renderer_status == RENDERER_STATUS_OK) {
	renderer_status = font_unicode_renderer_attach(
	    &mapfont, text_system, main_renderer,
	    Xp_text_config_families(XP_TEXT_SPACING_PROPORTIONAL));
    }
    if (renderer_status != RENDERER_STATUS_OK
	|| gamefont.text_renderer == NULL || mapfont.text_renderer == NULL
	|| gamefont.unicode_renderer == NULL
	|| mapfont.unicode_renderer == NULL) {
	error("Font text renderer initialization failed (%d)",
	      (int)renderer_status);
	goto fail;
    }
    printf("Font text renderers ready: game=renderer map=renderer\n");
    fflush(stdout);

    return 0;

fail:
    mapfont_initialized = mf_init;
    gamefont_initialized = gf_init;
    if (cleanup_fonts())
	cleanup_window_system();
    return -1;
}

SdlRenderer *Get_sdl_renderer(void)
{
    return main_renderer;
}

XpTextSystem *Get_text_system(void)
{
    return text_system;
}

/* function to reset our viewport after a window resize */
int Resize_Window(int width, int height)
{
    SDL_DisplayMode mode;

    if (main_window == NULL || width <= 0 || height <= 0)
	return -1;

    if (fullscreen) {
	if (!closest_display_mode(width, height, &mode) ||
	    !SDL_SetWindowFullscreenMode(main_window, &mode))
	    return -1;
	width = mode.w;
	height = mode.h;
    } else {
	SDL_SetWindowSize(main_window, width, height);
	SDL_GetWindowSize(main_window, &width, &height);
	windowed_width = width;
	windowed_height = height;
    }
    apply_window_size(width, height);
    return 0;
}

void Window_size_changed(int width, int height)
{
    if (width <= 0 || height <= 0)
	return;

    if (!fullscreen) {
	windowed_width = width;
	windowed_height = height;
    }
    apply_window_size(width, height);
}

void Swap_buffers(void)
{
    if (main_window != NULL) {
	SDL_GL_SwapWindow(main_window);
        Gl_diagnostics_check("buffer swap");
    }
}

void Set_window_grab(bool on)
{
    if (main_window != NULL)
	SDL_SetWindowMouseGrab(main_window, on ? true : false);
}

bool Set_relative_mouse_mode(bool on)
{
    if (main_window == NULL) {
	SDL_SetError("The application window is not initialized");
	return false;
    }
    return SDL_SetWindowRelativeMouseMode(main_window, on);
}

void Toggle_fullscreen(void)
{
    SDL_DisplayMode mode;
    int width, height;

    if (main_window == NULL)
	return;

    if (fullscreen) {
	if (!SDL_SetWindowFullscreen(main_window, false)) {
	    Add_message("Failed to leave fullscreen mode. [*Client reply*]");
	    return;
	}
	fullscreen = false;
	SDL_SetWindowFullscreenMode(main_window, NULL);
	SDL_SetWindowSize(main_window, windowed_width, windowed_height);
    } else {
	SDL_GetWindowSize(main_window, &windowed_width, &windowed_height);
	if (!closest_display_mode(windowed_width, windowed_height, &mode) ||
	    !SDL_SetWindowFullscreenMode(main_window, &mode) ||
	    !SDL_SetWindowFullscreen(main_window, true)) {
	    SDL_SetWindowFullscreen(main_window, false);
	    SDL_SetWindowFullscreenMode(main_window, NULL);
	    SDL_SetWindowSize(main_window, windowed_width, windowed_height);
	    Add_message("Failed to change video mode. [*Client reply*]");
	    return;
	}
	fullscreen = true;
    }

    SDL_GetWindowSize(main_window, &width, &height);
    apply_window_size(width, height);
}

void Platform_specific_cleanup(void)
{
    if (playing_windows_initialized) {
	if (MainWidget != NULL)
	    Close_Widget(&MainWidget);
	Gui_cleanup();
	Console_cleanup();
	playing_windows_initialized = false;
    }
    if (cleanup_fonts())
	cleanup_window_system();
}

static bool Set_geometry(xp_option_t *opt, const char *s)
{
    int w = 0, h = 0;

    if (s[0] == '=') {
	sscanf(s, "%*c%d%*c%d", &w, &h);
    } else {
	sscanf(s, "%d%*c%d", &w, &h);
    }
    if (w == 0 || h == 0) return false;
    if (main_window != NULL) {
	Resize_Window(w, h);
    } else {
	draw_width = w;
	draw_height = h;
    }
    return true;
}

static const char* Get_geometry(xp_option_t *opt)
{
    static char buf[20]; /* should be enough */
    snprintf(buf, 20, "%dx%d", draw_width, draw_height);
    return buf;
}

static bool Set_fontName(xp_option_t *opt, const char *value)
{
    UNUSED_PARAM(opt);
    XFREE(gamefontname);
    gamefontname = xp_safe_strdup(value);

    return true;
}

static const char *Get_fontName(xp_option_t *opt)
{
    UNUSED_PARAM(opt);
    return gamefontname;
}

static xp_option_t sdlinit_options[] = {
    XP_STRING_OPTION(
	"geometry",
	"1280x1024",
	NULL,
	0,
	Set_geometry, NULL, Get_geometry,
	XP_OPTFLAG_DEFAULT,
	"Set the initial window geometry.\n"),
    
     XP_INT_OPTION(
        "gameFontSize",
	16, 12, 32,
	&gameFontSize,
	NULL,
	XP_OPTFLAG_DEFAULT,
	"Height of font used for game strings.\n"),

    XP_INT_OPTION(
        "mapFontSize",
	16, 12, 64,
	&mapFontSize,
	NULL,
	XP_OPTFLAG_DEFAULT,
	"Height of font used for strings painted on the map.\n"),

    XP_STRING_OPTION(
	"TTFont",
	CONF_FONTDIR "FreeSansBoldOblique.ttf",
	NULL, 0,
	Set_fontName, NULL, Get_fontName,
	XP_OPTFLAG_DEFAULT,
	"Set the font to use.\n")
};

void Store_sdlinit_options(void)
{
    STORE_OPTIONS(sdlinit_options);
}
