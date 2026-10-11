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

Window filepromptwin;
max_str_t filepromptname = "continent";

char oldmap[90] = {
    ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ',
    '_', '+', ' ', '-', ' ',
    ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', '<', ' ',
    '>', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', '#', ' ',
    ' ', '(', ' ', ' ', ' ',
    ' ', ' ', ')', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ',
    '@', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ',
    'a', ' ', 'c', 'd', ' ',
    'f', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ',
    ' ', 'q', 'r', 's', ' ',
    ' ', ' ', 'w', 'x', ' '
};

/***************************************************************************/
/* GetMapDir                                                               */
/* Arguments :                                                             */
/* Purpose :                                                               */
/***************************************************************************/
static char *GetMapDir(void)
{
    static char *mapdir;
    char *src, *dst;

    if (!mapdir) {
#if defined(MAPDIR)
	mapdir = (char *) malloc(sizeof(MAPDIR) + 1);
	strcpy(mapdir, MAPDIR);
#elif defined(LIBDIR)
	mapdir = (char *) malloc(sizeof(LIBDIR) + 10);
	strcpy(mapdir, LIBDIR);
	strcat(mapdir, "/maps");
#else
	static char default_mapdir[] = "/usr/local/games/lib/xpilot/maps";
	mapdir = (char *) malloc(sizeof(default_mapdir) + 1);
	strcpy(mapdir, default_mapdir);
#endif
	/* remove duplicate slashes. */
	for (dst = src = mapdir; (*dst = *src) != '\0'; src++) {
	    if (*dst != '/' || dst == mapdir || dst[-1] != '/') {
		dst++;
	    }
	}
	/* remove trailing slash. */
	if (dst > mapdir && dst[-1] == '/') {
	    *--dst = '\0';
	}
    }
    return mapdir;
}

/***************************************************************************/
/* SavePrompt                                                              */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int SavePrompt(HandlerInfo_t info)
{

    ClearSelectArea();
    T_PopupClose(changedwin);
    T_PopupClose(filepromptwin);

    strcpy(filepromptname, map.mapFileName);
    filepromptwin = T_PopupPrompt(-1, -1, 300, 150, "Save Map",
				  "Enter file name to save:", "Save", NULL,
				  filepromptname, sizeof(max_str_t),
				  SaveOk);
    return 0;
}

/***************************************************************************/
/* SaveOk                                                                  */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int SaveOk(HandlerInfo_t info)
{
    int len;

    len = strlen(filepromptname);

    if (len > 3) {
	if (strcmp(&filepromptname[len - 3], ".xp")) {
	    if (len > 4) {
		if (strcmp(&filepromptname[len - 4], ".map")) {
		    strcat(filepromptname, ".xp");
		}
	    } else {
		strcat(filepromptname, ".xp");
	    }
	}
    } else {
	strcat(filepromptname, ".xp");
    }

    SaveMap(filepromptname);
    T_PopupClose(info.form->window);
    map.changed = 0;
    return 0;
}

