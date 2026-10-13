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

#include "xpclient.h"

char *talk_fast_msgs[TALK_FAST_NR_OF_MSGS];	/* talk macros */

/*
 * Abandon your hope, all you enter here
 */

/* The final string, sent to the server */
static char final_str[MAX_CHARS];

#define MSG_PARSED_FIELD_LEN      20

/*
 * Returns a pointer to the first character after the fields
 */
static char *Talk_macro_fields_info(char *buf, int *n_fields)
{
    int end_found = 0, level = 0;

    *n_fields = 0;
    while (!end_found) {
	switch (*buf) {
	case TALK_FAST_START_DELIMITER:
	    if (level++ == 0)
		(*n_fields)++;
	    break;
	case TALK_FAST_MIDDLE_DELIMITER:
	    if (level == 1)
		(*n_fields)++;
	    break;
	case TALK_FAST_END_DELIMITER:
	    level--;
	    if (level == 0)
		end_found = 1;
	    else if (level < 0)
		return NULL;
	    break;
	case '\0':
	    return NULL;
	    break;
	default:
	    break;
	}
	buf++;
    }
    return buf;
}


/* Returns a string pointer to the wanted_field
 * This pointer must be freed after using it
 */
static char *Talk_macro_get_field(char *buf, int wanted_field)
{
    int finished = 0, level = 0, field = 0;
    size_t len;
    char *field_ptr, *start_ptr = NULL, *end_ptr = NULL;

    while (!finished) {
	switch (*buf) {
	case TALK_FAST_START_DELIMITER:
	    if (level == 0) {
		field++;
		if (field == wanted_field)
		    start_ptr = buf + 1;
	    }
	    level++;
	    break;
	case TALK_FAST_MIDDLE_DELIMITER:
	    if (level == 1) {
		field++;
		if (field == wanted_field)
		    start_ptr = buf + 1;
		else if (field == wanted_field + 1) {
		    end_ptr = buf;
		    finished = 1;
		}
	    }
	    break;
	case TALK_FAST_END_DELIMITER:
	    level--;
	    if (level == 0) {
		if (field == wanted_field)
		    end_ptr = buf;
		finished = 1;
	    } else if (level < 0)
		return NULL;
	    break;
	case '\0':
	    return NULL;
	    break;
	default:
	    break;
	}
	buf++;

    }
    len = (size_t) (end_ptr - start_ptr);
    if ((field_ptr = XMALLOC(char, len + 1)) == NULL) {
	error("Can't allocate memory for talk macro");
	return NULL;
    }
    strlcpy(field_ptr, start_ptr, len + 1);
    return field_ptr;
}


