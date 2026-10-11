/*
	SDL_console: An easy to use drop-down console based on the SDL library
	Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Clemens Wacha
	
	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Library General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.
	
	This library is distributed in the hope that it will be useful,
	but WHITOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Library General Public License for more details.
	
	You should have received a copy of the GNU Library Generla Public
	License along with this library; if not, write to the Free
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
	
	Clemens Wacha
	reflex-2000@gmx.net
*/

/*  SDL_console.c
 *  Written By: Garrett Banuk <mongoose@mongeese.org>
 *  Code Cleanup and heavily extended by: Clemens Wacha <reflex-2000@gmx.net>
 *  Modified for XPilot Infinity/SDL: Juha Lindström <juhal@users.sourceforge.net>
 */

#include "xpclient_sdl.h"

#include "SDL_console.h"
#include "DT_drawtext.h"

#ifdef _WINDOWS
# define __FUNCTION__ ""
# define vsnprintf _vsnprintf
#endif
#define PRINT_ERROR(X) fprintf(stderr, "ERROR in %s:%s(): %s\n", __FILE__, __FUNCTION__, X)

/* This contains a pointer to the "topmost" console. The console that
 * is currently taking keyboard input. */
static ConsoleInformation *Topmost;

static SDL_Surface *CON_CreateSurface(SDL_PixelFormat format,
				      int width, int height)
{
    SDL_Surface *surface;

    if (format == SDL_PIXELFORMAT_UNKNOWN || width <= 0 || height <= 0) {
	SDL_SetError("Invalid console surface parameters");
	return NULL;
    }

    surface = SDL_CreateSurface(width, height, format);
    if (surface == NULL)
	return NULL;
    if (!SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE)) {
	SDL_DestroySurface(surface);
	return NULL;
    }
    return surface;
}

static int CON_PrepareInputBackground(SDL_Surface *input_background,
				      SDL_Surface *console_surface,
				      SDL_Surface *background_image,
				      int background_x, int background_y,
				      Uint8 green)
{
    SDL_Rect backgroundsrc, backgrounddest;

    if (!SDL_FillSurfaceRect(input_background, NULL,
		     SDL_MapSurfaceRGBA(input_background, 0, green, 0,
					SDL_ALPHA_OPAQUE)))
	return -1;
    if (background_image == NULL)
	return 0;

    backgroundsrc.x = 0;
    backgroundsrc.y = console_surface->h - input_background->h -
	background_y;
    backgroundsrc.w = background_image->w;
    backgroundsrc.h = input_background->h;

    backgrounddest.x = background_x;
    backgrounddest.y = 0;
    backgrounddest.w = background_image->w;
    backgrounddest.h = input_background->h;

    return SDL_BlitSurface(background_image, &backgroundsrc,
			   input_background, &backgrounddest) ? 0 : -1;
}

/*  Takes keys from the keyboard and inputs them to the console
    If the event was not handled (i.e. WM events or unknown ctrl-shift 
    sequences) the function returns the event for further processing. */
SDL_Event *CON_Events(SDL_Event * event)
{
    const unsigned char *input;

    if (Topmost == NULL)
	return event;
    if (!CON_isVisible(Topmost))
	return event;

    if (event->type == SDL_EVENT_KEY_UP) {
	Topmost->TextInputModifiers = event->key.mod;
	return event;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
	Topmost->TextInputModifiers = event->key.mod;
	if (event->key.mod & SDL_KMOD_CTRL) {
	    /* CTRL pressed */
	    /* kps - please modify this to work like in talk.c */
	    switch (event->key.key) {
	    case SDLK_A:
		Cursor_Home(Topmost);
		break;
	    case SDLK_B:
		Cursor_Left(Topmost);
		break;
	    case SDLK_F:
		Cursor_Right(Topmost);
		break;
	    case SDLK_E:
		Cursor_End(Topmost);
		break;
		/*
		 * kps - Ctrl-k should really just clear from current
		 * cursor position to end of line.
		 */
	    case SDLK_K:
	    case SDLK_U:
		Clear_Command(Topmost);
		break;
	    case SDLK_L:
		Clear_History(Topmost);
		CON_UpdateConsole(Topmost);
		break;
	    default:
		return event;
	    }
#if 0
	} else if (event->key.mod & SDL_KMOD_ALT) {
	    /* the console does not handle ALT combinations! */
	    return event;
#endif
	} else {
	    switch (event->key.key) {
	    case SDLK_HOME:
		if (event->key.mod & SDL_KMOD_SHIFT) {
		    Topmost->ConsoleScrollBack = Topmost->LineBuffer - 1;
		    CON_UpdateConsole(Topmost);
		} else {
		    Cursor_Home(Topmost);
		}
		break;
	    case SDLK_END:
		if (event->key.mod & SDL_KMOD_SHIFT) {
		    Topmost->ConsoleScrollBack = 0;
		    CON_UpdateConsole(Topmost);
		} else {
		    Cursor_End(Topmost);
		}
		break;
	    case SDLK_PAGEUP:
		Topmost->ConsoleScrollBack += CON_LINE_SCROLL;
		if (Topmost->ConsoleScrollBack > Topmost->LineBuffer - 1)
		    Topmost->ConsoleScrollBack = Topmost->LineBuffer - 1;

		CON_UpdateConsole(Topmost);
		break;
	    case SDLK_PAGEDOWN:
		Topmost->ConsoleScrollBack -= CON_LINE_SCROLL;
		if (Topmost->ConsoleScrollBack < 0)
		    Topmost->ConsoleScrollBack = 0;
		CON_UpdateConsole(Topmost);
		break;
	    case SDLK_UP:
		Command_Up(Topmost);
		break;
	    case SDLK_DOWN:
		Command_Down(Topmost);
		break;
	    case SDLK_LEFT:
		Cursor_Left(Topmost);
		break;
	    case SDLK_RIGHT:
		Cursor_Right(Topmost);
		break;
	    case SDLK_BACKSPACE:
		Cursor_BSpace(Topmost);
		break;
	    case SDLK_DELETE:
		Cursor_Del(Topmost);
		break;
	    case SDLK_INSERT:
		Topmost->InsMode = 1 - Topmost->InsMode;
		break;
	    case SDLK_TAB:
		CON_TabCompletion(Topmost);
		break;
	    case SDLK_RETURN:
		if (strlen(Topmost->Command) > 0) {
		    CON_NewLineCommand(Topmost);

		    /* copy the input into the past commands strings */
		    strcpy(Topmost->CommandLines[0], Topmost->Command);

		    /* display the command including the prompt */
		    CON_Out(Topmost, "%s%s", Topmost->Prompt,
			    Topmost->Command);

		    CON_Execute(Topmost, Topmost->Command);
		    /* printf("Command: %s\n", Topmost->Command); */

		    Clear_Command(Topmost);
		    Topmost->CommandScrollBack = -1;
		}
		else
		    /* deactivate Console if return is pressed on empty line */
		    
		    /* was: CON_Hide(Topmost); */
		    Talk_set_state(false);
		break;
	    case SDLK_ESCAPE:
			if (strlen(Topmost->Command) > 0) {
		    CON_NewLineCommand(Topmost);

		    /* copy the input into the past commands strings */
		    strcpy(Topmost->CommandLines[0], Topmost->Command);

		    Clear_Command(Topmost);
		    Topmost->CommandScrollBack = -1;
			}
		/* deactivate Console */

		/*was: CON_Hide(Topmost);*/
		Talk_set_state(false);
		
		return NULL;
	    default:
		return event;
	    }
	}
	return NULL;
    }
    if (event->type == SDL_EVENT_TEXT_INPUT) {
	if (Topmost->TextInputModifiers & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI))
	    return NULL;
	for (input = (const unsigned char *)event->text.text;
	     *input != '\0'; input++) {
	    if (*input < 0x20 || *input > 0x7e)
		continue;
	    Cursor_Add(Topmost, (char)*input);
	    if (!Topmost->InsMode)
		Cursor_Del(Topmost);
	}
	return NULL;
    }
    return event;
}

