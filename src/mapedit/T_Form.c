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

T_Form_t *T_Form = NULL;

/***************************************************************************/
/* T_FormEventCheck                                                        */
/* Arguments :                                                             */
/*   report                                                                */
/* Purpose : Called from the main event check loop in application.         */
/***************************************************************************/
void T_FormEventCheck(XEvent * report)
{
    switch (report->type) {

    case Expose:
	if (report->xexpose.count == 0) {
	    T_FormExpose(report);
	}
	break;

    case ButtonPress:
	T_FormButtonPress(report);
	break;

    case KeyPress:
	T_FormKeyPress(report);
	break;
    }
}

/***************************************************************************/
/* T_FormExpose                                                            */
/* Arguments :                                                             */
/*   report                                                                */
/* Purpose : Handle an exposure for window win. Draw all items in form.    */
/***************************************************************************/
void T_FormExpose(XEvent * report)
{
    Window win;
    T_Form_t *form;
    T_Field_t *field;
    char *label;
    int multi, w, h, i, j;

    win = report->xexpose.window;
    form = (*(SeekForm(win, 0)));
    if (form == NULL)
	return;
    field = form->field;
    while (field != NULL) {
	if (field->active != INACTIVE) {
	    switch (field->type) {

	    case T_BUTTON:
	    case T_TEXT_BUTTON:
	    case T_HOLD_BUTTON:
		T_DrawTextButton(form->window, field->x, field->y,
				 field->width, field->height, RAISED,
				 field->label);
		break;

	    case T_MULTI_BUTTON:
		label = malloc(strlen(field->label) + 1);
		strcpy(label, field->label);
		label = strtok(label, ";");
		multi = 1;
		w = (field->width) / (field->x2);
		h = (field->height) / (field->y2);
		for (i = 0; i < field->y2; i++) {
		    for (j = 0; j < field->x2; j++) {
			if (multi == (*(field->intvar))) {
			    T_DrawTextButton(form->window,
					     j * w + field->x,
					     i * h + field->y, w - 1,
					     h - 1, LOWERED, label);
			} else {
			    T_DrawTextButton(form->window,
					     j * w + field->x,
					     i * h + field->y, w - 1,
					     h - 1, RAISED, label);
			}
			multi++;
			if (label != NULL) {
			    label = strtok(NULL, ";");
			}
		    }
		}
		break;

	    case T_TEXT:
		T_DrawString(form->window, field->x, field->y,
			     field->width, field->height, BKGR,
			     field->label, field->x2, CROP_RIGHT, -1);
		break;

	    case T_STRING_ENTRY:
		T_DrawEntryField(form, field);
		if (field->label != NULL) {
		    w = XTextWidth(T_Font, field->label,
				   strlen(field->label));
		    i = field->x + field->x2;
		    j = field->y + field->y2;
		    T_DrawString(form->window, i, j, w, field->height,
				 BKGR, field->label, JUSTIFY_LEFT,
				 CROP_RIGHT, -1);
		}
		break;
	    }
	}
	field = field->next;
    }
}

