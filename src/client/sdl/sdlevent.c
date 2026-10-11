/*
 * XPilot Infinity/SDL, an SDL/OpenGL XPilot client. Copyright (C) 2003-2004 by
 *
 *     Juha Lindström <juhal@users.sourceforge.net>
 *     Erik Andersson <deity_at_home.se>
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

#include "sdlevent.h"
#include "sdlinit.h"
#include "sdlkeys.h"
#include "console.h"
#include "sdlpaint.h"
#include "glwidgets.h"

#include <limits.h>

/* TODO: remove these from client.h and put them in *event.h */
bool            initialPointerControl = false;

static float	mouseMovement;	/* accumulated SDL3 high-resolution pointer input. */
static float	wheelMovement;	/* accumulated SDL3 high-resolution wheel input. */

GLWidget *clicktarget[NUM_MOUSE_BUTTONS];
GLWidget *hovertarget = NULL;

static void release_input_state(void)
{
    GLWidget *target;
    int button, x, y;
    float mouse_x, mouse_y;

    SDL_GetMouseState(&mouse_x, &mouse_y);
    x = (int)mouse_x;
    y = (int)mouse_y;
    Key_clear_counts();
    mouseMovement = 0.0f;
    wheelMovement = 0.0f;

    for (button = 1; button <= NUM_MOUSE_BUTTONS; button++) {
	Pointer_button_released(button);
	target = clicktarget[button - 1];
	if (target != NULL && target->button != NULL)
	    target->button((Uint8)button, false, x, y,
			   target->buttondata);
	clicktarget[button - 1] = NULL;
    }

    if (hovertarget != NULL && hovertarget->hover != NULL)
	hovertarget->hover(false, x, y, hovertarget->hoverdata);
    hovertarget = NULL;
}

static void process_mouse_button(Uint8 button, bool state, int x, int y)
{
    GLWidget *target;

    if (button < 1 || button > NUM_MOUSE_BUTTONS)
	return;

    if (clData.pointerControl) {
	if (state == true)
	    Pointer_button_pressed(button);
	else
	    Pointer_button_released(button);
	return;
    }

    if (state == true) {
	target = FindGLWidget(MainWidget, x, y);
	clicktarget[button - 1] = target;
	if (target != NULL && target->button != NULL)
	    target->button(button, state, x, y, target->buttondata);
    } else {
	target = clicktarget[button - 1];
	if (target != NULL && target->button != NULL)
	    target->button(button, state, x, y, target->buttondata);
	clicktarget[button - 1] = NULL;
    }
}

/* XPilot's SDL1 bindings reserve buttons 4 and 5 for synthesized wheel
 * events.  SDL3 uses the same values for physical X1 and X2 buttons. */
static bool is_sdl_side_button(Uint8 button)
{
    return button == SDL_BUTTON_X1 || button == SDL_BUTTON_X2;
}

void Platform_specific_pointer_control_set_state(bool on)
{
    assert(clData.pointerControl != on);
#ifdef __EMSCRIPTEN__
    /* The browser can lock the canvas from a separate page button even when
     * SDL's mouse focus still points outside the game window. */
    extern void Browser_pointer_request(int enabled);
    MainWidget_ShowMenu(MainWidget, !on);
    Browser_pointer_request(on);
#else

    if (on) {
    	MainWidget_ShowMenu(MainWidget, false);
	if (!Set_relative_mouse_mode(true))
	    warn("Could not enable relative mouse mode: %s", SDL_GetError());
	Set_window_grab(true);
	if (!SDL_HideCursor())
	    warn("Could not hide the pointer: %s", SDL_GetError());
    } else {
    	MainWidget_ShowMenu(MainWidget, true);
	if (!Set_relative_mouse_mode(false))
	    warn("Could not disable relative mouse mode: %s", SDL_GetError());
	Set_window_grab(false);
	if (!SDL_ShowCursor())
	    warn("Could not show the pointer: %s", SDL_GetError());
    }
#endif
}

void Platform_specific_talk_set_state(bool on)
{
    assert(clData.talking != on);
    if (on)
	Console_show();
    else
	Console_hide();
}

void Record_toggle(void)
{
    /* TODO: implement if you think it is worth it */
    Add_message("Can't record with this client. [*Client reply*]");
}

void Toggle_radar_and_scorelist(void)
{
    /* TODO */
    return;
}