static int Talk_macro_parse_mesg(char *outbuf, char *inbuf, long pos,
				 long max)
{
    FILE *fp;
    char c;
    long fsize;
    int i;
    int done = 0;
    int n_fields;
    char *tmpptr;
    char *tmpptr1;
    char *tmpptr2;
    char *tmpptr3 = 0;
    char *nextpos;
    char *filename;
    other_t *player = NULL;

    while (!done && (c = *inbuf++) != '\0') {
	if (pos >= max - 2) {
	    if (outbuf == final_str) {	/* parsing to the talk buffer */
		outbuf[pos] = '\0';
		if (Net_talk(outbuf) == -1)
		    return -1;
		pos = 0;
	    } else
		break;
	}

	if (player != NULL) {
	    switch (c) {
	    case 'l':
		if (BIT(Setup->mode, LIMITED_LIVES))
		    outbuf[pos++] = player->life + '0';
		break;
	    case 'n':
		tmpptr = player->nick_name;
		for (i = 0; tmpptr[i] != '\0' && pos < max - 2; ++i)
		    outbuf[pos++] = tmpptr[i];
		break;
	    case 's':
		if (pos < max - 1 - 6)	/* short - "-16535" max no of chars */
		    pos += sprintf(outbuf + pos, "%.2f", player->score);
		break;
	    case 't':
		if (BIT(Setup->mode, TEAM_PLAY))
		    outbuf[pos++] = player->team + '0';
		break;
	    default:
		break;
	    }
	    player = NULL;
	} else {
	    switch (c) {
	    case TALK_FAST_SPECIAL_TALK_CHAR:
		if ((c = *inbuf++) == '\0') {
		    done = 1;
		    break;
		}
		switch (c) {
		case '=':	/* String comparison */
		    nextpos = Talk_macro_fields_info(inbuf, &n_fields);
		    if (n_fields < 3 || n_fields > 4 || nextpos == NULL)
			break;
		    /* parse field 1 */
		    if ((tmpptr = Talk_macro_get_field(inbuf, 1)) == NULL) {
			error("Talk_macro_get_field (1) error!");
			break;
		    }
		    if ((tmpptr1
			 = XMALLOC(char, MSG_PARSED_FIELD_LEN)) == NULL) {
			error("Can't allocate memory for talk macro.");
			free(tmpptr);	/* successful malloc from before */
			break;
		    }
		    Talk_macro_parse_mesg(tmpptr1, tmpptr, 0,
					  MSG_PARSED_FIELD_LEN);
		    free(tmpptr);
		    /* parse field 2 */
		    if ((tmpptr = Talk_macro_get_field(inbuf, 2)) == NULL) {
			error("Talk_macro_get_field (2) error!");
			break;
		    }
		    if ((tmpptr2
			 = XMALLOC(char, MSG_PARSED_FIELD_LEN)) == NULL) {
			error("Can't allocate memory for talk macro.");
			free(tmpptr);	/* successful malloc from before */
			break;
		    }
		    Talk_macro_parse_mesg(tmpptr2, tmpptr, 0,
					  MSG_PARSED_FIELD_LEN);
		    free(tmpptr);
		    if (!strcmp(tmpptr1, tmpptr2)) {
			/* True */
			if ((tmpptr3 = Talk_macro_get_field(inbuf, 3))
			    == NULL) {
			    error("Talk_macro_get_field (3) error!");
			    free(tmpptr1);
			    free(tmpptr2);
			    break;
			}
			pos = Talk_macro_parse_mesg(outbuf, tmpptr3,
						    pos, max);
		    } else if (n_fields == 4) {
			/* False */
			if ((tmpptr3 = Talk_macro_get_field(inbuf, 4))
			    == NULL) {
			    error("Talk_macro_get_field (4) error!");
			    free(tmpptr1);
			    free(tmpptr2);
			    break;
			}
			pos = Talk_macro_parse_mesg(outbuf, tmpptr3, pos, max);
		    }
		    inbuf = nextpos;
		    free(tmpptr1);
		    free(tmpptr2);
		    free(tmpptr3);
		    break;
		case 'f':
		    nextpos = Talk_macro_fields_info(inbuf, &n_fields);
		    if (n_fields != 1 || nextpos == NULL)
			break;
		    if ((tmpptr = Talk_macro_get_field(inbuf, 1)) == NULL) {
			error("Talk_macro_get_field error!");
			break;
		    }
		    inbuf = nextpos;
		    if ((filename
			 = XMALLOC(char, TALK_FAST_MSG_FNLEN)) == NULL) {
			error("Can't allocate memory for talk macro.");
			free(tmpptr);
			break;
		    }

		    Talk_macro_parse_mesg(filename, tmpptr, 0,
					  TALK_FAST_MSG_FNLEN);
		    free(tmpptr);
		    /* Keep byte counts stable across Windows newline translation. */
		    if ((fp = fopen(filename, "rb")) == NULL) {
			error("Couldn't open file %s", filename);
			free(filename);
			break;
		    }
		    free(filename);

		    /* Get filesize */
		    if (fseek(fp, 0L, SEEK_END) != 0 ||
			(fsize = ftell(fp)) < 0 || (uintmax_t)fsize >= SIZE_MAX ||
			fseek(fp, 0L, SEEK_SET) != 0) {
			error("Couldn't determine talk macro file size");
			fclose(fp);
			break;
		    }

		    if ((tmpptr = XMALLOC(char, (size_t)fsize + 1)) == NULL) {
			fclose(fp);
			break;
		    }
		    if (fread(tmpptr, 1, fsize, fp) != (size_t)fsize || ferror(fp)) {
			error("Couldn't read complete talk macro file");
			free(tmpptr);
			fclose(fp);
			break;
		    }
		    tmpptr[fsize] = '\0';
		    fclose(fp);
		    pos = Talk_macro_parse_mesg(outbuf, tmpptr, pos, max);
		    free(tmpptr);
		    break;
		case 'h':
		    tmpptr = getenv("HOME");
		    if (tmpptr != NULL) {
			while (*tmpptr != '\0' && pos < max - 2)
			    outbuf[pos++] = *tmpptr++;
		    }
		    break;
		case 'r':
		    nextpos = Talk_macro_fields_info(inbuf, &n_fields);
		    if (n_fields <= 0 || nextpos == NULL)
			break;
		    if ((tmpptr = Talk_macro_get_field
			 (inbuf, (int)(randomMT() % n_fields + 1)))
			== NULL) {
			error("Talk_macro_get_field error (random)");
			break;
		    }
		    inbuf = nextpos;
		    pos = Talk_macro_parse_mesg(outbuf, tmpptr, pos, max);
		    free(tmpptr);
		    break;
		case 'n':
		    outbuf[pos] = '\0';
		    if (Net_talk(outbuf) == -1)
			return -1;
		    pos = 0;
		    break;
		case 'l':
		    if (!snooping) {
			if ((player = Other_by_id(lock_id)) == NULL) {
			    pos = 0;
			    done = 1;
			    break;
			}
		    } else {
			if ((player = Other_by_id(eyesId)) == NULL) {
			    pos = 0;
			    done = 1;
			    break;
			}
		    }
		    break;
		case 's':
		    player = self;
		    break;
		case 't':
		    if (BIT(Setup->mode, TEAM_PLAY))
			outbuf[pos++] = self->team + '0';
		    break;
		case TALK_FAST_SPECIAL_TALK_CHAR:
		    outbuf[pos++] = c;
		    break;
		default:
		    break;
		}	/* case TALK_FAST_SPECIAL_TALK_CHAR: switch (c) */
		break;
	    case '\r':
	    case '\n':
		break;
	    default:
		outbuf[pos++] = c;
		break;
	    }		/* (player != NULL) switch(c) */
	}
    }

    outbuf[pos] = '\0';

    return pos;
}