/***************************************************************************/
/* T_FormButtonPress                                                       */
/* Arguments :                                                             */
/*   eventreport                                                           */
/* Purpose : Handle button press.                                          */
/***************************************************************************/
void T_FormButtonPress(XEvent * report)
{
    Window win, root, child;
    int x, y, rootx, rooty;
    T_Form_t *form;
    T_Field_t *field;
    unsigned int keysbuttons, btn;
    int multi, w, h, x2, y2, count = 0;
    XEvent event;

    win = report->xbutton.window;
    x = report->xbutton.x;
    y = report->xbutton.y;
    btn = report->xbutton.button;

    form = (*(SeekForm(win, 0)));
    if (form == NULL)
	return;
    field = form->field;
    while (field != NULL) {
	if ((x > field->x) && (y > field->y)
	    && (x < (field->x + field->width))
	    && (y < (field->y + field->height))
	    && (field->active == ACTIVE)) {

	    switch (field->type) {

	    case T_BUTTON:
	    case T_TEXT_BUTTON:
	    case T_HOLD_BUTTON:
		T_DrawTextButton(form->window, field->x, field->y,
				 field->width, field->height, LOWERED,
				 field->label);
		break;

	    case T_MULTI_BUTTON:
		multi = (y - field->y) * field->y2 / field->height;
		multi *= field->x2;
		multi += (x - field->x) * field->x2 / field->width;

		if (((multi + 1) != (*(field->intvar))) || (field->null)) {

		    w = field->width / field->x2;
		    h = field->height / field->y2;
		    y2 = ((*(field->intvar)) - 1) / field->x2;
		    x2 = ((*(field->intvar)) - 1) - (y2 * field->x2);
		    if ((*(field->intvar)) != 0) {
			T_PopButton(form->window, x2 * w + field->x,
				    y2 * h + field->y, w - 1, h - 1,
				    RAISED);
		    }

		    y2 = multi / field->x2;
		    x2 = multi - (y2 * field->x2);
		    if ((multi + 1) != (*(field->intvar))) {
			T_PopButton(form->window, x2 * w + field->x,
				    y2 * h + field->y, w - 1, h - 1,
				    LOWERED);
			(*(field->intvar)) = multi + 1;
		    } else {
			(*(field->intvar)) = 0;
		    }
		}
		return;

	    case T_STRING_ENTRY:
		T_SetEntryField(form, field, x - field->x - 2);
		return;
	    }
	    switch (field->type) {

	    case T_SCROLL_UNBOUND:
	    case T_BUTTON:
	    case T_TEXT_BUTTON:
		x2 = -1;
		y2 = -1;
		event = *report;
		while (event.type != ButtonRelease) {
		    if ((x != x2) || (y != y2)) {
			count++;
			if ((field->type == T_SCROLL_UNBOUND)) {
			    CallFieldHandler(form, field, x, y, btn, count,
					     field->handler);
			}
			x2 = x;
			y2 = y;
		    }
		    XCheckMaskEvent(display, (long) (ButtonReleaseMask),
				    &event);
		    if (!XQueryPointer
			(display, event.xmotion.window, &root, &child,
			 &rootx, &rooty, &x, &y, &keysbuttons)) {
			event.type = ButtonRelease;
		    }
		}
		break;

	    case T_HOLD_BUTTON:
		event = *report;
		while (event.type != ButtonRelease) {
		    count++;
		    CallFieldHandler(form, field, x, y, btn, count,
				     field->handler);
		    if (XCheckMaskEvent
			(display, (long) (ButtonReleaseMask),
			 &event) == True) {
			event.type = ButtonRelease;
		    }
		}
		break;
	    }

	    switch (field->type) {

	    case T_BUTTON:
	    case T_TEXT_BUTTON:
	    case T_HOLD_BUTTON:
		T_DrawTextButton(form->window, field->x, field->y,
				 field->width, field->height, RAISED,
				 field->label);
		if ((x > field->x) && (y > field->y)
		    && (x < (field->x + field->width))
		    && (y < (field->y + field->height))) {
		    CallFieldHandler(form, field, x, y, btn, 0,
				     field->handler);
		}
		break;

	    case T_STRING_ENTRY:
	    case T_SCROLL_UNBOUND:
		CallFieldHandler(form, field, x, y, btn, 0,
				 field->handler);
		break;
	    }
	    return;
	} else {
	    field = field->next;
	}
    }
}