/* Updates the console, draws the background and the history lines. Does not draw the Commandline */
void CON_UpdateConsole(ConsoleInformation * console)
{
    int loop;
    int loop2;
    int Screenlines;
    SDL_Rect DestRect;
    /*BitFont *CurrentFont = DT_FontPointer(console->FontNumber);*/

    if (!console)
	return;

    /* Due to the Blits, the update is not very fast: So only update if it's worth it */
    if (!CON_isVisible(console))
	return;

    Screenlines = console->ConsoleSurface->h / console->FontHeight;


    SDL_FillSurfaceRect(console->ConsoleSurface, NULL,
		 SDL_MapSurfaceRGBA(console->ConsoleSurface, 0, 20, 0,
				    SDL_ALPHA_OPAQUE));

    /* draw the background image if there is one */
    if (console->BackgroundImage) {
	DestRect.x = console->BackX;
	DestRect.y = console->BackY;
	DestRect.w = console->BackgroundImage->w;
	DestRect.h = console->BackgroundImage->h;
	SDL_BlitSurface(console->BackgroundImage, NULL,
			console->ConsoleSurface, &DestRect);
    }

    /*      now draw text from last but second line to top
       loop: for every line in the history
       loop2: draws the scroll indicators to the line above the Commandline
     */
    for (loop = 0;
	 loop < Screenlines - 1
	 && loop < console->LineBuffer - console->ConsoleScrollBack;
	 loop++) {
	if (console->ConsoleScrollBack != 0 && loop == 0)
	    for (loop2 = 0; loop2 < (console->VChars / 5) + 1; loop2++)
		DT_DrawText(CON_SCROLL_INDICATOR, console->ConsoleSurface,
			    console->FontNumber,
			    CON_CHAR_BORDER +
			    (loop2 * 5 * console->FontWidth),
			    (Screenlines - loop -
			     2) * console->FontHeight);
	else
	    DT_DrawText(console->
			ConsoleLines[console->ConsoleScrollBack + loop],
			console->ConsoleSurface, console->FontNumber,
			CON_CHAR_BORDER,
			(Screenlines - loop - 2) * console->FontHeight);
    }
}

void CON_UpdateOffset(ConsoleInformation * console)
{
    if (!console)
	return;

    switch (console->Visible) {
    case CON_CLOSING:
	console->RaiseOffset -= CON_OPENCLOSE_SPEED;
	if (console->RaiseOffset <= 0) {
	    console->RaiseOffset = 0;
	    console->Visible = CON_CLOSED;
	}
	break;
    case CON_OPENING:
	console->RaiseOffset += CON_OPENCLOSE_SPEED;
	if (console->RaiseOffset >= console->ConsoleSurface->h) {
	    console->RaiseOffset = console->ConsoleSurface->h;
	    console->Visible = CON_OPEN;
	}
	break;
    case CON_OPEN:
    case CON_CLOSED:
	break;
    }
}

