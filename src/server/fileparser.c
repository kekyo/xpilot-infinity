/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 1991-2001 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
 *      Dick Balaska         <dick@xpilot.org>
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

#include "xpserver.h"

static char    *FileName;
static int      LineNumber;


/*
 * Skips to the end of the line.
 */
static void toeol(char **map_ptr)
{
    int ich;

    while (**map_ptr) {
	ich = **map_ptr;
	(*map_ptr)++;
	if (ich == '\n') {
	    ++LineNumber;
	    return;
	}
    }
}


/*
 * Skips to the first non-whitespace character, returning that character.
 */
static int skipspace(char **map_ptr)
{
    int ich;

    while (**map_ptr) {
	ich = **map_ptr;
	(*map_ptr)++;
	if (ich == '\n') {
	    ++LineNumber;
	    return ich;
	}
	if (!isascii(ich) || !isspace(ich))
	    return ich;
    }
    return '\0';
}


/*
 * Read in a multiline value.
 */
static char *getMultilineValue(char **map_ptr, char *delimiter)
{
    char *s = XMALLOC(char, 32768);
    size_t i = 0, slen = 32768;
    char *bol;
    int ich;

    bol = s;
    for (;;) {
	ich = **map_ptr;
	(*map_ptr)++;
	if (ich == '\0') {
	    s = (char *)realloc(s, i + 1);
	    s[i] = '\0';
	    return s;
	}
	if (i == slen) {
	    char *t = s;

	    slen += (slen / 2) + 8192;
	    s = (char *)realloc(s, slen);
	    bol += s - t;
	}
	if (ich == '\n') {
	    s[i] = 0;
	    if (delimiter && !strcmp(bol, delimiter)) {
		char *t = s;

		s = (char *)realloc(s, (size_t) (bol - s + 1));
		s[bol - t] = '\0';
		return s;
	    }
	    bol = &s[i + 1];
	    ++LineNumber;
	}
	s[i++] = ich;
    }
}


/*
 * Parse a standard line from a defaults file, in the form Name: value.
 * Name must start at the beginning of the line with an alphabetic character.
 * Whitespace within name is ignored. Value may contain any character other
 * than # or newline, but leading and trailing whitespace are discarded.
 *
 * Characters after a # are ignored - this can be used for comments.
 *
 * If value begins with \override:, the override flag is set when
 * Option_set_value is called, so that this value will override
 * an existing value.
 *
 * If value starts with \multiline:, then the rest of the line is used
 * as a delimiter, and subsequent lines are read and saved as the value
 * until the delimiter is encountered.   No interpretation is done on
 * the text in the multiline sequence, so # does not serve as a comment
 * character there, and newlines and whitespace are not discarded.
 *
 * Lines can also be in the form "define: name value".
 * And similarly "define: name \multiline: TAG".
 *
 * Names can be macros which are expanded inplace with the keyword expand like:
 * expand: name
 *
 */
#define EXPAND				\
    if (i == slen)			\
	s = (char *)realloc(s, slen *= 2);