/***************************************************************************/
/* T_FormKeyPress                                                          */
/* Arguments :                                                             */
/*   report                                                                */
/* Purpose : Handle Keypresses.                                            */
/***************************************************************************/
void T_FormKeyPress(XEvent * report)
{
    T_Form_t *form;
    char buffer[2];
    int bufsize = 1;
    KeySym keysym;
    XComposeStatus compose;
    int count;
    char *tmpstr, *tmpstr2;

    form = (*(SeekForm(report->xkey.window, 0)));
    if (form == NULL)
	return;
    if (form->entry == NULL)
	return;
    count =
	XLookupString(&report->xkey, buffer, bufsize, &keysym, &compose);
    buffer[bufsize] = '\0';
    if ((keysym == XK_Return) || (keysym == XK_KP_Enter) ||
	(keysym == XK_Linefeed)) {
	T_SetEntryField(form, 0, 0);
	return;
    } else if (((keysym >= XK_KP_Space) && (keysym <= XK_KP_9)) ||
	       ((keysym >= XK_space) && (keysym <= XK_asciitilde))) {

	if (strlen(form->entry->charvar) < form->entry->charvar_length) {
	    tmpstr = malloc(strlen(form->entry->charvar) + 2);
	    tmpstr[0] = '\0';
	    tmpstr2 = form->entry->charvar;
	    tmpstr2 += form->entry_cursor;
	    strncat(tmpstr, form->entry->charvar, form->entry_cursor);
	    strcat(tmpstr, buffer);
	    strcat(tmpstr, tmpstr2);
	    strcpy(form->entry->charvar, tmpstr);
	    free(tmpstr);
	    form->entry_cursor++;
	}

    } else if ((keysym == XK_BackSpace) || (keysym == XK_Delete)) {

	if (form->entry_cursor > 0) {
	    tmpstr = malloc(strlen(form->entry->charvar));
	    tmpstr[0] = '\0';
	    tmpstr2 = form->entry->charvar;
	    tmpstr2 += form->entry_cursor;
	    strncat(tmpstr, form->entry->charvar, form->entry_cursor - 1);
	    strcpy(form->entry->charvar, tmpstr);
	    strcat(form->entry->charvar, tmpstr2);
	    free(tmpstr);
	    form->entry_cursor--;
	}
    } else if (keysym == XK_Home) {
	form->entry_cursor = 0;
    } else if (keysym == XK_Right) {
	if (form->entry_cursor < strlen(form->entry->charvar)) {
	    form->entry_cursor++;
	} else {
	    return;
	}
    } else if (keysym == XK_Left) {
	if (form->entry_cursor > 0) {
	    form->entry_cursor--;
	} else {
	    return;
	}
    } else {
	return;
    }

    if ((form->entry_cursor - 1) < form->entry_pos) {
	if (form->entry_pos != 0) {
	    form->entry_pos = form->entry_cursor - 1;
	    if (form->entry_pos < 0) {
		form->entry_pos = 0;
	    }
	}
    }
    tmpstr = form->entry->charvar;
    tmpstr += form->entry_pos;
    count = form->entry_cursor - form->entry_pos;

    while (XTextWidth(T_Font, tmpstr, count) > form->entry->width - 10) {
	tmpstr++;
	count--;
	form->entry_pos++;
    }
    T_DrawEntryField(form, form->entry);
}

/***************************************************************************/
/* CallFieldHandler                                                        */
/* Arguments :                                                             */
/*   handler                                                               */
/* Purpose : Set up the HandlerInfo_t structure and call handler.            */
/***************************************************************************/
void CallFieldHandler(T_Form_t * form, T_Field_t * field, int x, int y,
		      unsigned int button, int count, handler_t handler)
{
    HandlerInfo_t info;

    if (handler == NULL)
	return;
    info.form = form;
    info.field = field;
    info.x = x;
    info.y = y;
    info.count = count;
    info.button = button;
    handler(info);
}

/***************************************************************************/
/* T_FormClear                                                             */
/* Arguments :                                                             */
/*   win                                                                   */
/* Purpose : Deallocate all resources for a form                           */
/***************************************************************************/
void T_FormClear(Window win)
{
    T_Form_t **form;
    T_Field_t *field, *delfield;

    /* traverse until we find the form correct window */
    form = SeekForm(win, 0);
    if ((*form) == NULL)
	return;

    /* traverse fields and free as we go */
    field = (*form)->field;
    while (field != NULL) {
	delfield = field->next;
	free(field);
	field = delfield;
    }
    (*form)->field = NULL;
    (*form)->entry = NULL;
    (*form)->entry_cursor = (*form)->entry_pos = 0;
    if ((*form)->entry_restore)
	free((*form)->entry_restore);
    (*form)->entry_restore = (char *) NULL;
}

/***************************************************************************/
/* T_FormCloseWindow                                                       */
/* Arguments :                                                             */
/*   win                                                                   */
/* Purpose : Deallocate all resources for a form and unmap window.         */
/***************************************************************************/
void T_FormCloseWindow(Window win)
{
    T_Form_t **form, *delform;
    T_Field_t *field, *delfield;

    /* traverse until we find the form correct window */
    form = SeekForm(win, 0);
    if ((*form) == NULL)
	return;

    XDestroyWindow(display, win);

    delform = (*form);
    (*form) = (*form)->next;

    /* traverse fields and free as we go */
    field = delform->field;
    while (field != NULL) {
	delfield = field->next;
	free(field);
	field = delfield;
    }
    free(delform);
}