/* Draws the console buffer to the screen if the console is "visible" */
void CON_DrawConsole(ConsoleInformation * console)
{
    SDL_Rect DestRect;
    SDL_Rect SrcRect;

    if (!console)
	return;

    /* only draw if console is visible: here this means, that the console is not CON_CLOSED */
    if (console->Visible == CON_CLOSED)
	return;

    /* Update the scrolling offset */
    CON_UpdateOffset(console);

    /* Update the command line since it has a blinking cursor */
    DrawCommandLine();

    SrcRect.x = 0;
    SrcRect.y = console->ConsoleSurface->h - console->RaiseOffset;
    SrcRect.w = console->ConsoleSurface->w;
    SrcRect.h = console->RaiseOffset;

    /* Setup the rect the console is being blitted into based on the output screen */
    DestRect.x = console->DispX;
    DestRect.y = console->DispY;
    DestRect.w = console->ConsoleSurface->w;
    DestRect.h = console->RaiseOffset;

    SDL_FillSurfaceRect(console->OutputScreen, &DestRect,
		 SDL_MapSurfaceRGBA(console->OutputScreen,
				    255, 255, 255, console->ConsoleAlpha));
    SDL_BlitSurface(console->ConsoleSurface, &SrcRect,
		    console->OutputScreen, &DestRect);

}


/* Initializes the console */
ConsoleInformation *CON_Init(const char *FontName,
			     SDL_Surface * DisplayScreen, int lines,
			     SDL_Rect rect)
{
    int loop;
    ConsoleInformation *newinfo;
    bool font_loaded = false;

    if (FontName == NULL || DisplayScreen == NULL ||
	DisplayScreen->format == SDL_PIXELFORMAT_UNKNOWN || DisplayScreen->w <= 0 ||
	DisplayScreen->h <= 0 || lines <= 0) {
	PRINT_ERROR("Invalid console initialization parameters");
	return NULL;
    }

    /* Create a new console struct and init it. */
    newinfo = (ConsoleInformation *)calloc(1, sizeof(ConsoleInformation));
    if (newinfo == NULL) {
	PRINT_ERROR
	    ("Could not allocate the space for a new console info struct.\n");
	return NULL;
    }
    newinfo->Visible = CON_CLOSED;
    newinfo->RaiseOffset = 0;
    newinfo->ConsoleLines = NULL;
    newinfo->CommandLines = NULL;
    newinfo->TotalConsoleLines = 0;
    newinfo->ConsoleScrollBack = 0;
    newinfo->TotalCommands = 0;
    newinfo->BackgroundImage = NULL;
    newinfo->ConsoleAlpha = SDL_ALPHA_OPAQUE;
    newinfo->Offset = 0;
    newinfo->InsMode = 1;
    newinfo->CursorPos = 0;
    newinfo->CommandScrollBack = 0;
    newinfo->OutputScreen = DisplayScreen;
    newinfo->Prompt = strdup(CON_DEFAULT_PROMPT);
    if (newinfo->Prompt == NULL) {
	PRINT_ERROR("Could not allocate the default console prompt");
	goto fail;
    }
    newinfo->HideKey = CON_DEFAULT_HIDEKEY;

    CON_SetExecuteFunction(newinfo, Default_CmdFunction);
    CON_SetTabCompletion(newinfo, Default_TabFunction);

    /* Load the consoles font */
    if (-1 == (newinfo->FontNumber = DT_LoadFont(FontName, TRANS_FONT))) {
	PRINT_ERROR("Could not load the font ");
	fprintf(stderr, "\"%s\" for the console!\n", FontName);
	goto fail;
    }
    font_loaded = true;

    newinfo->FontHeight = DT_FontHeight(newinfo->FontNumber);
    newinfo->FontWidth = DT_FontWidth(newinfo->FontNumber);
    if (newinfo->FontHeight <= 0 || newinfo->FontWidth <= 0) {
	PRINT_ERROR("The console font has invalid dimensions");
	goto fail;
    }

    /* make sure that the size of the console is valid */
    if (rect.w <= 0 || rect.w > newinfo->OutputScreen->w
	|| rect.w / newinfo->FontWidth < 32)
	rect.w = newinfo->OutputScreen->w;
    if (rect.h <= 0 || rect.h > newinfo->OutputScreen->h
	|| rect.h < newinfo->FontHeight)
	rect.h = newinfo->OutputScreen->h;
    if (rect.w <= CON_CHAR_BORDER || rect.h < newinfo->FontHeight) {
	PRINT_ERROR("The output screen is too small for the console");
	goto fail;
    }
    if (rect.x < 0 || rect.x > newinfo->OutputScreen->w - rect.w)
	newinfo->DispX = 0;
    else
	newinfo->DispX = rect.x;
    if (rect.y < 0 || rect.y > newinfo->OutputScreen->h - rect.h)
	newinfo->DispY = 0;
    else
	newinfo->DispY = rect.y;

    /* load the console surface */
    newinfo->ConsoleSurface = CON_CreateSurface(newinfo->OutputScreen->format,
						 rect.w, rect.h);
    if (newinfo->ConsoleSurface == NULL) {
	PRINT_ERROR("Couldn't create the ConsoleSurface\n");
	goto fail;
    }
    if (!SDL_FillSurfaceRect(newinfo->ConsoleSurface, NULL,
		     SDL_MapSurfaceRGBA(newinfo->ConsoleSurface, 0, 20, 0,
					newinfo->ConsoleAlpha))) {
	PRINT_ERROR(SDL_GetError());
	goto fail;
    }

    /* Load the dirty rectangle for user input */
    newinfo->InputBackground =
	CON_CreateSurface(newinfo->OutputScreen->format, rect.w,
			  newinfo->FontHeight);
    if (newinfo->InputBackground == NULL) {
	PRINT_ERROR("Couldn't create the InputBackground\n");
	goto fail;
    }
    if (CON_PrepareInputBackground(newinfo->InputBackground,
				   newinfo->ConsoleSurface, NULL, 0, 0,
				   20) < 0) {
	PRINT_ERROR(SDL_GetError());
	goto fail;
    }

    /* calculate the number of visible characters in the command line */
    newinfo->VChars = (rect.w - CON_CHAR_BORDER) / newinfo->FontWidth;
    if (newinfo->VChars <= 0) {
	PRINT_ERROR("The console is too narrow for its font");
	goto fail;
    }
    if (newinfo->VChars > CON_CHARS_PER_LINE)
	newinfo->VChars = CON_CHARS_PER_LINE;

    /* deprecated! Memory errors disabled by C.Wacha :-)
       We would like to have a minumum # of lines to guarentee we don't create a memory error */
    /*
       if(rect.h / newinfo->FontHeight > lines)
       newinfo->LineBuffer = rect.h / newinfo->FontHeight;
       else
       newinfo->LineBuffer = lines;
     */
    newinfo->LineBuffer = lines;

    newinfo->ConsoleLines = (char **)calloc(newinfo->LineBuffer,
					    sizeof(*newinfo->ConsoleLines));
    newinfo->CommandLines = (char **)calloc(newinfo->LineBuffer,
					    sizeof(*newinfo->CommandLines));
    if (newinfo->ConsoleLines == NULL || newinfo->CommandLines == NULL) {
	PRINT_ERROR("Could not allocate the console line buffers");
	goto fail;
    }
    for (loop = 0; loop <= newinfo->LineBuffer - 1; loop++) {
	newinfo->ConsoleLines[loop] =
	    (char *) calloc(CON_CHARS_PER_LINE + 1, sizeof(char));
	newinfo->CommandLines[loop] =
	    (char *) calloc(CON_CHARS_PER_LINE + 1, sizeof(char));
	if (newinfo->ConsoleLines[loop] == NULL ||
	    newinfo->CommandLines[loop] == NULL) {
	    PRINT_ERROR("Could not allocate a console line");
	    goto fail;
	}
    }
    memset(newinfo->Command, 0, CON_CHARS_PER_LINE + 1);
    memset(newinfo->LCommand, 0, CON_CHARS_PER_LINE + 1);
    memset(newinfo->RCommand, 0, CON_CHARS_PER_LINE + 1);
    memset(newinfo->VCommand, 0, CON_CHARS_PER_LINE + 1);


    /*CON_Out(newinfo, "Console initialised."); */
    CON_NewLineConsole(newinfo);

    return newinfo;

fail:
    if (font_loaded)
	DT_UnloadFont(newinfo->FontNumber);
    CON_Free(newinfo);
    return NULL;
}

