/*
 * XPilot Infinity XP-MapEdit, a map editor for xp maps.  Copyright (C) 1993 by
 *
 *      Aaron Averill           <averila@oes.orst.edu>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Modifications:
 * 1996:
 *      Robert Templeman        <mbcaprt@mphhpd.ph.man.ac.uk>
 * 1997:
 *      William Docter          <wad2@lehigh.edu>
 */

#include "xpmapedit.h"

int screennum, root_width, root_height;
Display *display;
GC T_Back_GC, T_Fore_GC, T_Hlgt_GC, T_Shdw_GC;
XFontStruct *T_Font;
Atom ProtocolAtom;
Atom KillAtom;

char *T_Background = COLOR_BACKGROUND,
    *T_Highlight = COLOR_HIGHLIGHT,
    *T_Foreground = COLOR_FOREGROUND, *T_Shadow = COLOR_SHADOW;

/***************************************************************************/
/* T_ConnectToServer                                                       */
/* Arguments :                                                             */
/*   display_name                                                          */
/* Purpose : Open connection to X server and set global variables display, */
/*           screennum, root_width, and root_height. Get toolkit GCs and   */
/*           font.                                                         */
/***************************************************************************/
void T_ConnectToServer(char *display_name)
{
    if (display_name == NULL) {
	display_name = getenv("DISPLAY");
    }
    if ((display = XOpenDisplay(display_name)) == NULL) {
	fprintf(stderr, "cannot connect to X server %s\n",
		XDisplayName(display_name));
	exit(-1);
    }
    screennum = DefaultScreen(display);
    root_width = DisplayWidth(display, screennum);
    root_height = DisplayHeight(display, screennum);
    T_GetGC(&T_Fore_GC, T_Foreground);
    T_GetGC(&T_Back_GC, T_Background);
    T_GetGC(&T_Hlgt_GC, T_Highlight);
    T_GetGC(&T_Shdw_GC, T_Shadow);
    T_FontInit(&T_Font, "9x15");
    ProtocolAtom = XInternAtom(display, "WM_PROTOCOLS", False);
    KillAtom = XInternAtom(display, "WM_DELETE_WINDOW", False);
}

/***************************************************************************/
/* T_CloseServerConnection                                                 */
/* Arguments :                                                             */
/* Purpose : Close X server connection, unload toolkit font and free GCs.  */
/***************************************************************************/
void T_CloseServerConnection(void)
{
    XUnloadFont(display, T_Font->fid);
    XFreeGC(display, T_Back_GC);
    XFreeGC(display, T_Fore_GC);
    XFreeGC(display, T_Hlgt_GC);
    XFreeGC(display, T_Shdw_GC);
    XCloseDisplay(display);
}

/***************************************************************************/
/* T_SetToolkitFont                                                        */
/* Arguments :                                                             */
/*   font                                                                  */
/* Purpose : Unload old toolkit font and load a new one with the name      */
/*           specified in the argument font.                               */
/***************************************************************************/
void T_SetToolkitFont(char *font)
{
    XUnloadFont(display, T_Font->fid);
    T_FontInit(&T_Font, font);
}

/***************************************************************************/
/* T_GetGC                                                                 */
/* Arguments :                                                             */
/*   gc                                                                    */
/*   foreground                                                            */
/* Purpose : Set up a GC with the specified foreground color name, line    */
/*           width 0. Return integer pixel value of allocated color.       */
/***************************************************************************/
int T_GetGC(GC * gc, char *foreground)
{
    XGCValues values;
    unsigned long valuemask;
    XColor color;
    Colormap colormap;

    colormap = DefaultColormap(display, screennum);

    if (!(XParseColor(display, colormap, foreground, &color))) {
	color.pixel = WhitePixel(display, screennum);
    } else {
	if (!(XAllocColor(display, colormap, &color))) {
	    color.pixel = WhitePixel(display, screennum);
	}
    }
    values.foreground = color.pixel;
    values.background = BlackPixel(display, screennum);
    values.line_width = 1;
    values.graphics_exposures = False;

    valuemask = GCForeground | GCBackground | GCLineWidth;

    *gc = XCreateGC(display, RootWindow(display, screennum), valuemask,
		    &values);
    return color.pixel;
}