static void parseLine(char **map_ptr, optOrigin opt_origin)
{
    int ich, override = 0, multiline = 0;
    char *value, *head, *name, *s = XMALLOC(char, 128), *p;
    size_t slen = 128, i = 0;

    ich = **map_ptr;
    (*map_ptr)++;

    /*
     * Skip blank lines... 
     */
    if (ich == '\n') {
	++LineNumber;
	free(s);
	return;
    }
    /*
     * Skip leading space... 
     */
    if (isascii(ich) && isspace(ich)) {
	ich = skipspace(map_ptr);
	if (ich == '\n') {
	    free(s);
	    return;
	}
    }
    /*
     * Skip lines that start with comment character... 
     */
    if (ich == '#') {
	toeol(map_ptr);
	free(s);
	return;
    }
    /*
     * Skip lines that start with the end of the file... :') 
     */
    if (ich == '\0') {
	free(s);
	return;
    }
    /*
     *** I18nize? *** */
    if (!isascii(ich) || !isalpha(ich)) {
	error("%s line %d: Names must start with an alphabetic.\n",
	      FileName, LineNumber);
	toeol(map_ptr);
	free(s);
	return;
    }
    s[i++] = ich;
    do {
	ich = **map_ptr;
	(*map_ptr)++;

	if (ich == '\n' || ich == '#' || ich == '\0') {
	    error("%s line %d: No colon found on line.\n",
		  FileName, LineNumber);
	    if (ich == '#')
		toeol(map_ptr);
	    else
		++LineNumber;
	    free(s);
	    return;
	}
	if (isascii(ich) && isspace(ich))
	    continue;
	if (ich == ':')
	    break;
	EXPAND;
	s[i++] = ich;
    } while (1);

    ich = skipspace(map_ptr);

    EXPAND;
    s[i++] = '\0';
    name = s;

    slen = 128;
    s = XMALLOC(char, slen);
    i = 0;
    do {
	EXPAND;
	s[i++] = ich;
	ich = **map_ptr;
	(*map_ptr)++;
    } while (ich != '\0' && ich != '#' && ich != '\n');

    if (ich == '\n')
	++LineNumber;

    if (ich == '#')
	toeol(map_ptr);

    EXPAND;
    s[i++] = 0;
    head = value = s;
    s = value + strlen(value) - 1;
    while (s >= value && isascii(*s) && isspace(*s))
	--s;
    *++s = 0;

    /*
     * Deal with 'define: MACRO \multiline: TAG'. 
     */
    if (strcmp(name, "define") == 0) {
	p = value;
	while (*p && isascii(*p) && !isspace(*p))
	    p++;
	*p++ = '\0';

	/*
	 * name becomes value 
	 */
	free(name);
	name = XMALLOC(char, (size_t) (p - value + 1));
	memcpy(name, value, (size_t) (p - value));
	name[p - value] = '\0';

	/*
	 * Move value to \multiline 
	 */
	while (*p && isspace(*p))
	    p++;
	value = p;
    }

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
	error("%s line %d: no value specified.\n", FileName, LineNumber);
	free(name);
	free(head);
	return;
    }
    if (multiline)
	value = getMultilineValue(map_ptr, value);

    /*
     * Deal with 'expand: MACRO'. 
     */
    if (strcmp(name, "expand") == 0)
	expandKeyword(value);

#ifdef REGIONS			/* not yet */
    /*
     * Deal with 'region: \multiline: TAG'. 
     */
    else if (strcmp(name, "region") == 0) {
	if (!multiline) {	/* Must be multiline. */
	    error("regions must use '\\multiline:'.\n");
	    free(name);
	    free(head);
	    return;
	}
	p = value;
	while (*p)
	    parseLine(&p, opt_origin);
    }
#endif
    else
	Option_set_value(name, value, override, opt_origin);

    if (multiline)
	free(value);
    free(name);
    free(head);
    return;
}

#undef EXPAND

/*
 * Parse a file containing defaults (and possibly a map).
 */
static bool parseOpenFile(FILE * ifile, optOrigin opt_origin)
{
    int n;
    size_t map_offset, map_size;
    char *map_buf;

    LineNumber = 1;

    /*
     * In case first map fails and this is another 
     */
    is_polygon_map = false;

    /*
     * First try the xp2 map format 
     */
    if (isXp2MapFile(ifile)) {
	is_polygon_map = true;
	return parseXp2MapFile(FileName, opt_origin);
    }

    /*
     * Using a 200 map sample, the average map size is 37k. This chunk
     * size could be increased to avoid lots of reallocs. 
     */
#define MAP_CHUNK_SIZE	8192

    map_offset = 0;
    map_size = 2 * MAP_CHUNK_SIZE;
    map_buf = XMALLOC(char, map_size + 1);
    if (!map_buf) {
	error("Not enough memory to read the map!");
	return false;
    }

    for (;;) {
	n = fread(&map_buf[map_offset], 1, map_size - map_offset, ifile);
	if (n < 0) {
	    error("Error reading map!");
	    free(map_buf);
	    return false;
	}
	if (n == 0)
	    break;
	map_offset += n;

	if (map_size - map_offset < MAP_CHUNK_SIZE) {
	    map_size += (map_size / 2) + MAP_CHUNK_SIZE;
	    map_buf = (char *)realloc(map_buf, map_size + 1);
	    if (!map_buf) {
		error("Not enough memory to read the map!");
		return false;
	    }
	}
    }

    map_buf = (char *)realloc(map_buf, map_offset + 1);
    map_buf[map_offset] = '\0';	/* EOF */

    if (isdigit(*map_buf)) {
	warn("%s is in old (v1.x) format, please convert it with mapmapper",
	     FileName);
	free(map_buf);
	return false;
    } else {
	/*
	 * Parse all the lines in the file. 
	 */
	char *map_ptr = map_buf;

	while (*map_ptr)
	    parseLine(&map_ptr, opt_origin);
    }

    free(map_buf);

    return true;
}