/***************************************************************************/
/* SaveMap                                                                 */
/* Arguments :                                                             */
/*   file                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int SaveMap(char *file)
{
    FILE *ofile = NULL;
    int n, i, j;
    char *tmpstr;
    time_t tim;

    if (strlen(file) == 0) {
	return 1;
    }
    ofile = fopen(file, "w");
    if (ofile == NULL) {
	tmpstr = (char *) malloc(strlen(file) + 20);
	sprintf(tmpstr, "Error saving file: %s", file);
	T_PopupAlert(1, tmpstr, NULL, NULL, NULL, NULL);
	free(tmpstr);
	return 1;
    }
    time(&tim);
    fprintf(ofile, "# Created by %s on %s\n", progname, ctime(&tim));
    if (map.comments != NULL) {
	fprintf(ofile, "%s\n", map.comments);
    }

    if ((map.worldLives != NULL) && (atoi(map.worldLives) != 0))
	fprintf(ofile, "limitedlives : yes\n");

    for (n = 0; n < numprefs; n++) {
	switch (prefs[n].type) {

	case MAPWIDTH:
	case MAPHEIGHT:
	case STRING:
	case INT:
	case POSINT:
	case FLOAT:
	case POSFLOAT:
	case COORD:

	    if (prefs[n].charvar[0] != '\0')
		fprintf(ofile, "%s : %s\n", prefs[n].name,
			prefs[n].charvar);
	    break;

	case MAPDATA:
	    fprintf(ofile, "\nmapData: \\multiline: EndOfMapdata\n");
	    for (i = 0; i < map.height; i++) {
		for (j = 0; j < map.width; j++) {
		    fprintf(ofile, "%c", map.data[j][i]);
		}
		fprintf(ofile, "\n");
	    }
	    fprintf(ofile, "EndOfMapdata\n");
	    break;

	case YESNO:
	    if ((*prefs[n].intvar) == 0)
		break;
	    if ((*prefs[n].intvar) == 1)
		fprintf(ofile, "%s : no\n", prefs[n].name);
	    else
		fprintf(ofile, "%s : yes\n", prefs[n].name);
	    break;
	}
    }

    fclose(ofile);
    strcpy(map.mapFileName, file);
    return 0;
}

/***************************************************************************/
/* LoadPrompt                                                              */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int LoadPrompt(HandlerInfo_t info)
{

    ClearSelectArea();
    if (ChangedPrompt(LoadPrompt)) {
	return 1;
    }
    if (T_IsPopupOpen(changedwin)) {
	T_PopupClose(changedwin);
	changedwin = (Window) NULL;
    }
    if (T_IsPopupOpen(filepromptwin)) {
	T_PopupClose(filepromptwin);
    }
    filepromptname[0] = '\0';
    filepromptwin = T_PopupPrompt(-1, -1, 300, 150, "Load Map",
				  "Enter file name to load:", "Load", NULL,
				  filepromptname, sizeof(max_str_t),
				  LoadOk);
    return 0;
}

/***************************************************************************/
/* LoadOk                                                                  */
/* Arguments :                                                             */
/*   win                                                                   */
/*   name                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int LoadOk(HandlerInfo_t info)
{
    if (LoadMap(filepromptname)) {
	return 1;
    }
    T_PopupClose(info.form->window);
    map.view_x = map.view_y = 0;
    map.view_zoom = DEFAULT_MAP_ZOOM;
    map.changed = 0;
    ResetMap();
    ClearUndo();
    return 0;
}

/***************************************************************************/
/* LoadMap                                                                 */
/* Arguments :                                                             */
/*   file                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
int LoadMap(char *file)
{
    FILE *ifile = NULL;
    int ich;
    int corrupted = 0;
    char *filename, *tmpstr;
    int i, j;
    char *mapdir = GetMapDir();


    if (strlen(file) == 0)
	return 1;

    filename = (char *) malloc(strlen(file) + 1);
    strcpy(filename, file);
    ifile = fopen(filename, "r");	/* "FILE" */

    if (ifile == NULL) {
	free(filename);
	filename = (char *) malloc(strlen(file) + 4);
	sprintf(filename, "%s.xp", file);
	ifile = fopen(filename, "r");	/* "FILE.xp" */

	if (ifile == NULL) {
	    free(filename);
	    filename = (char *) malloc(strlen(file) + 5);
	    sprintf(filename, "%s.map", file);
	    ifile = fopen(filename, "r");	/* "FILE.map" */

	    if (ifile == NULL) {
		free(filename);
		filename =
		    (char *) malloc(strlen(file) + strlen(mapdir) + 2);
		sprintf(filename, "%s/%s", mapdir, file);
		ifile = fopen(filename, "r");	/* "MAPDIR/FILE" */

		if (ifile == NULL) {
		    free(filename);
		    filename =
			(char *) malloc(strlen(file) + strlen(mapdir) + 5);
		    sprintf(filename, "%s/%s.xp", mapdir, file);
		    ifile = fopen(filename, "r");	/* "MAPDIR/FILE.xp" */

		    if (ifile == NULL) {
			free(filename);
			filename =
			    (char *) malloc(strlen(file) + strlen(mapdir) +
					    6);
			sprintf(filename, "%s/%s.map", mapdir, file);
			ifile = fopen(filename, "r");	/* "MAPDIR/FILE.map" */

			if (ifile == NULL) {
			    tmpstr = (char *) malloc(strlen(file) + 21);
			    sprintf(tmpstr, "Couldn't find file: %s",
				    file);
			    T_PopupAlert(1, tmpstr, NULL, NULL, NULL,
					 NULL);
			    free(tmpstr);
			    return 1;
			}
		    }
		}
	    }
	}
    }
    if (map.comments)
	free(map.comments);
    map.comments = (char *) NULL;
    map.mapName[0] = map.mapAuthor[0] = map.gravity[0] = map.shipMass[0] =
	'\0';
    map.maxRobots[0] = map.worldLives[0] = '\0';
    map.view_zoom = DEFAULT_MAP_ZOOM;
    map.changed = map.edgeWrap = map.edgeBounce = map.teamPlay = 0;
    map.timing = 0;
    map.limitedVisibility = map.allowShields = 0;
    for (i = 0; i < MAX_MAP_SIZE; i++)
	for (j = 0; j < MAX_MAP_SIZE; j++)
	    map.data[i][j] = ' ';

    strcpy(map.mapFileName, filename);
    tmpstr = strrchr(filename, (int) '.');
    if (tmpstr != NULL) {
	if (strcmp(tmpstr, ".xbm") == 0) {
	    fclose(ifile);
	    return LoadXbmFile(filename);
	}
    }
    ich = getc(ifile);
    if (ich != EOF)
	ungetc(ich, ifile);
    if (isdigit(ich)) {
	fclose(ifile);
	return LoadOldMap(filename);
    } else {
	while (!feof(ifile)) {
	    if (ParseLine(ifile))
		corrupted = 1;
	}
    }
    if (ifile)
	fclose(ifile);
    if (corrupted) {
	T_PopupAlert(1, "Corrupted map file.", NULL, NULL, NULL, NULL);
    }
    return 0;
}

