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

/* Original options parsing code contributed by Ted Lemon <mellon@ncd.com> */

#include "xpserver.h"


/*
 * Print the option list in "-help" format.
 * NT uses this to generate the ServerOpts.txt file
 */
static void Parse_help(char *progname)
{
    int j, flags, all_flags, option_count;
    const char *str;
    option_desc *option_descs;
    char msg[MSG_LEN];

    option_descs = Get_option_descs(&option_count);

    printf("Usage: %s [ options ]\n"
	   "\n",
	   progname);
    printf("Option flags specify where an option can be used and whether it\n"
	   "is visible or not.  Default flags are \"any\" if not otherwise\n"
	   "specified by one or more of these keywords:\n"
	   "\n");
    printf("command       May be specified on the command-line.\n"
	   "passwordfile  May be specified in the password file.\n"
	   "defaults      May be specified in the defaults file.\n"
	   "any           May be specified in the map file, defaults file,\n"
	   "              or on the command line.\n"
	   "invisible     Is not displayed when a list of options is\n"
	   "              requested from the server by an operator.\n"
	   "\n");
    printf("The possible options include:\n"
	   "\n");
    printf("    -tcp\n"
	   "\tSelect TCP for both contact/lobby and gameplay.\n"
	   "\n"
	   "    -udp\n"
	   "\tSelect UDP for both contact/lobby and gameplay.  This is the "
	       "default.\n"
	   "\n"
	   "    -websocket\n"
	   "\tSelect WebSocket for both contact/lobby and gameplay.\n"
	   "\n"
	   "    -transport <udp|tcp|websocket>\n"
	   "\tSelect one transport for both contact/lobby and gameplay.\n"
	   "\tTransport options are applied from left to right.\n"
	   "\n");
    for (j = 0; j < option_count; j++) {
	printf("    %s%s",
	       option_descs[j].type == valBool ? "-/+" : "-",
	       option_descs[j].name);
	if (strcasecmp(option_descs[j].commandLineOption,
		       option_descs[j].name))
	    xpprintf(" or %s", option_descs[j].commandLineOption);
	printf(" %s\n",
	       option_descs[j].type == valInt ? "<integer>" :
	       option_descs[j].type == valReal ? "<real>" :
	       option_descs[j].type == valString ? "<string>" :
	       option_descs[j].type == valIPos ? "<position>" :
	       option_descs[j].type == valList ? "<list>" :
	       "");
	for (str = option_descs[j].helpLine; *str; str++) {
	    if (str == option_descs[j].helpLine || str[-1] == '\n')
		putchar('\t');
	    putchar(*str);
	}
	if (str > option_descs[j].helpLine && str[-1] != '\n')
	    putchar('\n');
	flags = option_descs[j].flags;
	all_flags = (OPT_ORIGIN_ANY | OPT_VISIBLE);
	if ((flags & all_flags) != all_flags && flags != 0) {
	    strlcpy(msg, "[ Flags: command, ", sizeof(msg));
	    if ((flags & OPT_PASSWORD) != 0)
		strlcat(msg, "passwordfile, ", sizeof(msg));
	    if ((flags & (OPT_DEFAULTS | OPT_MAP)) == OPT_DEFAULTS)
		strlcat(msg, "defaults, ", sizeof(msg));
	    if ((flags & OPT_MAP) != 0)
		strlcat(msg, "any, ", sizeof(msg));
	    if ((flags & OPT_VISIBLE) == 0)
		strlcat(msg, "invisible, ", sizeof(msg));
	    msg[strlen(msg) - 2] = '\0';
	    strlcat(msg, " ]", sizeof(msg));
	    printf("\t%s\n", msg);
	}
	putchar('\n');
    }
    printf(
"    \n"
"    The probabilities are in the range [0.0-1.0] and they refer to the\n"
"    probability that an event will occur in a block per second.\n"
"    Boolean options are turned off by using +<option>.\n"
"    \n"
"    Please refer to the manual pages, xpilot-infinity-server(6),\n"
"    xpilot-infinity-x11(6) and xpilot-infinity-sdl(6) for more specific help.\n"
	  );
}


/*
 * Print the option list.
 */