static int copyFilename(const char *file)
{
    XFREE(FileName);
    FileName = xp_strdup(file);
    return (FileName != 0);
}


static FILE *fileOpen(const char *file)
{
    FILE *fp = Xp_fopen(file, "r");

    if (fp) {
	if (!copyFilename(file)) {
	    fclose(fp);
	    fp = NULL;
	}
    }
    return fp;
}


static void fileClose(FILE *fp)
{
    fclose(fp);
    XFREE(FileName);
}


/*
 * Test if filename has the XPilot map extension.
 */
static bool hasMapExtension(const char *filename)
{
    int             fnlen = strlen(filename);
    if (fnlen > 4 && !strcmp(&filename[fnlen - 4], ".xp2"))
	return true;
    if (fnlen > 3 && !strcmp(&filename[fnlen - 3], ".xp"))
	return true;
    if (fnlen > 4 && !strcmp(&filename[fnlen - 4], ".map"))
	return true;
    return false;
}


/*
 * Test if filename has a directory component.
 */
static bool hasDirectoryPrefix(const char *filename)
{
    static const char sep = '/';

    return (strchr(filename, sep) != NULL ? true : false);
}


/*
 * Combine a directory and a file.
 * Returns new path as dynamically allocated memory.
 */
static char *fileJoin(const char *dir, const char *file)
{
    static const char sep = '/';
    char *path;

    path = XMALLOC(char, strlen(dir) + 1 + strlen(file) + 1);
    if (path)
	sprintf(path, "%s%c%s", dir, sep, file);
    return path;
}


/*
 * Combine a file and a filename extension.
 * Returns new path as dynamically allocated memory.
 */
static char *fileAddExtension(const char *file, const char *ext)
{
    char *path;

    path = XMALLOC(char, strlen(file) + strlen(ext) + 1);
    if (path)
	sprintf(path, "%s%s", file, ext);
    return path;
}


#ifdef CONF_COMPRESSED_MAPS
static bool     usePclose = false;


static bool isCompressed(const char *filename)
{
    int fnlen = strlen(filename);
    int celen = strlen(Conf_zcat_ext());

    if (fnlen > celen && !strcmp(&filename[fnlen - celen], Conf_zcat_ext()))
	return true;
    return false;
}


static void closeCompressedFile(FILE * fp)
{
    if (usePclose) {
	pclose(fp);
	usePclose = false;
	XFREE(FileName);
    } else
	fileClose(fp);
}


static FILE *openCompressedFile(const char *filename)
{
    FILE *fp = NULL;
    char *cmdline = NULL;
    char *newname = NULL;

    usePclose = false;
    if (!isCompressed(filename)) {
	if (Xp_access(filename, 4) == 0)
	    return fileOpen(filename);
	newname = fileAddExtension(filename, Conf_zcat_ext());
	if (!newname)
	    return NULL;
	filename = newname;
    }
    if (Xp_access(filename, 4) == 0) {
	cmdline = XMALLOC(char,
			  strlen(CONF_ZCAT_FORMAT) + strlen(filename) + 1);
	if (cmdline) {
	    sprintf(cmdline, CONF_ZCAT_FORMAT, filename);
	    fp = popen(cmdline, "r");
	    if (fp) {
		usePclose = true;
		if (!copyFilename(filename)) {
		    closeCompressedFile(fp);
		    fp = NULL;
		}
	    }
	}
    }
    XFREE(newname);
    XFREE(cmdline);
    return fp;
}

