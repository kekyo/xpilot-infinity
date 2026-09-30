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

#ifndef XPCONFIG_H
#define XPCONFIG_H

#ifdef _WINDOWS
#  /* kps - what about this ???? */
#  undef CONF_DATADIR
#  define CONF_DATADIR			"lib/"
#endif

#ifndef CONF_DATADIR
#  error "CONF_DATADIR NOT DEFINED. GIVING UP."
#endif

#ifndef CONF_LOCALGURU
#  define CONF_LOCALGURU		PACKAGE_BUGREPORT
#endif

#ifndef CONF_DEFAULT_MAP
#  define CONF_DEFAULT_MAP		"ndh.xp2"
#endif

#ifndef CONF_MAPDIR
#  define CONF_MAPDIR			CONF_DATADIR "maps/"
#endif

#ifndef CONF_TEXTUREDIR
#  define CONF_TEXTUREDIR		CONF_DATADIR "textures/"
#endif

#ifndef CONF_SOUNDDIR
#  define CONF_SOUNDDIR			CONF_DATADIR "sound/"
#endif

#ifndef CONF_FONTDIR
#  define CONF_FONTDIR			CONF_DATADIR "fonts/"
#endif

#ifndef CONF_DEFAULTS_FILE_NAME
#  define CONF_DEFAULTS_FILE_NAME	CONF_DATADIR "defaults.txt"
#endif

#ifndef CONF_PASSWORD_FILE_NAME
#  define CONF_PASSWORD_FILE_NAME	CONF_DATADIR "password.txt"
#endif

/* not used currently */
#ifndef CONF_PLAYER_PASSWORDS_FILE_NAME
#  define CONF_PLAYER_PASSWORDS_FILE_NAME CONF_DATADIR "player_passwords.txt"
#endif

#ifndef CONF_ROBOTFILE
#  define CONF_ROBOTFILE		CONF_DATADIR "robots.txt"
#endif

#ifndef CONF_SERVERMOTDFILE
#  define CONF_SERVERMOTDFILE		CONF_DATADIR "servermotd.txt"
#endif

#ifndef CONF_LOCALMOTDFILE
#  define CONF_LOCALMOTDFILE		CONF_DATADIR "localmotd.txt"
#endif

#ifndef CONF_LOGFILE
#  define CONF_LOGFILE			CONF_DATADIR "log.txt"
#endif

#ifndef CONF_SOUNDFILE
#  define CONF_SOUNDFILE	  	CONF_SOUNDDIR "sounds.txt"
#endif

#ifndef CONF_SHIP_FILE
#  define CONF_SHIP_FILE		CONF_DATADIR "shipshapes.txt"
#endif



#ifndef _WINDOWS

#  ifdef DEBUG
#    define D(x)	x ;  fflush(stdout);
#  else
#    define D(x)
#  endif

#else /* _WINDOWS */

#  ifdef _DEBUG
#    define DEBUG	1
#    define D(x)	x
#  else
#    define D(x)
#  endif

#endif /* _WINDOWS */

/*
 * Uncomment this if your machine doesn't use
 * two's complement negative numbers.
 */
/* #define MOD2(x, m) mod(x, m) */

/*
 * The following macros decide the speed of the game and
 * how often the server should draw a frame.  (Hmm...)
 */

#define CONF_UPDATES_PR_FRAME	1

/*
 * If COMPRESSED_MAPS is defined, the server will attempt to uncompress
 * maps on the fly (but only if neccessary). CONF_ZCAT_FORMAT should produce
 * a command that will unpack the given .gz file to stdout (for use in popen).
 * CONF_ZCAT_EXT should define the proper compressed file extension.
 */
#ifndef _WINDOWS
#  define CONF_COMPRESSED_MAPS
#else
/* Couldn't find a popen(), also compress and gzip don't exist. */
#  undef CONF_COMPRESSED_MAPS
#endif
#define CONF_ZCAT_EXT			".gz"
#define CONF_ZCAT_FORMAT 		"gzip -d -c < %s"

void Conf_print(void);
#ifdef _WINDOWS
/** Use UTF-8 for Windows installation paths.
 * @remarks Call before reading any Conf_* path. Intended for the server's
 * UTF-8 argument and file-I/O boundary; existing clients keep their path mode.
 */
void Conf_use_utf8_paths(void);
#endif
char *Conf_datadir(void);
char *Conf_defaults_file_name(void);
char *Conf_password_file_name(void);
char *Conf_player_passwords_file_name(void);
char *Conf_mapdir(void);
char *Conf_fontdir(void);
char *Conf_default_map(void);
char *Conf_servermotdfile(void);
char *Conf_localmotdfile(void);
char *Conf_logfile(void);
char *Conf_ship_file(void);
char *Conf_texturedir(void);
char *Conf_sounddir(void);
char *Conf_soundfile(void);
char *Conf_localguru(void);
char *Conf_robotfile(void);
char *Conf_zcat_ext(void);
char *Conf_zcat_format(void);

#endif /* XPCONFIG_H */