static void Parser_dump_options(char *progname)
{
    int j, option_count;
    option_desc *option_descs;

    UNUSED_PARAM(progname);
    option_descs = Get_option_descs(&option_count);

    for (j = 0; j < option_count; j++) {
	if (option_descs[j].type != valVoid) {
	    int len = strlen(option_descs[j].name);

	    xpprintf("%s:%*s%s\n", option_descs[j].name,
		   (len < 40) ? (40 - len) : 1, "",
		   (option_descs[j].defaultValue != NULL)
		       ? option_descs[j].defaultValue
		       : "");
	}
    }
    xpprintf("\n");
}


/*
 * Print the option flags.
 */
static void Parser_dump_flags(char *progname)
{
    int j, option_count;
    option_desc *option_descs;
    char msg[MSG_LEN];

    UNUSED_PARAM(progname);
    option_descs = Get_option_descs(&option_count);

    for (j = 0; j < option_count; j++) {
	int len = strlen(option_descs[j].name);

	strlcpy(msg, "{", sizeof(msg));
	if ((option_descs[j].flags & OPT_COMMAND) != 0)
	    strlcat(msg, "command, ", sizeof(msg));
	if ((option_descs[j].flags & OPT_PASSWORD) != 0)
	    strlcat(msg, "passwordfile, ", sizeof(msg));
	if ((option_descs[j].flags & OPT_DEFAULTS) != 0)
	    strlcat(msg, "defaults, ", sizeof(msg));
	if ((option_descs[j].flags & OPT_MAP) != 0)
	    strlcat(msg, "map, ", sizeof(msg));
	if ((option_descs[j].flags & OPT_VISIBLE) != 0)
	    strlcat(msg, "visible, ", sizeof(msg));
	if ((strlen(msg) >= 2))
	    msg[strlen(msg) - 2] = '\0';
	strlcat(msg, "}", sizeof(msg));
	xpprintf("%s:%*s%s\n", option_descs[j].name,
		 (len < 40) ? (40 - len) : 1, "", msg);
    }
    xpprintf("\n");
}


/*
 * Print some compile time configuration parameters.
 */
static void Parser_dump_config(char *progname)
{
    option_desc *option_descs;
    int option_count;

    option_descs = Get_option_descs(&option_count);

    xpprintf("\n");
    xpprintf("# %s option dump\n", progname);
    xpprintf("# \n");
    xpprintf("# CONF_DATADIR = %s\n", Conf_datadir());
    xpprintf("# CONF_DEFAULTS_FILE_NAME = %s\n", Conf_defaults_file_name());
    xpprintf("# CONF_PASSWORD_FILE_NAME = %s\n", Conf_password_file_name());
    xpprintf("# CONF_MAPDIR = %s\n", Conf_mapdir());
    xpprintf("# CONF_DEFAULT_MAP = %s\n", Conf_default_map());
    xpprintf("# CONF_SERVERMOTDFILE = %s\n", Conf_servermotdfile());
    xpprintf("# CONF_ROBOTFILE = %s\n", Conf_robotfile());
    xpprintf("# \n");
}


/*
 * Print the option list and
 * some compile time configuration parameters.
 */
static void Parser_dump_all(char *progname)
{
    Parser_dump_config(progname);
    Parser_dump_options(progname);
}


/*
 * Get the current value of a server option
 * in a text buffer.
 *
 * This is called when a client requests
 * to see the current server parameter list.
 */
int Parser_list_option(int *ind, char *buf)
{
    int i = *ind, option_count;
    option_desc *opts;

    opts = Get_option_descs(&option_count);

    if (i < 0 || i >= option_count)
	return -1;

    if (opts[i].defaultValue == NULL)
	return 0;

    if ((opts[i].flags & OPT_VISIBLE) == 0)
	return 0;

    switch (opts[i].type) {
    case valInt:
	sprintf(buf, "%s:%d", opts[i].name, *(int *)opts[i].variable);
	break;
    case valReal:
	sprintf(buf, "%s:%g", opts[i].name, *(double *)opts[i].variable);
	break;
    case valBool:
	sprintf(buf, "%s:%s", opts[i].name,
		*(bool *)opts[i].variable ? "yes" : "no");
	break;
    case valIPos:
	sprintf(buf, "%s:%d,%d", opts[i].name,
		((ipos_t *)opts[i].variable)->x,
		((ipos_t *)opts[i].variable)->y);
	break;
    case valString:
	sprintf(buf, "%s:%s", opts[i].name, *(char **)opts[i].variable);
	break;
    case valList:
	{
	    list_t list = *(list_t *)opts[i].variable;

	    sprintf(buf, "%s:", opts[i].name);
	    if (list) {
		list_iter_t iter;

		for (iter = List_begin(list);
		     iter != List_end(list);
		     LI_FORWARD(iter)) {
		    char *str = (char *)LI_DATA(iter);

		    if (iter != List_begin(list))
			strlcat(buf, ",", MSG_LEN);
		    if (strlcat(buf, str, MSG_LEN) >= MSG_LEN)
			break;
		}
	    }
	}
	break;
    default:
	return 0;
    }
    return 1;
}


