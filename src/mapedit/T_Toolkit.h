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

#ifndef T_TOOLKIT_H
#define T_TOOLKIT_H

#include "xpmapedit.h"

/* Constants for T_DrawButton */
#define                  RAISED          0
#define                  LOWERED         1

/* Constants for T_DrawString */
#define                  CURSOR_CHAR     "^"
#define                  JUSTIFY_LEFT    0
#define                  JUSTIFY_RIGHT   1
#define                  JUSTIFY_CENTER  2
#define                  CROP_RIGHT      0
#define                  CROP_LEFT       1
#define                  BKGR            T_Back_GC

typedef char max_str_t[255];

extern int screennum, root_width, root_height;
extern Display *display;
extern GC T_Back_GC, T_Fore_GC, T_Hlgt_GC, T_Shdw_GC;
extern XFontStruct *T_Font;
extern char *T_Background, *T_Foreground, *T_Shadow, *T_Highlight;
extern Atom ProtocolAtom;
extern Atom KillAtom;


#define                  INACTIVE          0
#define                  ACTIVE            1
#define                  SHADED            2
#define                  OUTPUT_ONLY       3

#define                  T_BUTTON          0
#define                  T_TEXT_BUTTON     1
#define                  T_SCROLL_BOUNDED  2
#define                  T_SCROLL_UNBOUND  3
#define                  T_MULTI_BUTTON    4
#define                  T_STRING_ENTRY    5
#define                  T_TEXT            6
#define                  T_HOLD_BUTTON     7

typedef struct HandlerInfo HandlerInfo_t;

typedef int (*handler_t)(HandlerInfo_t);

typedef struct T_Field {
    char *name, *label;
    short type, active;
    short x, y, width, height, x2, y2;
    handler_t handler;
    int *intvar;
    char *charvar;
    int charvar_length;
    short null;
    struct T_Field *next;
} T_Field_t;

typedef struct T_Form {
    Window window;
    T_Field_t *field;
    T_Field_t *entry;
    int entry_cursor, entry_pos;
    char *entry_restore;
    struct T_Form *next;
} T_Form_t;

struct HandlerInfo {
    T_Form_t *form;
    T_Field_t *field;
    unsigned int button;
    int x, y, count;
};

extern T_Form_t *T_Form;

#define                  POPUPBTNWIDTH     55
#define                  POPUPBTNHEIGHT    30

typedef struct T_Popup_t {
    Window window;
    struct T_Popup_t *next;
} T_Popup_t;

extern T_Popup_t *T_Popup;


/* T_Toolkit.c prototypes */
void T_ConnectToServer(char *display_name);
void T_CloseServerConnection(void);
void T_SetToolkitFont(char *font);
int T_GetGC(GC * gc, char *foreground);
int T_FontInit(XFontStruct ** fontinfo, char *fontname);
Window T_MakeWindow(int x, int y, int width, int height,
		    char *fg, char *bg);
void T_SetWindowName(Window window, char windowname[], char iconname[]);
void T_SetWindowSizeLimits(Window window, int minwidth,
			   int minheight, int maxwidth, int maxheight,
			   int aspectx, int aspecty);

void T_ClearArea(Window win, int x, int y, int width, int height);
void T_DrawButton(Window win, int x, int y, int width,
		  int height, int zheight, int clear);
void T_PopButton(Window win, int x, int y, int width,
		 int height, int zheight);
void T_DrawTextButton(Window win, int x, int y, int width,
		      int height, int zheight, char *string);
void T_DrawString(Window win, int x, int y, int width,
		  int height, GC gc, const char *string, int justify,
		  int crop, int cursorpos);
void T_DrawText(Window win, int x, int y, int width, int height,
		GC gc, char *text);

/* T_Form.c prototypes */
void T_FormEventCheck(XEvent * report);
void T_FormExpose(XEvent * report);
void T_FormButtonPress(XEvent * report);
void T_FormKeyPress(XEvent * report);
void CallFieldHandler(T_Form_t * form, T_Field_t * field, int x,
		      int y, unsigned int button, int count,
		      handler_t handler);
void T_FormClear(Window win);
void T_FormCloseWindow(Window win);
T_Form_t **SeekForm(Window win, short add);
void ChangeField(Window win, const char *name, const char *label,
		 short type, short active, short x, short y, short width,
		 short height, short x2, short y2, handler_t handler,
		 int *intvar, char *charvar, int charvar_length,
		 short null);
void T_FormButton(Window win, const char *name, short x, short y, short width,
		  short height, const char *label, handler_t handler);
void T_FormHoldButton(Window win, const char *name, short x, short y,
		      short width, short height, const char *label,
		      handler_t handler);
void T_FormMultiButton(Window win, const char *name, short x, short y,
		       short width, short height, short x2, short y2,
		       const char *label, int *intvar, short no_null);
void T_FormScrollArea(Window win, const char *name, short type, short x, short y,
		      short width, short height, handler_t handler);
void T_FormText(Window win, const char *name, short x, short y, short width,
		short height, const char *label, short justify);
void T_FormStringEntry(Window win, const char *name, short x, short y,
		       short width, short height, short x2, short y2,
		       const char *label, char *charvar, int charvar_length,
		       handler_t handler);
void T_DrawEntryField(T_Form_t * form, T_Field_t * field);
void T_SetEntryField(T_Form_t * form, T_Field_t * field, int x);
void T_FormRedrawEntryField(char *charvar);

/* T_Popup.c prototypes */
Window T_PopupCreate(int x, int y, int width, int height, char *title);
Window T_PopupAlert(int type, char *message, char *btn1,
		    char *btn2, handler_t handler1, handler_t handler2);
Window T_PopupPrompt(int x, int y, int width, int height,
		     char *title, char *message, char *btn1, char *btn2,
		     char *charvar, int length, handler_t handler);
int T_IsPopupOpen(Window win);
void T_PopupClose(Window win);

/* T_Handler.c prototypes */
int ValidateFloatHandler(HandlerInfo_t info);
int ValidatePositiveFloatHandler(HandlerInfo_t info);
int ValidateIntHandler(HandlerInfo_t info);
int ValidatePositiveIntHandler(HandlerInfo_t info);
int PopupCloseHandler(HandlerInfo_t info);
int FormCloseHandler(HandlerInfo_t info);

#endif