/* Makes the console visible */
void CON_Show(ConsoleInformation * console)
{
    if (console) {
	console->Visible = CON_OPENING;
	CON_UpdateConsole(console);

    }
}

/* Hides the console (make it invisible) */
void CON_Hide(ConsoleInformation * console)
{
    if (console) {
	console->Visible = CON_CLOSING;
    }
}

/* tells wether the console is visible or not */
int CON_isVisible(ConsoleInformation * console)
{
    if (!console)
	return CON_CLOSED;
    return ((console->Visible == CON_OPEN)
	    || (console->Visible == CON_OPENING));
}

/* Frees all the memory loaded by the console */
void CON_Destroy(ConsoleInformation * console)
{
	if (console == NULL)
	    return;
	DT_UnloadFont(console->FontNumber);
	CON_Free(console);
}

/* Frees all the memory loaded by the console */
void CON_Free(ConsoleInformation * console)
{
    int i;

    if (!console)
	return;

    if (Topmost == console)
	Topmost = NULL;

    if (console->ConsoleLines != NULL) {
	for (i = 0; i < console->LineBuffer; i++)
	    free(console->ConsoleLines[i]);
    }
    if (console->CommandLines != NULL) {
	for (i = 0; i < console->LineBuffer; i++)
	    free(console->CommandLines[i]);
    }
    free(console->ConsoleLines);
    free(console->CommandLines);
    free(console->Prompt);
    SDL_DestroySurface(console->BackgroundImage);
    SDL_DestroySurface(console->InputBackground);
    SDL_DestroySurface(console->ConsoleSurface);
    console->ConsoleLines = NULL;
    console->CommandLines = NULL;
    console->Prompt = NULL;
    console->BackgroundImage = NULL;
    console->InputBackground = NULL;
    console->ConsoleSurface = NULL;
    free(console);
}


/* Increments the console lines */
void CON_NewLineConsole(ConsoleInformation * console)
{
    int loop;
    char *temp;

    if (!console)
	return;

    temp = console->ConsoleLines[console->LineBuffer - 1];

    for (loop = console->LineBuffer - 1; loop > 0; loop--)
	console->ConsoleLines[loop] = console->ConsoleLines[loop - 1];

    console->ConsoleLines[0] = temp;

    memset(console->ConsoleLines[0], 0, CON_CHARS_PER_LINE + 1);
    if (console->TotalConsoleLines < console->LineBuffer - 1)
	console->TotalConsoleLines++;

    /* Now adjust the ConsoleScrollBack
       dont scroll if not at bottom */
    if (console->ConsoleScrollBack != 0)
	console->ConsoleScrollBack++;
    /* boundaries */
    if (console->ConsoleScrollBack > console->LineBuffer - 1)
	console->ConsoleScrollBack = console->LineBuffer - 1;

}