/*
 * Check if the i-th command line argument
 * is a request for help or info.
 */
static bool Parse_check_info_request(char **argv, int i)
{
    char *arg = argv[i];

    if (arg[0] == '-' && arg[1] == '-')
	/* when arg starts with two dashes skip first one */
	arg++;

    if (strcmp(arg, "-help") == 0
	|| strcmp(arg, "-h") == 0) {
	Parse_help(*argv);
	return true;
    }
    if (strcmp(arg, "-dump") == 0) {
	Parser_dump_all(*argv);
	return true;
    }
    if (strcmp(arg, "-dumpMan") == 0) {
	Parser_dump_options(*argv);
	return true;
    }
    if (strcmp(arg, "-dumpWindows") == 0) {
	Parser_dump_options(*argv);
	return true;
    }
    if (strcmp(arg, "-dumpFlags") == 0) {
	Parser_dump_flags(*argv);
	return true;
    }
    if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0) {
	puts(TITLE);
	return true;
    }

    return false;
}


/*
 * Parse command-line-only shortcuts which update both transport options.
 * Applying them while walking argv preserves ordering with the individual
 * contactTransport and gameTransport options.
 */
static bool Parse_transport_option(int argc, char **argv, int *index)
{
    const char *arg = argv[*index];
    const char *transport;

    if (arg[0] != '-')
	return false;
    if (!strcasecmp(arg + 1, "tcp"))
	transport = "tcp";
    else if (!strcasecmp(arg + 1, "udp"))
	transport = "udp";
    else if (!strcasecmp(arg + 1, "websocket"))
	transport = "websocket";
    else if (!strcasecmp(arg + 1, "transport")) {
	if (*index + 1 == argc) {
	    warn("Option '%s' needs an argument", arg);
	    return true;
	}
	(*index)++;
	transport = argv[*index];
    }
    else
	return false;

    Option_set_value("contactTransport", transport, 1, OPT_COMMAND);
    Option_set_value("gameTransport", transport, 1, OPT_COMMAND);
    return true;
}


/*
 * Parse all command line arguments
 * and read the server defaults file and map file.
 * Then convert the map data into a World structure.
 */