/***************************************************************************/
/* T_FontInit                                                              */
/* Arguments :                                                             */
/*   fontinfo                                                              */
/*   fontname                                                              */
/* Purpose : Load a font with the specified name into fontinfo. Return 0   */
/*           if successful. If not, load font "9x15" and return 1.         */
/***************************************************************************/
int T_FontInit(XFontStruct ** fontinfo, char *fontname)
{
    if ((*fontinfo = XLoadQueryFont(display, fontname)) == NULL) {
	*fontinfo = XLoadQueryFont(display, "9x15");
	fprintf(stderr, "Could not find font %s, using 9x15\n", fontname);
	return 1;
    }
    return 0;
}

/***************************************************************************/
/* T_MakeWindow                                                            */
/* Arguments :                                                             */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   fg                                                                    */
/*   bg                                                                    */
/* Purpose : Create a simple window with the specified x,y,width, and      */
/*           height. Set background and foreground to bg and fg. Set       */
/*           window hints such that the window cannot be resized.          */
/***************************************************************************/
Window T_MakeWindow(int x, int y, int width, int height, char *fg,
		    char *bg)
{
    Window window;
    XColor color;
    Colormap colormap;
    int fgpixel, bgpixel;

    colormap = DefaultColormap(display, screennum);

    if (!(XParseColor(display, colormap, fg, &color))) {
	color.pixel = WhitePixel(display, screennum);
    } else {
	if (!(XAllocColor(display, colormap, &color))) {
	    color.pixel = WhitePixel(display, screennum);
	}
    }
    fgpixel = color.pixel;
    if (!(XParseColor(display, colormap, bg, &color))) {
	color.pixel = BlackPixel(display, screennum);
    } else {
	if (!(XAllocColor(display, colormap, &color))) {
	    color.pixel = BlackPixel(display, screennum);
	}
    }
    bgpixel = color.pixel;

    window = XCreateSimpleWindow(display, RootWindow(display, screennum),
				 x, y, width, height, 4, fgpixel, bgpixel);
    T_SetWindowSizeLimits(window, width, height, width, height, 0, 0);
    XSetWMProtocols(display, window, &KillAtom, 1);

    return window;
}

/***************************************************************************/
/* T_SetWindowName                                                         */
/* Arguments :                                                             */
/*   window                                                                */
/*   windowname                                                            */
/*   iconname                                                              */
/* Purpose : Sets window and icon name hints for window.                   */
/***************************************************************************/
void T_SetWindowName(Window window, char windowname[], char iconname[])
{
    XTextProperty windowName, iconName;

    if (XStringListToTextProperty(&windowname, 1, &windowName) == 0) {
	fprintf(stderr, "structure allocation for windowName failed.\n");
	exit(-1);
    }
    if (XStringListToTextProperty(&iconname, 1, &iconName) == 0) {
	fprintf(stderr, "structure allocation for iconName failed.\n");
	exit(-1);
    }

    XSetWMName(display, window, &windowName);
    XSetWMIconName(display, window, &iconName);
    XFree(windowName.value);
    XFree(iconName.value);
}