/***************************************************************************/
/* LoadXbmFile                                                             */
/* Arguments :                                                             */
/*   file                                                                  */
/* Purpose : Load a version 2 map                                          */
/***************************************************************************/
static bool ParseMapDimension(const char *text, int *dimension)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || value < 1 || value > MAX_MAP_SIZE)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;
    *dimension = (int)value;
    return true;
}

int LoadXbmFile(char *file)
{
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        T_PopupAlert(1, "Couldn't open bitmap file.", NULL, NULL, NULL, NULL);
        return 1;
    }
    max_str_t line, name, dimension;
    int width, height;
    if (!fgets(line, sizeof(line), fp) ||
        sscanf(line, "#define %254s %254s", name, dimension) != 2 ||
        !ParseMapDimension(dimension, &width) ||
        !fgets(line, sizeof(line), fp) ||
        sscanf(line, "#define %254s %254s", name, dimension) != 2 ||
        !ParseMapDimension(dimension, &height))
        goto corrupted;

    map.width = width;
    map.height = height;
    snprintf(map.width_str, sizeof(map.width_str), "%d", width);
    snprintf(map.height_str, sizeof(map.height_str), "%d", height);
    int x = 0, y = 0;
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while ((p = strstr(p, "0x")) != NULL) {
            char *end;
            p += 2;
            errno = 0;
            unsigned long bits = strtoul(p, &end, 16);
            if (y >= height || errno != 0 || end == p || bits > 255 ||
                (*end != ',' && *end != '}' && !isspace((unsigned char)*end)))
                goto corrupted;
            /* The final byte in each row may contain padding bits. */
            for (int bit = 0; bit < 8 && x + bit < width; bit++)
                map.data[x + bit][y] = (bits & (1UL << bit)) ? 'x' : ' ';
            x += 8;
            if (x >= width) {
                x = 0;
                y++;
            }
            p = end;
        }
    }
    if (ferror(fp) || y != height || x != 0)
        goto corrupted;
    fclose(fp);
    return 0;