bool Parser(int argc, char **argv)
{
    int i;
    char *fname;
    option_desc *desc;

    options.mapData = NULL;
    options.mapWidth = 0;
    options.mapHeight = 0;

    if (Init_options() == false)
	return false;

    for (i = 1; i < argc; i++) {
	if (Parse_check_info_request(argv, i))
	    return false;
	if (Parse_transport_option(argc, argv, &i))
	    continue;

	if (argv[i][0] == '-' || argv[i][0] == '+') {
	    desc = Find_option_by_name(argv[i] + 1);
	    if (desc != NULL) {
		if (desc->type == valBool) {
		    const char *bool_value;

		    if (argv[i][0] == '-')
			bool_value = "true";
		    else
			bool_value = "false";
		    Option_set_value(desc->name, bool_value, 1, OPT_COMMAND);
		}
		else if (desc->type == valVoid)
		    ;
		else {
		    if (i + 1 == argc)
			warn("Option '%s' needs an argument", argv[i]);
		    else {
			i++;
			Option_set_value(desc->name, argv[i], 1, OPT_COMMAND);
		    }
		}
	    }
	    else
		warn("Unknown option '%s'", argv[i]);
	}
	else
	    warn("Unknown option '%s'", argv[i]);
    }

    /*
     * Read local defaults file
     */
    if ((fname = Option_get_value("defaultsFileName", NULL)) != NULL)
	parseDefaultsFile(fname);
    else
	parseDefaultsFile(Conf_defaults_file_name());

    /*
     * Read local password file
     */
    if ((fname = Option_get_value("passwordFileName", NULL)) != NULL)
	parsePasswordFile(fname);
    else
	parsePasswordFile(Conf_password_file_name());

    /*
     * Read map file if map data not found yet.
     *
     * If "mapFileName" is defined then read it's contents from file.
     * Else read a default map.
     */
    if (!(fname = Option_get_value("mapData", NULL))) {
	const char *strict_value = Option_get_value("strictMap", NULL);
	bool strict_map = strict_value && ON(strict_value);
	if ((fname = Option_get_value("mapFileName", NULL)) != NULL) {
	    if (!parseMapFile(fname)) {
		if (strict_map) {
		    xpprintf("Unable to read required map %s\n", fname);
		    Options_free();
		    return false;
		}
		xpprintf("Unable to read %s, trying to open %s\n",
			 fname, Conf_default_map());
		if (!parseMapFile(Conf_default_map()))
		    xpprintf("Unable to read %s\n", Conf_default_map());
	    }
	} else {
	    if (strict_map) {
		xpprintf("A map must be specified when strictMap is enabled\n");
		Options_free();
		return false;
	    }
	    xpprintf("Map not specified, trying to open %s\n",
		     Conf_default_map());
	    if (!parseMapFile(Conf_default_map()))
		xpprintf("Unable to read %s\n", Conf_default_map());
	}
    }
    /*
     * Parse the options database and 'internalise' it.
     */
    Options_parse();

    Options_free();

    /*
     * Construct the World structure from the options.
     */
    return Grok_map();
}


/*
 * Modify an option during the game.
 *
 * Options which can be modified have a so called tuner function,
 * which checks the validity of the new option value, and possibly
 * does something extra depending upon the option in question.
 * Options which don't need such a tuner function set it to 'tuner_dummy'.
 * Options which cannot be modified have the tuner set to 'tuner_none'.
 */
int Tune_option(char *name, char *val)
{
    int ival;
    double fval;
    option_desc *opt;

    if (!(opt = Find_option_by_name(name)))
	return -2;	/* Variable not found */

    if (opt->tuner == tuner_none)
	return -1;	/* Operation undefined */

    switch (opt->type) {
    case valInt:
	if (Convert_string_to_int(val, &ival) != true)
	    return 0;
	*(int *)opt->variable = ival;
	(*opt->tuner)();
	return 1;
    case valBool:
	if (ON(val))
	    *(bool *)opt->variable = true;
	else if (OFF(val))
	    *(bool *)opt->variable = false;
	else
	    return 0;
	(*opt->tuner)();
	return 1;
    case valReal:
	if (Convert_string_to_float(val, &fval) != true)
	    return 0;
	*(double *)opt->variable = fval;
	(*opt->tuner)();
	return 1;
    case valString:
	{
	    char *s = xp_strdup(val);

	    if (!s)
		return 0;
	    if (*(char **)(opt->variable) != opt->defaultValue)
		free(*(char **)opt->variable);
	    *(char **)opt->variable = s;
	    (*opt->tuner)();
	    return 1;
	}
    default:
	return -1;	/* Operation undefined */
    }
}


int Get_option_value(const char *name, char *value, size_t size)
{
    option_desc *opt;

    if (size < 12)
	return -1;	/* Generic error. */

    if (!(opt = Find_option_by_name(name)))
	return -2;	/* Variable not found */

    if ((opt->flags & OPT_VISIBLE) == 0)
	return -3;

    switch (opt->type) {
    case valInt:
	sprintf(value, "%d", *((int *)opt->variable));
	break;
    case valReal:
	sprintf(value, "%g", *((double *)opt->variable));
	break;
    case valBool:
	sprintf(value, "%s", *((bool *)opt->variable) ? "true" : "false");
	break;
    case valString:
	if (*((char **)opt->variable) == NULL)
	    return -4;
	strlcpy(value, *((char **)opt->variable), size);
	break;
    case valIPos:
	sprintf(value, "%d, %d",
		((ipos_t *)opt->variable)->x,
		((ipos_t *)opt->variable)->y);
    default:
	return -1;	/* Generic error. */
    }

    return 1;	/* Success. */
}