int Process_event(SDL_Event *evt)
{
    int button, mouse_movement = 0, x, y;
    float mouse_x, mouse_y, wheel_y;

    if (Console_process(evt)) return 1;
    
    switch (evt->type) {
	
    case SDL_EVENT_QUIT:
	Client_exit(0);
	break;
	
    case SDL_EVENT_KEY_DOWN:
	if (Console_isVisible()) break;
	if (evt->key.repeat != 0) break;
	Keyboard_button_pressed((xp_keysym_t)evt->key.key);
	break;
	
    case SDL_EVENT_KEY_UP:
        /* letting release events through to prevent some keys from locking */
	/*if (Console_isVisible()) break;*/
	Keyboard_button_released((xp_keysym_t)evt->key.key);
	break;
	
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
	button = evt->button.button;
	if (is_sdl_side_button(button))
	    break;
	process_mouse_button(button, evt->button.down,
			     (int)evt->button.x, (int)evt->button.y);
	break;
	
    case SDL_EVENT_MOUSE_MOTION:
	if (clData.pointerControl) {
	    mouseMovement += evt->motion.xrel;
	    if (mouseMovement >= (float)INT_MAX) {
		mouse_movement = INT_MAX;
		mouseMovement -= (float)INT_MAX;
	    } else if (mouseMovement <= (float)INT_MIN) {
		mouse_movement = INT_MIN;
		mouseMovement -= (float)INT_MIN;
	    } else {
		mouse_movement = (int)mouseMovement;
		mouseMovement -= (float)mouse_movement;
	    }
	} else {
	    /*xpprintf("mouse motion xrel=%i yrel=%i\n",evt->motion.xrel,evt->motion.yrel);*/
	    /*for (i = 0;i<NUM_MOUSE_BUTTONS;++i)*/ /* dragdrop for all mouse buttons*/
	    if (clicktarget[0]) { /*is button one pressed?*/
		/*xpprintf("SDL_EVENT_MOUSE_BUTTON_DOWN drag: area found!\n");*/
		if (clicktarget[0]->motion) {
		    clicktarget[0]->motion(evt->motion.xrel,
					   evt->motion.yrel,
					   evt->motion.x,
					   evt->motion.y,
					   clicktarget[0]->motiondata);
		}
	    } else {
		GLWidget *tmp = FindGLWidget(MainWidget, evt->motion.x,
					     evt->motion.y);
		if (tmp != hovertarget) {
		    if (hovertarget && hovertarget->hover) {
			hovertarget->hover(false, evt->motion.x,
					   evt->motion.y,
					   hovertarget->hoverdata);
		    }
		    tmp = FindGLWidget(MainWidget, evt->motion.x,
					   evt->motion.y);
		    if (tmp && tmp->hover)
			tmp->hover(true, evt->motion.x, evt->motion.y,
				   tmp->hoverdata);
		    hovertarget = tmp;
		}
	    }
	}
	break;
	
    case SDL_EVENT_MOUSE_BUTTON_UP:
	button = evt->button.button;
	if (is_sdl_side_button(button))
	    break;
	process_mouse_button(button, evt->button.down,
			     (int)evt->button.x, (int)evt->button.y);
	break;

    case SDL_EVENT_MOUSE_WHEEL:
	wheel_y = evt->wheel.y;
	if (evt->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
	    wheel_y = -wheel_y;
	wheelMovement += wheel_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	x = (int)mouse_x;
	y = (int)mouse_y;
	while (wheelMovement >= 1.0f) {
	    process_mouse_button(4, true, x, y);
	    process_mouse_button(4, false, x, y);
	    wheelMovement -= 1.0f;
	}
	while (wheelMovement <= -1.0f) {
	    process_mouse_button(5, true, x, y);
	    process_mouse_button(5, false, x, y);
	    wheelMovement += 1.0f;
	}
	break;

#ifdef __EMSCRIPTEN__
    /* Browser game geometry uses CSS pixels, independently of backing pixels. */
    case SDL_EVENT_WINDOW_RESIZED:
#else
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
#endif
	Window_size_changed(evt->window.data1, evt->window.data2);
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
	release_input_state();
        break;

    default:
      break;
    }
    
    if (mouse_movement)
	Client_pointer_move(mouse_movement);
    return 1;
}

/* kps - just here so that this can link to generic client files */
void Config_redraw(void)
{

}