/***************************************************************************/
/* T_SetWindowSizeLimits                                                   */
/* Arguments :                                                             */
/*   window                                                                */
/*   minwidth                                                              */
/*   minheight                                                             */
/*   maxwidth                                                              */
/*   maxheight                                                             */
/*   aspectx                                                               */
/*   aspecty                                                               */
/* Purpose : Sets size hints for window.                                   */
/***************************************************************************/
void T_SetWindowSizeLimits(Window window, int minwidth, int minheight,
			   int maxwidth, int maxheight, int aspectx,
			   int aspecty)
{
    XSizeHints *sizeh;

    /* Allocate memory for hints */
    if (!(sizeh = XAllocSizeHints())) {
	fprintf(stderr, "failure allocating memory\n");
	exit(-1);
    }

    sizeh->flags = PPosition | PSize;
    if ((maxwidth != 0) && (maxheight != 0)) {
	sizeh->flags = sizeh->flags | PMaxSize;
	sizeh->max_width = maxwidth;
	sizeh->max_height = maxheight;
    }
    if ((minwidth != 0) && (minheight != 0)) {
	sizeh->flags = sizeh->flags | PMinSize;
	sizeh->min_width = minwidth;
	sizeh->min_height = minheight;
    }
    if ((aspectx != 0) && (aspecty != 0)) {
	sizeh->flags = sizeh->flags | PAspect;
	sizeh->max_aspect.x = aspectx;
	sizeh->min_aspect.x = aspectx;
	sizeh->max_aspect.y = aspecty;
	sizeh->min_aspect.y = aspecty;
    }
    XSetWMNormalHints(display, window, sizeh);
    free(sizeh);
}

/***************************************************************************/
/* T_ClearArea                                                             */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/* Purpose : Fills window area with toolkit foreground color.              */
/***************************************************************************/
void T_ClearArea(Window win, int x, int y, int width, int height)
{
    XFillRectangle(display, win, T_Fore_GC, x, y, width, height);
}

/***************************************************************************/
/* T_DrawButton                                                            */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   zheight                                                               */
/*   clear                                                                 */
/* Purpose : Draw a button on window win at x,y,width,height. Argument     */
/*           zheight may be RAISED or LOWERED. If clear is set, clear      */
/*           button area before drawing button.                            */
/***************************************************************************/
void T_DrawButton(Window win, int x, int y, int width, int height,
		  int zheight, int clear)
{
    if (clear) {
	XFillRectangle(display, win, T_Fore_GC, x, y, width + 1,
		       height + 1);
    }
    if (zheight == RAISED) {
	XDrawLine(display, win, T_Hlgt_GC, x, y, x + width, y);
	XDrawLine(display, win, T_Hlgt_GC, x, y, x, y + height);
	XDrawLine(display, win, T_Back_GC, x, y + height, x + width,
		  y + height);
	XDrawLine(display, win, T_Back_GC, x + width, y, x + width,
		  y + height);

    } else {
	XDrawLine(display, win, T_Back_GC, x, y, x + width, y);
	XDrawLine(display, win, T_Back_GC, x, y, x, y + height);
	XDrawLine(display, win, T_Hlgt_GC, x, y + height, x + width,
		  y + height);
	XDrawLine(display, win, T_Hlgt_GC, x + width, y, x + width,
		  y + height);
    }
}

/***************************************************************************/
/* T_PopButton                                                             */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   zheight                                                               */
/* Purpose : Move a button up or down depending on zheight. Copies area    */
/*           inside button up or down and redraws button border.           */
/***************************************************************************/
void T_PopButton(Window win, int x, int y, int width, int height,
		 int zheight)
{
    if (zheight == RAISED) {
	XCopyArea(display, win, win, T_Fore_GC, x + 2, y + 2, width - 2,
		  height - 2, x + 1, y + 1);
    } else {
	XCopyArea(display, win, win, T_Fore_GC, x + 1, y + 1, width - 2,
		  height - 2, x + 2, y + 2);
    }
    T_DrawButton(win, x, y, width, height, zheight, 0);
}

/***************************************************************************/
/* T_DrawTextButton                                                        */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   zheight                                                               */
/*   string                                                                */
/* Purpose : Draw a button with a label string centered.                   */
/***************************************************************************/
void T_DrawTextButton(Window win, int x, int y, int width, int height,
		      int zheight, char *string)
{
    T_DrawButton(win, x, y, width, height, zheight, 1);
    if (string != NULL)
	T_DrawString(win, x + zheight, y + zheight, width, height, BKGR,
		     string, JUSTIFY_CENTER, CROP_RIGHT, -1);
}