/***************************************************************************/
/* SeekForm                                                                */
/* Arguments :                                                             */
/*   win                                                                   */
/*   add                                                                   */
/* Purpose : Seek to the form of the specified window. If it is not found, */
/*           add a blank form if add is set. Return address of pointer     */
/*           to form.                                                      */
/***************************************************************************/
T_Form_t **SeekForm(Window win, short add)
{
    T_Form_t **form;

    /* traverse until we find the form with the specified window */
    form = &T_Form;
    while (((*form) != NULL) && ((*form)->window != win)) {
	form = &((*form)->next);
    }

    /* if we are at the end add a window */
    if (((*form) == NULL) && (add)) {
	(*form) = (T_Form_t *) malloc(sizeof(T_Form_t));
	(*form)->window = win;
	(*form)->field = NULL;
	(*form)->entry = NULL;
	(*form)->entry_cursor = (*form)->entry_pos = 0;
	(*form)->entry_restore = NULL;
	(*form)->next = NULL;
    }

    return form;
}

/***************************************************************************/
/* ChangeField                                                             */
/* Arguments :                                                             */
/* Purpose : Change or add a field with the specified information.         */
/***************************************************************************/
void ChangeField(Window win, const char *name, const char *label,
		 short type, short active, short x, short y, short width,
		 short height, short x2, short y2, handler_t handler,
		 int *intvar, char *charvar, int charvar_length,
		 short null)
{
    T_Form_t **form;
    T_Field_t **field, *next = NULL;

    if ((win == 0) || (name == NULL))
	return;
    form = SeekForm(win, 1);
    field = &((*form)->field);
    while (((*field) != NULL) && (strcmp((*field)->name, name))) {
	field = &((*field)->next);
    }
    if ((*field) != NULL) {
	next = (*field)->next;
	free(*field);
    }
    (*field) = (T_Field_t *) malloc(sizeof(T_Field_t));
    (*field)->name = malloc(strlen(name) + 1);
    strcpy((*field)->name, name);
    if (label != NULL) {
	(*field)->label = malloc(strlen(label) + 1);
	strcpy((*field)->label, label);
    } else {
	(*field)->label = NULL;
    }
    (*field)->type = type;
    (*field)->active = active;
    (*field)->x = x;
    (*field)->y = y;
    (*field)->width = width;
    (*field)->height = height;
    (*field)->x2 = x2;
    (*field)->y2 = y2;
    (*field)->handler = handler;
    (*field)->intvar = intvar;
    (*field)->charvar = charvar;
    (*field)->charvar_length = charvar_length;
    (*field)->null = null;
    (*field)->next = next;
}

/***************************************************************************/
/* T_FormHoldButton                                                        */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   label                                                                 */
/*   handler                                                               */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormHoldButton(Window win, const char *name, short x, short y,
		      short width, short height, const char *label,
		      handler_t handler)
{
    ChangeField(win, name, label, T_HOLD_BUTTON, ACTIVE, x, y, width,
		height, 0, 0, handler, (int *) NULL, (char *) NULL, 0, 0);
}

/***************************************************************************/
/* T_FormButton                                                            */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   label                                                                 */
/*   handler                                                               */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormButton(Window win, const char *name, short x, short y,
		  short width, short height, const char *label,
		  handler_t handler)
{
    ChangeField(win, name, label, T_BUTTON, ACTIVE, x, y, width, height, 0,
		0, handler, (int *) NULL, (char *) NULL, 0, 0);
}

/***************************************************************************/
/* T_FormMultiButton                                                       */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   x2                                                                    */
/*   y2                                                                    */
/*   label                                                                 */
/*   intvar                                                                */
/*   null                                                                  */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormMultiButton(Window win, const char *name, short x, short y,
		       short width, short height, short x2, short y2,
		       const char *label, int *intvar, short null)
{
    ChangeField(win, name, label, T_MULTI_BUTTON, ACTIVE, x, y, width,
		height, x2, y2, NULL, intvar, (char *) NULL, 0, null);
}

/***************************************************************************/
/* T_FormScrollArea                                                        */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   type                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   handler                                                               */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormScrollArea(Window win, const char *name, short type, short x, short y,
		      short width, short height, handler_t handler)
{
    ChangeField(win, name, NULL, type, ACTIVE, x, y, width, height, 0, 0,
		handler, (int *) NULL, (char *) NULL, 0, 0);
}

/***************************************************************************/
/* T_FormText                                                              */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   label                                                                 */
/*   justify                                                               */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormText(Window win, const char *name, short x, short y,
		short width, short height, const char *label, short justify)
{
    ChangeField(win, name, label, T_TEXT, ACTIVE, x, y, width, height,
		justify, 0, NULL, (int *) NULL, (char *) NULL, 0, 0);
}