corrupted:
    fclose(fp);
    T_PopupAlert(1, "Corrupted bitmap file.", NULL, NULL, NULL, NULL);
    return 1;
}

/***************************************************************************/
/* LoadOldMap                                                              */
/* Arguments :                                                             */
/*   file                                                                  */
/* Purpose : Load a version 2 map                                          */
/***************************************************************************/
int LoadOldMap(char *file)
{
    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        T_PopupAlert(1, "Couldn't open map file.", NULL, NULL, NULL, NULL);
        return 1;
    }
    max_str_t line;
    int width, height;
    /* read in map x and y size */
    if (!fgets(line, sizeof(line), fp))
        goto corrupted;
    char *separator = strchr(line, 'x');
    if (separator == NULL)
        goto corrupted;
    *separator++ = '\0';
    if (!ParseMapDimension(line, &width) || !ParseMapDimension(separator, &height))
        goto corrupted;
    map.width = width;
    map.height = height;
    snprintf(map.width_str, sizeof(map.width_str), "%d", width);
    snprintf(map.height_str, sizeof(map.height_str), "%d", height);
    /* read in map rule */
    if (!fgets(line, sizeof(line), fp))
        goto corrupted;
    char *end;
    errno = 0;
    long rule = strtol(line, &end, 10);
    if (errno != 0 || end == line)
        goto corrupted;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        goto corrupted;
    if (rule == 6)
        map.edgeWrap = 2;
    /* get map name and author */
    if (!fgets(line, sizeof(line), fp) || strchr(line, '\n') == NULL)
        goto corrupted;
    line[strcspn(line, "\r\n")] = '\0';
    strlcpy(map.mapName, line, sizeof(map.mapName));
    if (!fgets(line, sizeof(line), fp) || strchr(line, '\n') == NULL)
        goto corrupted;
    line[strcspn(line, "\r\n")] = '\0';
    strlcpy(map.mapAuthor, line, sizeof(map.mapAuthor));

    /* read in map */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int c = fgetc(fp);
            if (c == '.')
                c = ' ';
            if (c < 32 || c - 32 >= (int)sizeof(oldmap))
                goto corrupted;
            map.data[x][y] = oldmap[c - 32];
        }
        int c = fgetc(fp);
        if (c == '\r')
            c = fgetc(fp);
        if (c != '\n' && !(c == EOF && y == height - 1))
            goto corrupted;
    }
    if (ferror(fp))
        goto corrupted;
    fclose(fp);
    return 0;

corrupted:
    fclose(fp);
    T_PopupAlert(1, "Corrupted map file.", NULL, NULL, NULL, NULL);
    return 1;
}

/***************************************************************************/
/* toeol                                                                   */
/* Arguments :                                                             */
/*   ifile                                                                 */
/* Purpose :                                                               */
/***************************************************************************/
void toeol(FILE * ifile)
{
    int ich;

    while (!feof(ifile))
	if ((ich = getc(ifile)) == '\n') {
	    return;
	}
}

/***************************************************************************/
/* skipspace                                                               */
/* Arguments :                                                             */
/*   ifile                                                                 */
/* Purpose :                                                               */
/***************************************************************************/
char skipspace(FILE * ifile)
{
    int ich;

    while (!feof(ifile)) {
	ich = getc(ifile);
	if (ich == '\n') {
	    return ich;
	}
	if (!isascii(ich) || !isspace(ich))
	    return ich;
    }
    return EOF;
}

/***************************************************************************/
/* char *getMultilineValue                                                 */
/* Arguments :                                                             */
/*   delimiter                                                             */
/*    ifile                                                                */
/* Purpose :                                                               */
/***************************************************************************/
char *getMultilineValue(char *delimiter, FILE * ifile)
{
    char *s = (char *) malloc(32768);
    int i = 0;
    int slen = 32768;
    char *bol;
    int ich;

    bol = s;
    while (1) {
	ich = getc(ifile);
	if (ich == EOF) {
	    s = (char *) realloc(s, i + 1);
	    s[i] = '\0';
	    return s;
	}
	if (i == slen) {
	    char *t = s;

	    s = (char *) realloc(s, slen += 32768);
	    bol += s - t;
	}
	if (ich == '\n') {
	    s[i] = 0;
	    if (delimiter && !strcmp(bol, delimiter)) {
		char *t = s;

		s = (char *) realloc(s, bol - s + 1);
		s[bol - t] = '\0';
		return s;
	    }
	    bol = &s[i + 1];
	}
	s[i++] = ich;
    }
}

