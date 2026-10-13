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

#ifndef PROTO_H
#define PROTO_H

#include "T_Toolkit.h"

/*
 * Prototypes
 */

/* prototypes for main.c */
void SetDefaults(int argc, char *argv[]);
void ParseArgs(int argc, char *argv[]);
void ResetMap(void);
void SizeMapwin(void);
void SizeSmallMap(void);
void SizeMapIcons(int zoom);
void Setup_default_server_options(void);

/* prototypes for expose.c */
void DrawTools(void);
void DrawMap(int x, int y, int width, int height);
void DrawMapSection(int x, int y, int width, int height,
		    int xpos, int ypos);
void DrawMapPic(Window win, int x, int y, int picnum, int zoom);
void DrawSmallMap(void);
void UpdateSmallMap(int x, int y);
void DrawViewBox(void);
void DrawViewSeg(int x1, int y_1, int x2, int y2);

/* prototypes for events.c */
void MainEventLoop(void);
void MapwinKeyPress(XEvent * report);

/* prototypes for tools.c */
int DrawMapIcon(HandlerInfo_t info);
void SelectArea(int x, int y, int count);
void ChangeMapData(int x, int y, char icon, int save);
int MoveMapView(HandlerInfo_t info);
int ZoomOut(HandlerInfo_t info);
int ZoomIn(HandlerInfo_t info);
void SizeSelectBounds(int oldvx, int oldvy);
int ExitApplication(HandlerInfo_t info);
int SaveUndoIcon(int x, int y, char icon);
int Undo(HandlerInfo_t info);
void ClearUndo(void);
int NewMap(HandlerInfo_t info);
int ResizeWidth(HandlerInfo_t info);
int ResizeHeight(HandlerInfo_t info);
int OpenPreferencesPopup(HandlerInfo_t info);
int OpenMapInfoPopup(HandlerInfo_t info);
int OpenRobotsPopup(HandlerInfo_t info);
int OpenVisibilityPopup(HandlerInfo_t info);
int OpenCannonsPopup(HandlerInfo_t info);
int OpenRoundsPopup(HandlerInfo_t info);
int OpenInitItemsPopup(HandlerInfo_t info);
int OpenMaxItemsPopup(HandlerInfo_t info);
int OpenProbsPopup(HandlerInfo_t info);
int OpenScoringPopup(HandlerInfo_t info);
int ValidateCoordHandler(HandlerInfo_t info);
int ShowHoles(HandlerInfo_t info);
char MapData(int x, int y);
int ChangedPrompt(handler_t handler);
void ClearSelectArea(void);
void DrawSelectArea(void);
int FillMapArea(HandlerInfo_t info);
int CopyMapArea(HandlerInfo_t info);
int CutMapArea(HandlerInfo_t info);
int PasteMapArea(HandlerInfo_t info);
int NegativeMapArea(HandlerInfo_t info);

/* prototypes for file.c */
int SavePrompt(HandlerInfo_t info);
int SaveOk(HandlerInfo_t info);
int SaveMap(char *file);
int LoadPrompt(HandlerInfo_t info);
int LoadOk(HandlerInfo_t info);
int LoadMap(char *file);
int LoadXbmFile(char *file);
int LoadOldMap(char *file);
void toeol(FILE * ifile);
char skipspace(FILE * ifile);
char *getMultilineValue(char *delimiter, FILE * ifile);
int ParseLine(FILE * ifile);
/** Apply an option without changing its name or value; return zero on success. */
int AddOption(const char *name, const char *value);
int YesNo(const char *val);
/**
 * Extract digits, an optional leading minus sign and at most one decimal point.
 * @param string Input text, which is not modified.
 * @param len Maximum output length, excluding the terminator; nonpositive means empty.
 * @param type INT, POSINT, FLOAT or POSFLOAT; positive types discard minus signs.
 * @return An allocated string owned by the caller, or NULL on allocation failure.
 */
char *StrToNum(const char *string, int len, int type);
int LoadMapData(const char *value);

/* prototypes for round.c */
int RoundMapArea(HandlerInfo_t info);

/* prototypes for help.c */
int OpenHelpPopup(HandlerInfo_t info);
void BuildHelpForm(Window win, int helppage);
void DrawHelpWin(void);
int NextHelp(HandlerInfo_t info);
int PrevHelp(HandlerInfo_t info);

/* prototypes for grow.c */
int GrowMapArea(HandlerInfo_t info);

/* prototypes for forms.c */
void BuildMapwinForm(void);
void BuildPrefsForm(void);
void BuildPrefsSheet(int num);

#endif
