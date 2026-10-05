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

#include "xpcommon.h"

#ifdef _WINDOWS
#include "utf8_files.h"

#define WINDOWS_CONF_PATH_SIZE (32768 * 4 + sizeof(CONF_SOUNDFILE))
static bool utf8_paths = false;

void Conf_use_utf8_paths(void)
{
    utf8_paths = true;
}

static void Conf_windows_path(char *path, size_t path_size,
			      const char *relative_path)
{
    char executable_path[MAX_PATH];
    char *forward_separator;
    char *back_separator;
    char *last_separator;
    DWORD executable_path_length;
    int written;

    if (utf8_paths) {
        wchar_t *wide_path = malloc(32768 * sizeof(wchar_t));
        DWORD length = wide_path ? GetModuleFileNameW(NULL, wide_path, 32768) : 0;
        wchar_t *separator = length && length < 32768 ? wcsrchr(wide_path, L'\\') : NULL;
        char *utf8 = NULL;
        if (separator) {
            separator[1] = L'\0';
            utf8 = Xp_utf8(wide_path);
        }
        written = snprintf(path, path_size, "%s%s", utf8 ? utf8 : "", relative_path);
        free(utf8); free(wide_path);
        if (written < 0 || (size_t)written >= path_size)
            strlcpy(path, relative_path, path_size);
        return;
    }

    executable_path_length = GetModuleFileNameA(NULL, executable_path,
						NELEM(executable_path));
    if (executable_path_length == 0
	|| executable_path_length >= NELEM(executable_path)) {
	strlcpy(path, relative_path, path_size);
	return;
    }

    forward_separator = strrchr(executable_path, '/');
    back_separator = strrchr(executable_path, '\\');
    if (forward_separator == NULL)
	last_separator = back_separator;
    else if (back_separator == NULL || forward_separator > back_separator)
	last_separator = forward_separator;
    else
	last_separator = back_separator;
    if (last_separator == NULL) {
	strlcpy(path, relative_path, path_size);
	return;
    }

    last_separator[1] = '\0';
    written = snprintf(path, path_size, "%s%s", executable_path,
		       relative_path);
    if (written < 0 || (size_t)written >= path_size)
	strlcpy(path, relative_path, path_size);
}

#define WINDOWS_CONF_FUNCTION(function_name, relative_path) \
char *function_name(void) \
{ \
    static char path[WINDOWS_CONF_PATH_SIZE]; \
    if (path[0] == '\0') \
	Conf_windows_path(path, sizeof(path), relative_path); \
    return path; \
}

WINDOWS_CONF_FUNCTION(Conf_datadir, CONF_DATADIR)
WINDOWS_CONF_FUNCTION(Conf_defaults_file_name, CONF_DEFAULTS_FILE_NAME)
WINDOWS_CONF_FUNCTION(Conf_password_file_name, CONF_PASSWORD_FILE_NAME)
WINDOWS_CONF_FUNCTION(Conf_mapdir, CONF_MAPDIR)
WINDOWS_CONF_FUNCTION(Conf_fontdir, CONF_FONTDIR)

#else

char *Conf_datadir(void)
{
    static char conf[] = CONF_DATADIR;

    return conf;
}

char *Conf_defaults_file_name(void)
{
    static char conf[] = CONF_DEFAULTS_FILE_NAME;

    return conf;
}

char *Conf_password_file_name(void)
{
    static char conf[] = CONF_PASSWORD_FILE_NAME;

    return conf;
}

#if 0
char *Conf_player_passwords_file_name(void)
{
    static char conf[] = CONF_PLAYER_PASSWORDS_FILE_NAME;

    return conf;
}
#endif

char *Conf_mapdir(void)
{
    static char conf[] = CONF_MAPDIR;

    return conf;
}

char *Conf_fontdir(void)
{
    static char conf[] = CONF_FONTDIR;

    return conf;
}

#endif

char *Conf_default_map(void)
{
    static char conf[] = CONF_DEFAULT_MAP;

    return conf;
}

char *Conf_servermotdfile(void)
{
#ifdef _WINDOWS
    static char conf[WINDOWS_CONF_PATH_SIZE];
#else
    static char conf[] = CONF_SERVERMOTDFILE;
#endif
    static char env[] = "XPILOTSERVERMOTD";
    char *filename;

    filename = getenv(env);
    if (filename == NULL) {
#ifdef _WINDOWS
	if (conf[0] == '\0')
	    Conf_windows_path(conf, sizeof(conf), CONF_SERVERMOTDFILE);
#endif
	filename = conf;
    }

    return filename;
}