#define                  EXPAND                        \
if (i == slen) {                   \
   s = (char *) realloc(s, slen *= 2);      \
}
/***************************************************************************/
/* ParseLine                                                               */
/* Arguments :                                                             */
/*   ifile                                                                 */
/* Purpose :                                                               */
/***************************************************************************/
int ParseLine(FILE * ifile)
{
    int ich;
    char *value, *head, *name, *s = (char *) malloc(128);
    char *tmp, *commentline;
    int slen = 128;
    int i = 0;
    int override = 0;
    int multiline = 0;

    ich = getc(ifile);

    /* Skip blank lines... */
    if (ich == '\n') {
	free(s);
	return 0;
    }
    /* Skip leading space... */
    if (isascii(ich) && isspace(ich)) {
	ich = skipspace(ifile);
	if (ich == '\n') {
	    free(s);
	    return 0;
	}
    }
    /* Skip lines that start with comment character... */
    if (ich == '#') {
	commentline = malloc(2);
	sprintf(commentline, "#");
	ich = getc(ifile);
	while ((ich != EOF) && (ich != '\n')) {
	    tmp = malloc(strlen(commentline) + 2);
	    sprintf(tmp, "%s%c", commentline, ich);
	    free(commentline);
	    commentline = tmp;
	    ich = getc(ifile);
	}
	if (ich == '\n') {
	    tmp = malloc(strlen(commentline) + 2);
	    sprintf(tmp, "%s\n", commentline);
	    free(commentline);
	    commentline = tmp;
	}

	/* only add comment lines not created by xmapedit */
	if (strstr(commentline, "Created by") == NULL) {
	    if (map.comments == NULL) {
		map.comments = commentline;
	    } else {
		tmp =
		    malloc(strlen(map.comments) + strlen(commentline) + 1);
		sprintf(tmp, "%s%s", map.comments, commentline);
		free(map.comments);
		free(commentline);
		map.comments = tmp;
	    }
	} else
	    free(commentline);
	free(s);
	return 0;
    }
    /* Skip lines that start with the end of the file... :') */
    if (ich == EOF) {
	free(s);
	return 0;
    }
    /* Start with ascii? */
    if (!isascii(ich) || !isalpha(ich)) {
	toeol(ifile);
	free(s);
	return 1;
    }
    s[i++] = ich;
    do {
	ich = getc(ifile);
	if (ich == '\n' || ich == '#' || ich == EOF) {
	    if (ich == '#')
		toeol(ifile);
	    free(s);
	    return 1;
	}
	if (isascii(ich) && isspace(ich))
	    continue;
	if (ich == ':')
	    break;
	EXPAND;
	s[i++] = ich;
    } while (1);

    ich = skipspace(ifile);

    EXPAND;
    s[i++] = '\0';
    name = s;

    s = (char *) malloc(slen = 128);
    i = 0;
    do {
	EXPAND;
	s[i++] = ich;
	ich = getc(ifile);
    } while (ich != EOF && ich != '#' && ich != '\n');

    if (ich == '#')
	toeol(ifile);

    EXPAND;
    s[i++] = 0;
    head = value = s;
    s = value + strlen(value) - 1;
    while (s >= value && isascii(*s) && isspace(*s))
	--s;
    *++s = 0;
    if (!strncmp(value, "\\override:", 10)) {
	override = 1;
	value += 10;
    }
    while (*value && isascii(*value) && isspace(*value))
	++value;
    if (!strncmp(value, "\\multiline:", 11)) {
	multiline = 1;
	value += 11;
    }
    while (*value && isascii(*value) && isspace(*value))
	++value;
    if (!*value) {
	free(name);
	free(head);
	return 1;
    }
    if (multiline)
	value = (char *) getMultilineValue(value, ifile);
    i = AddOption(name, value);

    free(name);
    free(head);
    return i;
}