/* Increments the command lines */
void CON_NewLineCommand(ConsoleInformation * console)
{
    int loop;
    char *temp;

    if (!console)
	return;

    temp = console->CommandLines[console->LineBuffer - 1];


    for (loop = console->LineBuffer - 1; loop > 0; loop--)
	console->CommandLines[loop] = console->CommandLines[loop - 1];

    console->CommandLines[0] = temp;

    memset(console->CommandLines[0], 0, CON_CHARS_PER_LINE + 1);
    if (console->TotalCommands < console->LineBuffer - 1)
	console->TotalCommands++;
}

/* Draws the command line the user is typing in to the screen */
/* completely rewritten by C.Wacha */
void DrawCommandLine()
{
    SDL_Rect rect;
    int x;
    int commandbuffer;
    BitFont *CurrentFont;
    static Uint32 NextBlinkTime = 0;	/* time the consoles cursor blinks again */
    static int LastCursorPos = 0;	/* Last Cursor Position */
    static int Blink = 0;	/* Is the cursor currently blinking */

    if (!Topmost)
	return;

    commandbuffer = Topmost->VChars - strlen(Topmost->Prompt) - 1;	/*  -1 to make cursor visible */

    CurrentFont = DT_FontPointer(Topmost->FontNumber);

    /* calculate display offset from current cursor position */
    if (Topmost->Offset < Topmost->CursorPos - commandbuffer)
	Topmost->Offset = Topmost->CursorPos - commandbuffer;
    if (Topmost->Offset > Topmost->CursorPos)
	Topmost->Offset = Topmost->CursorPos;

    /* first add prompt to visible part */
    SDL_strlcpy(Topmost->VCommand, Topmost->Prompt, sizeof(Topmost->VCommand));

    /* then add the visible part of the command */
    SDL_strlcat(Topmost->VCommand, &Topmost->Command[Topmost->Offset],
	       sizeof(Topmost->VCommand));


    /* first of all restore InputBackground */
    rect.x = 0;
    rect.y = Topmost->ConsoleSurface->h - Topmost->FontHeight;
    rect.w = Topmost->InputBackground->w;
    rect.h = Topmost->InputBackground->h;
    SDL_BlitSurface(Topmost->InputBackground, NULL,
		    Topmost->ConsoleSurface, &rect);

    /* now add the text */
    DT_DrawText(Topmost->VCommand, Topmost->ConsoleSurface,
		Topmost->FontNumber, CON_CHAR_BORDER,
		Topmost->ConsoleSurface->h - Topmost->FontHeight);

    /* at last add the cursor
       check if the blink period is over */
    if (SDL_GetTicks() > NextBlinkTime) {
	NextBlinkTime = SDL_GetTicks() + CON_BLINK_RATE;
	Blink = 1 - Blink;
    }

    /* check if cursor has moved - if yes display cursor anyway */
    if (Topmost->CursorPos != LastCursorPos) {
	LastCursorPos = Topmost->CursorPos;
	NextBlinkTime = SDL_GetTicks() + CON_BLINK_RATE;
	Blink = 1;
    }

    if (Blink) {
	x = CON_CHAR_BORDER + Topmost->FontWidth * (Topmost->CursorPos -
						    Topmost->Offset +
						    strlen(Topmost->
							   Prompt));
	if (Topmost->InsMode)
	    DT_DrawText(CON_INS_CURSOR, Topmost->ConsoleSurface,
			Topmost->FontNumber, x,
			Topmost->ConsoleSurface->h - Topmost->FontHeight);
	else
	    DT_DrawText(CON_OVR_CURSOR, Topmost->ConsoleSurface,
			Topmost->FontNumber, x,
			Topmost->ConsoleSurface->h - Topmost->FontHeight);
    }

}

/* Outputs text to the console (in game), up to CON_CHARS_PER_LINE chars can be entered */
void CON_Out(ConsoleInformation * console, const char *str, ...)
{
    va_list marker;

    char temp[CON_CHARS_PER_LINE + 1];
    char *ptemp;

    if (!console)
	return;

    va_start(marker, str);
    vsnprintf(temp, CON_CHARS_PER_LINE, str, marker);
    va_end(marker);

    ptemp = temp;

    /* temp now contains the complete string we want to output
       the only problem is that temp is maybe longer than the console
       width so we have to cut it into several pieces */

    if (console->ConsoleLines) {
	while ((int)strlen(ptemp) > console->VChars) {
	    CON_NewLineConsole(console);
	    strncpy(console->ConsoleLines[0], ptemp, console->VChars);
	    console->ConsoleLines[0][console->VChars] = '\0';
	    ptemp = &ptemp[console->VChars];
	}
	CON_NewLineConsole(console);
	strncpy(console->ConsoleLines[0], ptemp, console->VChars);
	console->ConsoleLines[0][console->VChars] = '\0';
	CON_UpdateConsole(console);
    }

    /* And print to stdout */
    /* printf("%s\n", temp); */
}


/* Sets the alpha level of the console, 0 turns off alpha blending */
void CON_Alpha(ConsoleInformation * console, unsigned char alpha)
{
    if (!console)
	return;

    /* store alpha as state! */
    console->ConsoleAlpha = alpha;
}