#else

static bool isCompressed(const char *filename)
{
    return false;
}

static void closeCompressedFile(FILE * fp)
{
    fileClose(fp);
}

static FILE *openCompressedFile(const char *filename)
{
    return fileOpen(filename);
}
#endif

/*
 * Open a map file.
 * Filename argument need not contain map filename extension
 * or compress filename extension.
 * The search order should be:
 *      filename
 *      filename.gz                   if CONF_COMPRESSED_MAPS is true
 *      filename.xp2
 *      filename.xp2.gz               if CONF_COMPRESSED_MAPS is true
 *      filename.xp
 *      filename.xp.gz                if CONF_COMPRESSED_MAPS is true
 *      filename.map
 *      filename.map.gz               if CONF_COMPRESSED_MAPS is true
 *      CONF_MAPDIR filename
 *      CONF_MAPDIR filename.gz       if CONF_COMPRESSED_MAPS is true
 *      CONF_MAPDIR filename.xp2
 *      CONF_MAPDIR filename.xp2.gz   if CONF_COMPRESSED_MAPS is true
 *      CONF_MAPDIR filename.xp
 *      CONF_MAPDIR filename.xp.gz    if CONF_COMPRESSED_MAPS is true
 *      CONF_MAPDIR filename.map
 *      CONF_MAPDIR filename.map.gz   if CONF_COMPRESSED_MAPS is true
 */
static FILE *openMapFile(const char *filename)
{
    FILE *fp = NULL;
    char *newname, *newpath;

    fp = openCompressedFile(filename);
    if (fp)
	return fp;
    if (!isCompressed(filename)) {
	if (!hasMapExtension(filename)) {
	    newname = fileAddExtension(filename, ".xp2");
	    fp = openCompressedFile(newname);
	    free(newname);
	    if (fp)
		return fp;
	    newname = fileAddExtension(filename, ".xp");
	    fp = openCompressedFile(newname);
	    free(newname);
	    if (fp)
		return fp;
	    newname = fileAddExtension(filename, ".map");
	    fp = openCompressedFile(newname);
	    free(newname);
	    if (fp)
		return fp;
	}
    }
    if (!hasDirectoryPrefix(filename)) {
	newpath = fileJoin(Conf_mapdir(), filename);
	if (!newpath)
	    return NULL;
	if (hasDirectoryPrefix(newpath))
	    /*
	     * call recursively. 
	     */
	    fp = openMapFile(newpath);
	free(newpath);
	if (fp)
	    return fp;
    }
    return NULL;
}


static void closeMapFile(FILE *fp)
{
    closeCompressedFile(fp);
}


static FILE *openDefaultsFile(const char *filename)
{
    return fileOpen(filename);
}


static void closeDefaultsFile(FILE *fp)
{
    fileClose(fp);
}


/*
 * Parse a file containing defaults.
 */
bool parseDefaultsFile(const char *filename)
{
    FILE *ifile;
    bool result;

    if ((ifile = openDefaultsFile(filename)) == NULL)
	return false;

    result = parseOpenFile(ifile, OPT_DEFAULTS);
    closeDefaultsFile(ifile);

    return true;
}


/*
 * Parse a file containing password.
 */
bool parsePasswordFile(const char *filename)
{
    FILE *ifile;
    bool result;

    if ((ifile = openDefaultsFile(filename)) == NULL)
	return false;

    result = parseOpenFile(ifile, OPT_PASSWORD);
    closeDefaultsFile(ifile);

    return true;
}


/*
 * Parse a file containing a map.
 */
bool parseMapFile(const char *filename)
{
    FILE *ifile;
    bool result;

    if ((ifile = openMapFile(filename)) == NULL)
	return false;

    result = parseOpenFile(ifile, OPT_MAP);
    closeMapFile(ifile);

    return result;
}


void expandKeyword(const char *keyword)
{
    optOrigin expand_origin;
    char *p;

    p = Option_get_value(keyword, &expand_origin);
    if (p == NULL)
	warn("Can't expand '%s' because it has not been defined.\n",
	     keyword);
    else {
	while (*p)
	    parseLine(&p, expand_origin);
    }
}