/***************************************************************************/
/* AddOption                                                               */
/* Arguments :                                                             */
/*   name                                                                  */
/*    value                                                                */
/* Purpose :                                                               */
/***************************************************************************/
int AddOption(const char *name, const char *value)
{
    int option;
    char *tmp;

    for (option = 0; option < numprefs; option++) {
	if (!strcasecmp(name, prefs[option].name))
	    break;
	if (!strcasecmp(name, prefs[option].altname))
	    break;
    }
    if (option >= numprefs) {
	if (map.comments == NULL) {
	    map.comments = malloc(strlen(name) + strlen(value) + 3);
	    sprintf(map.comments, "%s:%s\n", name, value);
	} else {
	    tmp =
		malloc(strlen(map.comments) + strlen(name) +
		       strlen(value) + 3);
	    sprintf(tmp, "%s%s:%s\n", map.comments, name, value);
	    free(map.comments);
	    map.comments = tmp;
	}
	return 0;
    }

    switch (prefs[option].type) {

    case MAPDATA:
	return (LoadMapData(value));

    case MAPWIDTH:
	if (!ParseMapDimension(value, &map.width))
	    return 1;
	strlcpy(map.width_str, value, sizeof(map.width_str));
	return 0;

    case MAPHEIGHT:
	if (!ParseMapDimension(value, &map.height))
	    return 1;
	strlcpy(map.height_str, value, sizeof(map.height_str));
	return 0;

    case STRING:
    case COORD:
	strlcpy(prefs[option].charvar, value, prefs[option].length + 1);
	return 0;

    case YESNO:
	(*(prefs[option].intvar)) = YesNo(value) + 1;
	return 0;

    case INT:
    case POSINT:
    case FLOAT:
    case POSFLOAT:
	tmp = StrToNum(value, prefs[option].length, prefs[option].type);
	if (tmp == NULL)
	    return 1;
	strcpy(prefs[option].charvar, tmp);
	free(tmp);
	return 0;
    }
    return 1;
}

/***************************************************************************/
/* YesNo                                                                   */
/* Arguments :                                                             */
/*   val                                                                   */
/* Purpose :                                                               */
/***************************************************************************/
int YesNo(const char *val)
{
    if ((tolower(val[0]) == 'y') || (tolower(val[0]) == 't'))
	return 1;
    return 0;
}

/***************************************************************************/
/* char *StrToNum                                                          */
/* Arguments :                                                             */
/*   string                                                                */
/*   len                                                                   */
/*   type                                                                  */
/* Purpose :                                                               */
/***************************************************************************/
char *StrToNum(const char *string, int len, int type)
{
    size_t capacity = len > 0 ? (size_t)len : 0;
    char *returnval = malloc(capacity + 1);
    if (returnval == NULL)
	return NULL;
    size_t used = 0;
    bool decimal = false;
    for (const char *p = string; *p != '\0' && used < capacity; p++) {
	if (*p >= '0' && *p <= '9')
	    returnval[used++] = *p;
	else if (*p == '-' && p == string && (type == FLOAT || type == INT))
	    returnval[used++] = *p;
	else if (*p == '.' && !decimal && (type == FLOAT || type == POSFLOAT)) {
	    /* Floating-point fields accept digits and a single decimal point. */
	    returnval[used++] = *p;
	    decimal = true;
	}
    }
    returnval[used] = '\0';
    return returnval;
}

/***************************************************************************/
/* LoadMapData                                                             */
/* Arguments :                                                             */
/*   value                                                                 */
/* Purpose :                                                               */
/***************************************************************************/
int LoadMapData(const char *value)
{
    int x = 0, y = 0;

    while (*value != '\0') {
	if (*value == '\n') {
	    x = 0;
	    y++;
	} else
	    map.data[x++][y] = *value;
	value++;
    }
    return 0;
}