int Talk_macro(int i)
{
    char *str;

    assert(i >= 0);
    assert(i < TALK_FAST_NR_OF_MSGS);
    str = talk_fast_msgs[i];
    if (str == NULL)
	return 1;

    if (Talk_macro_parse_mesg(final_str, str, 0L, MAX_CHARS) > 0)
	Net_talk(final_str);

    return 0;
}


static inline int index_by_option(xp_option_t *opt)
{
    return atoi(Option_get_name(opt) + strlen("msg")) - 1;
}

static bool Set_talk_macro(xp_option_t *opt, const char *value)
{
    int i = index_by_option(opt);

    /*printf("Set_talk_macro: i = %d\n", i);*/
    assert(i >= 0);
    assert(i < TALK_FAST_NR_OF_MSGS);

    XFREE(talk_fast_msgs[i]);
    talk_fast_msgs[i] = xp_safe_strdup(value);

    return true;
}

static const char *Get_talk_macro(xp_option_t *opt)
{
    int i = index_by_option(opt);

    /*printf("Get_talk_macro: i = %d\n", i);*/
    assert(i >= 0);
    assert(i < TALK_FAST_NR_OF_MSGS);

    return talk_fast_msgs[i];
}



xp_option_t talk_macro_options[] = {
    /*
     * Default warning macros for newbies.
     */
    XP_STRING_OPTION(
	"msg1",
	"#t:***    BALL! Our ball is gone! Save it!   ***",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 1.\n"),

    XP_STRING_OPTION(
	"msg2",
	"#t:*** SAFE! Our ball is safe. ***",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 2.\n"),

    XP_STRING_OPTION(
	"msg3",
	"#t:*** COVER! The enemy ball is approaching our base. ***",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 3.\n"),

    XP_STRING_OPTION(
	"msg4",
	"#t:*** POP! The enemy ball is back at the enemy base. ***",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 4.\n"),

    /*
     * This macro swaps between teams 2 and 4, useful on Blood's Music
     * maps.
     */
    XP_STRING_OPTION(
	"msg5",
	"#=[#t|2|/team 4|/team 2]",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 5.\n"),

    XP_STRING_OPTION(
	"msg6",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 6.\n"),

    XP_STRING_OPTION(
	"msg7",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 7.\n"),

    XP_STRING_OPTION(
	"msg8",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 8.\n"),

    XP_STRING_OPTION(
	"msg9",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 9.\n"),

    XP_STRING_OPTION(
	"msg10",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 10.\n"),

    XP_STRING_OPTION(
	"msg11",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 11.\n"),

    XP_STRING_OPTION(
	"msg12",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 12.\n"),

    XP_STRING_OPTION(
	"msg13",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 13.\n"),

    XP_STRING_OPTION(
	"msg14",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 14.\n"),

    XP_STRING_OPTION(
	"msg15",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 15.\n"),

    XP_STRING_OPTION(
	"msg16",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 16.\n"),

    XP_STRING_OPTION(
	"msg17",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 17.\n"),

    XP_STRING_OPTION(
	"msg18",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 18.\n"),

    XP_STRING_OPTION(
	"msg19",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 19.\n"),

    XP_STRING_OPTION(
	"msg20",
	"",
	NULL, 0,
	Set_talk_macro, NULL, Get_talk_macro,
	XP_OPTFLAG_DEFAULT,
	"Talkmessage 20.\n"),
};

void Store_talk_macro_options(void)
{
    STORE_OPTIONS(talk_macro_options);
}