/* Adds  background image to the console, x and y based on consoles x and y */
int CON_Background(ConsoleInformation * console, const char *image, int x,
			   int y)
{
    SDL_Surface *temp = NULL;
    SDL_Surface *new_background = NULL;
    SDL_Surface *new_input_background = NULL;
    SDL_Surface *old_background;
    SDL_Surface *old_input_background;

    if (console == NULL || console->OutputScreen == NULL ||
	console->OutputScreen->format == SDL_PIXELFORMAT_UNKNOWN ||
	console->ConsoleSurface == NULL || console->InputBackground == NULL)
	return 1;

    /* Load a new background */
    if (image != NULL) {
#ifdef HAVE_SDL_IMAGE
	temp = IMG_Load(image);
#else
	temp = SDL_LoadBMP(image);
#endif
	if (temp == NULL) {
	    fprintf(stderr, "Cannot load background %s: %s\n", image,
		    SDL_GetError());
	    return 1;
	}

	new_background =
	    SDL_ConvertSurface(temp, console->OutputScreen->format);
	SDL_DestroySurface(temp);
	temp = NULL;
	if (new_background == NULL) {
	    fprintf(stderr, "Cannot convert background %s: %s\n", image,
		    SDL_GetError());
	    return 1;
	}
	if (!SDL_SetSurfaceBlendMode(new_background, SDL_BLENDMODE_NONE)) {
	    fprintf(stderr, "Cannot prepare background %s: %s\n", image,
		    SDL_GetError());
	    SDL_DestroySurface(new_background);
	    return 1;
	}
    }

    new_input_background =
	CON_CreateSurface(console->ConsoleSurface->format,
			  console->InputBackground->w,
			  console->InputBackground->h);
    if (new_input_background == NULL ||
	CON_PrepareInputBackground(new_input_background,
				   console->ConsoleSurface, new_background,
				   x, y, 0) < 0) {
	PRINT_ERROR(SDL_GetError());
	SDL_DestroySurface(new_input_background);
	SDL_DestroySurface(new_background);
	return 1;
    }

    old_background = console->BackgroundImage;
    old_input_background = console->InputBackground;
    console->BackgroundImage = new_background;
    console->InputBackground = new_input_background;
    if (image != NULL) {
	console->BackX = x;
	console->BackY = y;
    }
    SDL_DestroySurface(old_input_background);
    SDL_DestroySurface(old_background);

    CON_UpdateConsole(console);
    return 0;
}

/* takes a new x and y of the top left of the console window */
void CON_Position(ConsoleInformation * console, int x, int y)
{
    if (!console)
	return;

    if (x < 0 || x > console->OutputScreen->w - console->ConsoleSurface->w)
	console->DispX = 0;
    else
	console->DispX = x;

    if (y < 0 || y > console->OutputScreen->h - console->ConsoleSurface->h)
	console->DispY = 0;
    else
	console->DispY = y;
}

/* resizes the console, has to reset alot of stuff
 * returns 1 on error */
static int CON_ResizeForOutput(ConsoleInformation *console,
			       SDL_Surface *output_screen, SDL_Rect rect)
{
    SDL_Surface *new_console_surface = NULL;
    SDL_Surface *new_input_background = NULL;
    SDL_Surface *old_console_surface;
    SDL_Surface *old_input_background;
    int new_disp_x, new_disp_y, new_visible_characters;

    if (console == NULL || output_screen == NULL ||
	output_screen->format == SDL_PIXELFORMAT_UNKNOWN || output_screen->w <= 0 ||
	output_screen->h <= 0 || console->FontWidth <= 0 ||
	console->FontHeight <= 0 || output_screen == console->ConsoleSurface ||
	output_screen == console->InputBackground ||
	output_screen == console->BackgroundImage)
	return 1;

    /* make sure that the size of the console is valid */
    if (rect.w > output_screen->w || rect.w <= 0 ||
	rect.w / console->FontWidth < 32)
	rect.w = output_screen->w;
    if (rect.h > output_screen->h || rect.h <= 0 ||
	rect.h < console->FontHeight)
	rect.h = output_screen->h;
    if (rect.w <= CON_CHAR_BORDER || rect.h < console->FontHeight)
	return 1;
    if (rect.x < 0 || rect.x > output_screen->w - rect.w)
	new_disp_x = 0;
    else
	new_disp_x = rect.x;
    if (rect.y < 0 || rect.y > output_screen->h - rect.h)
	new_disp_y = 0;
    else
	new_disp_y = rect.y;

    /* load the console surface */
    new_console_surface = CON_CreateSurface(output_screen->format,
					     rect.w, rect.h);
    if (new_console_surface == NULL) {
	PRINT_ERROR("Couldn't create the console->ConsoleSurface\n");
	return 1;
    }
    if (!SDL_FillSurfaceRect(new_console_surface, NULL,
		     SDL_MapSurfaceRGBA(new_console_surface, 0, 20, 0,
					console->ConsoleAlpha))) {
	PRINT_ERROR(SDL_GetError());
	SDL_DestroySurface(new_console_surface);
	return 1;
    }

    /* Load the dirty rectangle for user input */
    new_input_background = CON_CreateSurface(output_screen->format, rect.w,
					      console->FontHeight);
    if (new_input_background == NULL) {
	PRINT_ERROR("Couldn't create the input background\n");
	SDL_DestroySurface(new_console_surface);
	return 1;
    }
    if (CON_PrepareInputBackground(new_input_background,
				   new_console_surface,
				   console->BackgroundImage,
				   console->BackX, console->BackY,
				   console->BackgroundImage == NULL ? 20 : 0) < 0) {
	PRINT_ERROR(SDL_GetError());
	SDL_DestroySurface(new_input_background);
	SDL_DestroySurface(new_console_surface);
	return 1;
    }

    new_visible_characters =
	(rect.w - CON_CHAR_BORDER) / console->FontWidth;
    if (new_visible_characters <= 0) {
	SDL_DestroySurface(new_input_background);
	SDL_DestroySurface(new_console_surface);
	return 1;
    }
    if (new_visible_characters > CON_CHARS_PER_LINE)
	new_visible_characters = CON_CHARS_PER_LINE;

    old_console_surface = console->ConsoleSurface;
    old_input_background = console->InputBackground;
    console->OutputScreen = output_screen;
    console->ConsoleSurface = new_console_surface;
    console->InputBackground = new_input_background;
    console->DispX = new_disp_x;
    console->DispY = new_disp_y;
    console->VChars = new_visible_characters;

    /* Now reset some stuff dependent on the previous size */
    console->ConsoleScrollBack = 0;
    SDL_DestroySurface(old_input_background);
    SDL_DestroySurface(old_console_surface);

    CON_UpdateConsole(console);
    return 0;
}