#ifdef _WINDOWS
WINDOWS_CONF_FUNCTION(Conf_localmotdfile, CONF_LOCALMOTDFILE)
#else
char *Conf_localmotdfile(void)
{
    static char conf[] = CONF_LOCALMOTDFILE;

    return conf;
}
#endif

#ifdef _WINDOWS
char conf_logfile_string[WINDOWS_CONF_PATH_SIZE];
#else
char conf_logfile_string[] = CONF_LOGFILE;
#endif

char *Conf_logfile(void)
{
#ifdef _WINDOWS
    if (conf_logfile_string[0] == '\0')
	Conf_windows_path(conf_logfile_string, sizeof(conf_logfile_string),
			  CONF_LOGFILE);
#endif
    return conf_logfile_string;
}

#ifdef _WINDOWS
WINDOWS_CONF_FUNCTION(Conf_ship_file, CONF_SHIP_FILE)
WINDOWS_CONF_FUNCTION(Conf_texturedir, CONF_TEXTUREDIR)
#else
char *Conf_ship_file(void)
{
    static char conf[] = CONF_SHIP_FILE;

    return conf;
}

char *Conf_texturedir(void)
{
    static char conf[] = CONF_TEXTUREDIR;

    return conf;
}
#endif

char *Conf_localguru(void)
{
    static char conf[] = CONF_LOCALGURU;

    return conf;
}

#ifdef _WINDOWS
WINDOWS_CONF_FUNCTION(Conf_robotfile, CONF_ROBOTFILE)
#else
char *Conf_robotfile(void)
{
    static char conf[] = CONF_ROBOTFILE;

    return conf;
}
#endif

char *Conf_zcat_ext(void)
{
    static char conf[] = CONF_ZCAT_EXT;

    return conf;
}

char *Conf_zcat_format(void)
{
    static char conf[] = CONF_ZCAT_FORMAT;

    return conf;
}

#ifdef _WINDOWS
WINDOWS_CONF_FUNCTION(Conf_sounddir, CONF_SOUNDDIR)
WINDOWS_CONF_FUNCTION(Conf_soundfile, CONF_SOUNDFILE)
#else
char *Conf_sounddir(void)
{
    static char conf[] = CONF_SOUNDDIR;

    return conf;
}

char *Conf_soundfile(void)
{
    static char conf[] = CONF_SOUNDFILE;

    return conf;
}
#endif

#ifdef _WINDOWS
#undef WINDOWS_CONF_FUNCTION
#undef WINDOWS_CONF_PATH_SIZE
#endif


void Conf_print(void)
{
    warn("============================================================");
    warn("VERSION                   = %s", VERSION);
    warn("PACKAGE                   = %s", PACKAGE);

#ifdef DBE
    warn("DBE");
#endif
#ifdef MBX
    warn("MBX");
#endif
#ifdef PLOCKSERVER
    warn("PLOCKSERVER");
#endif
#ifdef DEVELOPMENT
    warn("DEVELOPMENT");
#endif

    warn("Conf_localguru()          = %s", Conf_localguru());
    warn("Conf_datadir()            = %s", Conf_datadir());
    warn("Conf_defaults_file_name() = %s", Conf_defaults_file_name());
    warn("Conf_password_file_name() = %s", Conf_password_file_name());
    warn("Conf_mapdir()             = %s", Conf_mapdir());
    warn("Conf_default_map()        = %s", Conf_default_map());
    warn("Conf_servermotdfile()     = %s", Conf_servermotdfile());
    warn("Conf_robotfile()          = %s", Conf_robotfile());
    warn("Conf_logfile()            = %s", Conf_logfile());
    warn("Conf_localmotdfile()      = %s", Conf_localmotdfile());
    warn("Conf_ship_file()          = %s", Conf_ship_file());
    warn("Conf_texturedir()         = %s", Conf_texturedir());
    warn("Conf_fontdir()            = %s", Conf_fontdir());
    warn("Conf_sounddir()           = %s", Conf_sounddir());
    warn("Conf_soundfile()          = %s", Conf_soundfile());
    warn("Conf_zcat_ext()           = %s", Conf_zcat_ext());
    warn("Conf_zcat_format()        = %s", Conf_zcat_format());
    warn("============================================================");
}
