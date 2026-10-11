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
 * Copyright (C) 2003-2004 Kristian Söderblom <kps@users.sourceforge.net>
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

const char c_commands[][16] = {
    "ignore", "i",
    "ignore!", "i!",
    "unignore", "u",
    "help", "h",
    "set", "s",
    "get", "g",
    "quit", "q",
};

static void print_ignorelist(void)
{
    char buffer[MSG_LEN] = "";
    int i;
    unsigned short check = 0;

    for (i = 0; i < num_others; i++) {
	if (Others[i].ignorelevel == 1) {
	    if (strlen(buffer) + strlen(Others[i].nick_name) + 17
		> sizeof(buffer)) {
		strcat(buffer, "[*Client reply*]");
		Add_message(buffer);
		buffer[0] = '\0';
	    }

	    strcat(buffer, Others[i].nick_name);
	    strcat(buffer, " ");
	    check = 1;
	} else if (Others[i].ignorelevel == 2) {
	    if (strlen(buffer) + strlen(Others[i].nick_name) + 18 > sizeof(buffer)) {
		strcat(buffer, "[*Client reply*]");
		Add_message(buffer);
		buffer[0] = '\0';
	    }

	    strcat(buffer, "!");
	    strcat(buffer, Others[i].nick_name);
	    strcat(buffer, " ");

	    check = 1;
	}
    }

    if (check) {
	strcat(buffer, "[*Client reply*]");
	Add_message(buffer);
    } else
	Add_message("Ignorelist is empty. [*Client reply*]");
}

static void print_help(const char *arg)
{
    int i;
    char message[MAX_CHARS] = "";

    /*Add_message(arg);*/

    if (arg == NULL) {
	for (i = 0; i < NELEM(c_commands); i += 2) {
	    strcat(message, c_commands[i]);
	    strcat(message, " ");
	}
	strcat(message, "[*Client reply*]");

	Add_message(message);
    } else {
	for (i = 0; i < NELEM(c_commands); i++) {
	    if (!strcmp(arg, c_commands[i]))
		break;
	}

	switch (i) {
	case 0:		/* ignore */
	case 1:		/* i */
	    Add_message("'\\ignore <player>' ignores <player> by changing "
			"text to ***'s. [*Client reply*]");
	    Add_message("Just '\\ignore' shows list of ignored players "
			"[*Client reply*]");
	    break;
	case 2:		/* ignore! */
	case 3:		/* i! */
	    Add_message("'\\ignore! <player>' ignores <player> completely. "
			"[*Client reply*]");
	    break;
	case 4:		/* unignore */
	case 5:		/* u */
	    Add_message("'\\unignore <player>' allows messages from <player> "
			"again. [*Client reply*]");
	    break;
	case 6:		/* help */
	case 7:		/* h */
	    Add_message("'\\help <command>' shows help about <command>. "
			"Just '\\help' show avaiable commands "
			"[*Client reply*]");
	    break;
	case 8:		/* set */
	case 9:		/* s */
	    Add_message("'\\set <option> <value>' sets an option value. "
			"[*Client reply*]");
	    break;
	case 10:	/* get */
	case 11:	/* g */
	    Add_message("'\\get <option>' gets an option value. "
			"[*Client reply*]");
	    break;
	case 12:	/* quit */
	case 13:	/* q */
	    Add_message("'\\quit' quits the game, no questions asked. "
			"[*Client reply*]");
	    break;
	default:
	    Add_message("No such command [*Client reply*]");
	    break;
	}
    }
}

static void ignorePlayer(const char *name, int level)
{
    other_t *other = Other_by_name(name, true);
    char buf[64 + MAX_CHARS];

    if (other != NULL) {
	if (level == 1) {
	    snprintf(buf, sizeof(buf),
		     "Ignoring %s (textmask). [*Client reply*]",
		     other->nick_name);
	    Add_message(buf);
	} else {
	    snprintf(buf, sizeof(buf),
		     "Ignoring %s (completely). [*Client reply*]",
		     other->nick_name);
	    Add_message(buf);
	}
	other->ignorelevel = level;
    }
}

static void unignorePlayer(const char *name)
{
    other_t *other = Other_by_name(name, true);
    char buf[64 + MAX_CHARS];

    if (other != NULL) {
	snprintf(buf, sizeof(buf),
		 "Stopped ignoring %s. [*Client reply*]", other->nick_name);
	Add_message(buf);
	other->ignorelevel = 0;
    }
}

void executeCommand(const char *talk_str)
{
    int i, command_num;
    char str[MAX_CHARS];
    char *command, *argument = NULL;

    assert(talk_str);
    if (strlen(talk_str) == 0) {
	Add_message("No clientcommand specified, try the \\help command. "
		    "[*Client reply*]");
	return;
    }

    strlcpy(str, talk_str, MAX_CHARS);
    command = strtok(str, " ");

    for (i = 0; i < NELEM(c_commands); i++) {
	if (!strcmp(command, c_commands[i]))
	    break;
    }

    if (i == NELEM(c_commands)) {
	Add_message("Invalid clientcommand. [*Client reply*]");
	return;
    }

    /* argument can contains spaces, that's why we have "" and not " " */
    argument = strtok(NULL, "");

    command_num = i;
    switch (command_num) {
    case 0:			/* ignore */
    case 1:			/* i */
    case 2:			/* ignore! */
    case 3:			/* i! */
	if (!argument)	/* empty */
	    print_ignorelist();
	else
	    ignorePlayer(argument, (int)(command_num / 2 + 1) /*stupid hack*/);
	break;
    case 4:			/* unignore */
    case 5:			/* u */
	if (!argument)
	    print_help(command);
	else
	    unignorePlayer(argument);
	break;
    case 6:			/* help */
    case 7:			/* h */
	print_help(argument);
	break;
    case 8:			/* set */
    case 9:			/* s */
	if (!argument)
	    print_help(command);
	else
	    Set_command(argument);
	break;
    case 10:			/* get */
    case 11:			/* g */
	if (!argument)
	    print_help(command);
	else
	    Get_command(argument);
	break;
    case 12:			/* quit */
    case 13:			/* q */
	Client_exit(0);
    default:
	warn("BUG: bad command num %d in executeCommand()", command_num);
	assert(0);
	break;
    }
}

void crippleTalk(char *msg)
{
    int i, msgEnd;

    for (i = strlen(msg) - 1; i > 0; i--) {
	if (msg[i - 1] == ' ' && msg[i] == '[')
	    break;
    }

    if (i == 0)
	return;

    msgEnd = i - 1;

    for (i = 0; i < msgEnd; i++) {
	if (isalpha(msg[i]) || isdigit(msg[i]))
	    msg[i] = '*';
    }
}