/***************************************************************************/
/* T_FormStringEntry                                                       */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/*   x                                                                     */
/*   y                                                                     */
/*   width                                                                 */
/*   height                                                                */
/*   x2                                                                    */
/*   y2                                                                    */
/*   label                                                                 */
/*   justify                                                               */
/* Purpose : Add a button to form of window win.                           */
/***************************************************************************/
void T_FormStringEntry(Window win, const char *name, short x, short y,
		       short width, short height, short x2, short y2,
		       const char *label, char *charvar, int charvar_length,
		       handler_t handler)
{
    ChangeField(win, name, label, T_STRING_ENTRY, ACTIVE, x, y, width,
		height, x2, y2, handler, (int *) NULL, charvar,
		charvar_length, 0);
}

/***************************************************************************/
/* T_DrawEntryField                                                        */
/* Arguments :                                                             */
/* Purpose : Deallocate all resources for a form and unmap window.         */
/***************************************************************************/
void T_DrawEntryField(T_Form_t * form, T_Field_t * field)
{
    char *tmpstr;

    if (field == (T_Field_t *) form->entry) {
	tmpstr = field->charvar;
	tmpstr += form->entry_pos;
	T_DrawButton(form->window, field->x, field->y, field->width,
		     field->height, RAISED, 1);
	T_DrawString(form->window, field->x + 5, field->y,
		     field->width - 10, field->height, BKGR, tmpstr,
		     JUSTIFY_LEFT, CROP_RIGHT,
		     form->entry_cursor - form->entry_pos);
    } else {
	T_DrawButton(form->window, field->x, field->y, field->width,
		     field->height, LOWERED, 1);
	T_DrawString(form->window, field->x + 5, field->y,
		     field->width - 10, field->height, BKGR,
		     field->charvar, JUSTIFY_LEFT, CROP_RIGHT, -1);
    }
}

/***************************************************************************/
/* T_SetEntryField                                                         */
/* Arguments :                                                             */
/*   form                                                                  */
/*    field                                                                */
/*    x                                                                    */
/* Purpose : Set global variables that point to the current text entry     */
/*           field. Popup the old text entry field and execute the         */
/*           function if it is not NULL.                                   */
/***************************************************************************/
void T_SetEntryField(T_Form_t * form, T_Field_t * field, int x)
{
    int length;
    char *tmpstr, *charvar;

    if (field == (T_Field_t *) form->entry) {
	tmpstr = field->charvar;
	tmpstr += form->entry_pos;
	length = strlen(tmpstr);
	while ((x < XTextWidth(T_Font, tmpstr, length))
	       && (length > 0))
	    length--;
	form->entry_cursor = form->entry_pos + length;
	T_DrawEntryField(form, (T_Field_t *) form->entry);

    } else {
	if (form->entry != NULL) {
	    /* BG hackfix: remember what our window is. */
	    Window form_window = form->window;

	    charvar = form->entry->charvar;
	    CallFieldHandler(form, (T_Field_t *) form->entry, 0, 0, 0, 0,
			     form->entry->handler);

	    /* BG hackfix:
	     * If this form was a popup and if the popup was destroyed by
	     * the form entry handler then we get out of here immediately.
	     */
	    if (*SeekForm(form_window, 0) != form) {
		return;
	    }
	    form->entry = NULL;
	    T_FormRedrawEntryField(charvar);
	    free(form->entry_restore);
	}
	form->entry = field;
	if (form->entry == NULL)
	    return;

	form->entry_restore = malloc(strlen(field->charvar) + 1);
	strcpy(form->entry_restore, field->charvar);
	length = strlen(form->entry->charvar);
	while ((x < XTextWidth(T_Font, form->entry->charvar, length)) &&
	       (length > 0)) {
	    length--;
	}
	form->entry_cursor = length;
	form->entry_pos = 0;
	T_DrawEntryField(form, field);
    }
}

/***************************************************************************/
/* T_FormRedrawEntryField                                                  */
/* Arguments :                                                             */
/*   charvar                                                               */
/* Purpose : Redraw fields containing charvar.                             */
/***************************************************************************/
void T_FormRedrawEntryField(char *charvar)
{
    T_Form_t *form;
    T_Field_t *field;

    form = T_Form;

    while (form != NULL) {
	field = form->field;
	while (field != NULL) {
	    if ((field->type == T_STRING_ENTRY)
		&& (field->charvar == charvar)
		&& (field->active != INACTIVE)) {
		T_DrawEntryField(form, field);
	    }
	    field = field->next;
	}
	form = form->next;
    }
}