int CON_Resize(ConsoleInformation * console, SDL_Rect rect)
{
    if (console == NULL)
	return 1;
    return CON_ResizeForOutput(console, console->OutputScreen, rect);
}

/* Transfers the console to another screen surface, and adjusts size */
int CON_Transfer(ConsoleInformation * console,
		 SDL_Surface * new_outputscreen, SDL_Rect rect)
{
    return CON_ResizeForOutput(console, new_outputscreen, rect);
}

/* Sets the topmost console for input */
void CON_Topmost(ConsoleInformation * console)
{
    SDL_Rect rect;

    if (!console)
	return;

    /* Make sure the blinking cursor is gone */
    if (Topmost) {
	rect.x = 0;
	rect.y = Topmost->ConsoleSurface->h - Topmost->FontHeight;
	rect.w = Topmost->InputBackground->w;
	rect.h = Topmost->InputBackground->h;
	SDL_BlitSurface(Topmost->InputBackground, NULL,
			Topmost->ConsoleSurface, &rect);
	DT_DrawText(Topmost->VCommand, Topmost->ConsoleSurface,
		    Topmost->FontNumber, CON_CHAR_BORDER,
		    Topmost->ConsoleSurface->h - Topmost->FontHeight);
    }
    Topmost = console;
}

/* Sets the Prompt for console */
void CON_SetPrompt(ConsoleInformation * console, const char *newprompt)
{
    char *prompt;

    if (console == NULL || newprompt == NULL)
	return;

    /* check length so we can still see at least 1 char :-) */
    if ((int)strlen(newprompt) < console->VChars) {
	prompt = strdup(newprompt);
	if (prompt == NULL) {
	    PRINT_ERROR("Could not allocate the console prompt");
	    return;
	}
	free(console->Prompt);
	console->Prompt = prompt;
    } else {
	CON_Out(console, "prompt too long. (max. %i chars)",
		console->VChars - 1);
    }
}

/* Sets the key that deactivates (hides) the console. */
void CON_SetHideKey(ConsoleInformation * console, int key)
{
    if (console)
	console->HideKey = key;
}

/* Executes the command entered */
void CON_Execute(ConsoleInformation * console, char *command)
{
    if (console)
	console->CmdFunction(console, command);
}

void CON_SetExecuteFunction(ConsoleInformation * console,
			    void (*CmdFunction) (ConsoleInformation *
						 console2, char *command))
{
    if (console)
	console->CmdFunction = CmdFunction;
}

void Default_CmdFunction(ConsoleInformation * console, char *command)
{
    CON_Out(console, "     No CommandFunction registered");
    CON_Out(console, "     use 'CON_SetExecuteFunction' to register one");
    CON_Out(console, " ");
    CON_Out(console, "Unknown Command \"%s\"", command);
}

void CON_SetTabCompletion(ConsoleInformation * console,
			  char *(*TabFunction) (char *command))
{
    if (console)
	console->TabFunction = TabFunction;
}

void CON_TabCompletion(ConsoleInformation * console)
{
    int i, j;
    char *command;

    if (!console)
	return;

    command = strdup(console->LCommand);
    command = console->TabFunction(command);

    if (!command)
	return;			/* no tab completion took place so return silently */

    /*      command now contains the tabcompleted string. check for correct size
       since the string has to fit into the commandline it can have a maximum length of
       CON_CHARS_PER_LINE = commandlength + space + cursor
       => commandlength = CON_CHARS_PER_LINE - 2
     */
    j = strlen(command);
    if (j + 2 > CON_CHARS_PER_LINE)
	j = CON_CHARS_PER_LINE - 2;

    memset(console->LCommand, 0, CON_CHARS_PER_LINE + 1);
    console->CursorPos = 0;

    for (i = 0; i < j; i++) {
	console->CursorPos++;
	console->LCommand[i] = command[i];
    }
    /* add a trailing space */
    console->CursorPos++;
    console->LCommand[j] = ' ';
    console->LCommand[j + 1] = '\0';

    Assemble_Command(console);
}

char *Default_TabFunction(char *command)
{
    CON_Out(Topmost, "     No TabFunction registered");
    CON_Out(Topmost, "     use 'CON_SetTabCompletion' to register one");
    CON_Out(Topmost, " ");
    return NULL;
}

void Cursor_Left(ConsoleInformation * console)
{
    char temp[CON_CHARS_PER_LINE + 1];

    if (Topmost->CursorPos > 0) {
	Topmost->CursorPos--;
	strcpy(temp, Topmost->RCommand);
	strcpy(Topmost->RCommand,
	       &Topmost->LCommand[strlen(Topmost->LCommand) - 1]);
	strcat(Topmost->RCommand, temp);
	Topmost->LCommand[strlen(Topmost->LCommand) - 1] = '\0';
	/* CON_Out(Topmost, "L:%s, R:%s", Topmost->LCommand, Topmost->RCommand); */
    }
}

void Cursor_Right(ConsoleInformation * console)
{
    char temp[CON_CHARS_PER_LINE + 1];

    if (Topmost->CursorPos < (int)strlen(Topmost->Command)) {
	Topmost->CursorPos++;
	strncat(Topmost->LCommand, Topmost->RCommand, 1);
	strcpy(temp, Topmost->RCommand);
	strcpy(Topmost->RCommand, &temp[1]);
	/* CON_Out(Topmost, "L:%s, R:%s", Topmost->LCommand, Topmost->RCommand); */
    }
}