/***************************************************************************/
/* T_DrawString                                                            */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   gc                                                                    */
/*   string                                                                */
/*   justify                                                               */
/*   crop                                                                  */
/*   cursorpos                                                             */
/* Purpose : Draw a string on win at x,y,width,height. gc may be a GC or   */
/*           the constant BKGR. justify may be one of the constants        */
/*           JUSTIFY_LEFT,JUSTIFY_RIGHT, or JUSTIFY_CENTER. crop may be    */
/*           CROP_RIGHT or CROP_LEFT. A cursor will be drawn under the     */
/*           character at curpos unless cursorpos is negative.             */
/***************************************************************************/
void T_DrawString(Window win, int x, int y, int width, int height, GC gc,
		  const char *string, int justify, int crop, int cursorpos)
{
    int length, c;

    c = cursorpos;
    XSetFont(display, gc, T_Font->fid);

    if (height < (T_Font->ascent + T_Font->descent))
	return;

    length = strlen(string);
    /* crop the left side of the string until the cursor is in view */
    if (cursorpos >= 0) {
	while ((XTextWidth(T_Font, string, c)) > width) {
	    length--;
	    string++;
	    c--;
	}
    }

    /* crop the left or right side until the string fits */
    while ((XTextWidth(T_Font, string, length)) > width) {
	length--;
	string += crop;
    }

    if (justify == JUSTIFY_CENTER) {
	x += width / 2 - XTextWidth(T_Font, string, length) / 2;
    } else if (justify == JUSTIFY_RIGHT) {
	x += width - XTextWidth(T_Font, string, length);
    }
    y = y + height / 2 + (T_Font->ascent + T_Font->descent) * 0.35;

    XDrawString(display, win, gc, x, y, string, length);

    if (cursorpos < 0) {
	return;
    } else if (cursorpos > length) {
	cursorpos = length;
    }
    XDrawString(display, win, gc,
		(int) (x + XTextWidth(T_Font, string, cursorpos) -
		       XTextWidth(T_Font, CURSOR_CHAR, 1) / 2),
		(int) (y + (T_Font->ascent + T_Font->descent) * .7),
		CURSOR_CHAR, 1);
}

/***************************************************************************/
/* T_DrawText                                                              */
/* Arguments :                                                             */
/*   win                                                                   */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   gc                                                                    */
/*   text                                                                  */
/* Purpose : Draw text on win at x,y,width,height, splitting lines at      */
/*           word boundaries and newlines. gc may be a GC or the           */
/*           constant BKGR.                                                */
/***************************************************************************/
void T_DrawText(Window win, int x, int y, int width, int height, GC gc,
		char *text)
{
    int length, last, h, line;
    char *draw, *next, *curr;

    XSetFont(display, gc, T_Font->fid);

    h = (T_Font->ascent + T_Font->descent);
    draw = next = curr = text;
    length = last = line = 0;
    while ((*curr) != '\0') {
	while ((XTextWidth(T_Font, draw, length + 1) < width) &&
	       ((*curr) != '\0') && ((*curr) != '\n')) {
	    length++;
	    curr++;
	    if ((*curr) == ' ') {
		last = length;
		next = curr;
		next++;
	    }
	}
	if (last == 0) {
	    last = length;
	    next = curr;
	}
	if (((*curr) == '\n') || ((*curr) == '\0')) {
	    last = length;
	    next = curr;
	    if ((*curr) == '\n') {
		next++;
	    }
	}

	XDrawString(display, win, gc, x, (int) (y + line * h + .7 * h),
		    draw, last);
	draw = curr = next;
	length = last = 0;
	line++;
    }
}