void Cursor_Home(ConsoleInformation * console)
{
    char temp[CON_CHARS_PER_LINE + 1];

    Topmost->CursorPos = 0;
    strcpy(temp, Topmost->RCommand);
    strcpy(Topmost->RCommand, Topmost->LCommand);
    SDL_strlcat(Topmost->RCommand, temp, sizeof(Topmost->RCommand));
    memset(Topmost->LCommand, 0, CON_CHARS_PER_LINE + 1);
}

void Cursor_End(ConsoleInformation * console)
{
    Topmost->CursorPos = strlen(Topmost->Command);
    SDL_strlcat(Topmost->LCommand, Topmost->RCommand,
	       sizeof(Topmost->LCommand));
    memset(Topmost->RCommand, 0, CON_CHARS_PER_LINE + 1);
}

void Cursor_Del(ConsoleInformation * console)
{
    char temp[CON_CHARS_PER_LINE + 1];

    if (strlen(Topmost->RCommand) > 0) {
	strcpy(temp, Topmost->RCommand);
	strcpy(Topmost->RCommand, &temp[1]);
	Assemble_Command(console);
    }
}

void Cursor_BSpace(ConsoleInformation * console)
{
    if (Topmost->CursorPos > 0) {
	Topmost->CursorPos--;
	Topmost->Offset--;
	if (Topmost->Offset < 0)
	    Topmost->Offset = 0;
	Topmost->LCommand[strlen(Topmost->LCommand) - 1] = '\0';
	Assemble_Command(console);
    }
}

void Cursor_Add(ConsoleInformation *console, char character)
{
    int len = 0;

    /* Again: the commandline has to hold the command and the cursor (+1) */
    if (character >= 0x20 && character <= 0x7e &&
	strlen(Topmost->Command) + 1 < CON_CHARS_PER_LINE) {
	Topmost->CursorPos++;
	len = strlen(Topmost->LCommand);
	Topmost->LCommand[len] = character;
	Topmost->LCommand[len + sizeof(char)] = '\0';
	Assemble_Command(console);
    }
}

void Add_String_to_Console(const char *text)
{

    int len = 0, textlen, i;
    
    textlen = (int)strlen(text);
    
    for (i = 0; i < textlen; ++i) {
	if ((unsigned char)text[i] < 0x20 ||
	    (unsigned char)text[i] > 0x7e)
	    continue;
    	/* Again: the commandline has to hold the command and the cursor (+1) */
    	if (strlen(Topmost->Command) + 1 < CON_CHARS_PER_LINE) {
	    Topmost->CursorPos++;
	    len = strlen(Topmost->LCommand);
	    Topmost->LCommand[len] = text[i];
	    Topmost->LCommand[len + sizeof(char)] = '\0';
	    Assemble_Command(Topmost);
    	}
    }
}

void Clear_Command(ConsoleInformation * console)
{
    Topmost->CursorPos = 0;
    memset(Topmost->VCommand, 0, CON_CHARS_PER_LINE + 1);
    memset(Topmost->Command, 0, CON_CHARS_PER_LINE + 1);
    memset(Topmost->LCommand, 0, CON_CHARS_PER_LINE + 1);
    memset(Topmost->RCommand, 0, CON_CHARS_PER_LINE + 1);
}

void Assemble_Command(ConsoleInformation * console)
{
    int len = 0;

    /* Concatenate the left and right side to command */
    len = CON_CHARS_PER_LINE - strlen(Topmost->LCommand);
    strcpy(Topmost->Command, Topmost->LCommand);
    strncat(Topmost->Command, Topmost->RCommand, len);
    Topmost->Command[CON_CHARS_PER_LINE] = '\0';
}

void Clear_History(ConsoleInformation * console)
{
    int loop;

    for (loop = 0; loop <= console->LineBuffer - 1; loop++)
	memset(console->ConsoleLines[loop], 0, CON_CHARS_PER_LINE + 1);
}

void Command_Up(ConsoleInformation * console)
{
    if (console->CommandScrollBack < console->TotalCommands - 1) {
		if (console->CommandScrollBack == -1) strcpy(console->BackupCommand,console->LCommand);
	/* move back a line in the command strings and copy the command to the current input string */
	console->CommandScrollBack++;
	/* I want to know if my string handling REALLY works :-) */
	/* memset(console->RCommand, 0, CON_CHARS_PER_LINE);
	   memset(console->LCommand, 0, CON_CHARS_PER_LINE); */
	console->RCommand[0] = '\0';

	console->Offset = 0;
	strcpy(console->LCommand,
	       console->CommandLines[console->CommandScrollBack]);
	console->CursorPos =
	    strlen(console->CommandLines[console->CommandScrollBack]);
	Assemble_Command(console);
    }
}

void Command_Down(ConsoleInformation * console)
{
    if (console->CommandScrollBack > -1) {
	/* move forward a line in the command strings and copy the command to the current input string */
	console->CommandScrollBack--;
	/* I want to know if my string handling REALLY works :-) */
	/* memset(console->RCommand, 0, CON_CHARS_PER_LINE);
	   memset(console->LCommand, 0, CON_CHARS_PER_LINE); */
	console->RCommand[0] = '\0';

	console->Offset = 0;
	if (console->CommandScrollBack > -1)
	    strcpy(console->LCommand, console->CommandLines[console->CommandScrollBack]);
	else
			strcpy(console->LCommand,console->BackupCommand);
	console->CursorPos = strlen(console->LCommand);
	Assemble_Command(console);
    }
}
