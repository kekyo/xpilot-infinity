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
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, browser_options_saved, (void), { Module.xpilotSettingsChanged?.(); });
#endif

int num_options = 0;
int max_options = 0;

xp_option_t *options = NULL;

xp_option_t *Find_option(const char *name)
{
    int i;

    for (i = 0; i < num_options; i++) {
	if (!strcasecmp(name, options[i].name))
	    return &options[i];
    }

    return NULL;
}

static const char *Option_default_value_to_string(xp_option_t *opt)
{
    static char buf[4096];

    switch (opt->type) {
    case xp_noarg_option:
	strcpy(buf, "");
	break;
    case xp_bool_option:
	sprintf(buf, "%s", opt->bool_defval ? "yes" : "no");
	break;
    case xp_int_option:
	sprintf(buf, "%d", opt->int_defval);
	break;
    case xp_double_option:
	sprintf(buf, "%.3f", opt->dbl_defval);
	break;
    case xp_string_option:
	if (opt->str_defval && strlen(opt->str_defval) > 0)
	    strlcpy(buf, opt->str_defval, sizeof(buf));
	else
	    strcpy(buf, "");
	break;
    case xp_key_option:
	if (opt->key_defval && strlen(opt->key_defval) > 0)
	    strlcpy(buf, opt->key_defval, sizeof(buf));
	else
	    strcpy(buf, "");
	break;
    default:
	assert(0 && "Unknown option type");
    }
    return buf;
}


static void Print_default_value(xp_option_t *opt)
{
    const char *defval = Option_default_value_to_string(opt);

    switch (opt->type) {
    case xp_noarg_option:
	break;
    case xp_bool_option:
    case xp_int_option:
    case xp_double_option:
    case xp_string_option:
	if (strlen(defval) > 0)
	    printf("        The default value is: %s.\n", defval);
	else
	    printf("        There is no default value for this option.\n");
	break;

    case xp_key_option:
	if (opt->key_defval && strlen(opt->key_defval) > 0)
	    printf("        The default %s: %s.\n",
		   (strchr(opt->key_defval, ' ') == NULL
		    ? "key is" : "keys are"), opt->key_defval);
	else
	    printf("        There is no default value for this option.\n");
	break;
    default:
	assert(0 && "Unknown option type");
    }
}

void Usage(void)
{
    int i;

    printf("Usage: %s [-options ...] [server-target ...]\n"
	   "Server targets: HOST, ws://HOST[:PORT], tcp://HOST[:PORT], "
	   "or udp://HOST[:PORT]\n"
	   "Where options include:\n" "\n", Program_name());
    for (i = 0; i < num_options; i++) {
	xp_option_t *opt = Option_by_index(i);

	printf("    -%s %s\n", opt->name,
	       (opt->type != xp_noarg_option) ? "<value>" : "");
	if (opt->help && opt->help[0]) {
	    const char *str;
	    printf("        ");
	    for (str = opt->help; *str; str++) {
		putchar(*str);
		if (*str == '\n' && str[1])
		    printf("        ");
	    }
	    if (str[-1] != '\n')
		putchar('\n');
	}
	Print_default_value(opt);
	printf("\n");
    }
    printf("Most of these options can also be set in the .xpilotrc file\n"
	   "in your home directory.\n"
	   "Each key option may have multiple keys bound to it and\n"
	   "one key may be used by multiple key options.\n"
	   "If no server is specified on the command line, xpilot will\n"
	   "display a welcome screen where you can select a server.\n");

    exit(1);
}

static void Version(void)
{
    puts(TITLE);
    exit(0);
}

bool Set_noarg_option(xp_option_t *opt, bool value, xp_option_origin_t origin)
{
    assert(opt);
    assert(opt->type == xp_noarg_option);
    assert(opt->noarg_ptr);

    *opt->noarg_ptr = value;
    opt->origin = origin;

    return true;
}


bool Set_bool_option(xp_option_t *opt, bool value, xp_option_origin_t origin)
{
    bool retval = true;

    assert(opt);
    assert(opt->type == xp_bool_option);
    assert(opt->bool_ptr);

    if (opt->bool_setfunc)
	retval = opt->bool_setfunc(opt, value);
    else
	*opt->bool_ptr = value;

    if (retval)
	opt->origin = origin;

    return retval;
}

bool Set_int_option(xp_option_t *opt, int value, xp_option_origin_t origin)
{
    bool retval = true;

    assert(opt);
    assert(opt->type == xp_int_option);
    assert(opt->int_ptr);

    if (origin == xp_option_origin_setcmd) {
	char msg[MSG_LEN];

	if (value < opt->int_minval) {
	    snprintf(msg, sizeof(msg),
		     "Minimum value for option %s is %d. [*Client reply*]",
		     opt->name, opt->int_minval);
	    Add_message(msg);
	}
	if (value > opt->int_maxval) {
	    snprintf(msg, sizeof(msg),
		     "Maximum value for option %s is %d. [*Client reply*]",
		     opt->name, opt->int_maxval);
	    Add_message(msg);
	}
    }
    else {
	if (!(value >= opt->int_minval && value <= opt->int_maxval)) {
	    warn("Bad value %d for option \"%s\", using default...",
		 value, opt->name);
	    value = opt->int_defval;
	}
    }

    LIMIT(value, opt->int_minval, opt->int_maxval);

    if (opt->int_setfunc)
	retval = opt->int_setfunc(opt, value);
    else
	*opt->int_ptr = value;

    if (retval)
	opt->origin = origin;

    return retval;
}

bool Set_double_option(xp_option_t *opt, double value,
		       xp_option_origin_t origin)
{
    bool retval = true;

    assert(opt);
    assert(opt->type == xp_double_option);
    assert(opt->dbl_ptr);

    if (origin == xp_option_origin_setcmd) {
	char msg[MSG_LEN];

	if (value < opt->dbl_minval) {
	    snprintf(msg, sizeof(msg),
		     "Minimum value for option %s is %.3f. [*Client reply*]",
		     opt->name, opt->dbl_minval);
	    Add_message(msg);
	}
	if (value > opt->dbl_maxval) {
	    snprintf(msg, sizeof(msg),
		     "Maximum value for option %s is %.3f. [*Client reply*]",
		     opt->name, opt->dbl_maxval);
	    Add_message(msg);
	}
    }
    else {
	if (!(value >= opt->dbl_minval && value <= opt->dbl_maxval)) {
	    warn("Bad value %.3f for option \"%s\", using default...",
		 value, opt->name);
	    value = opt->dbl_defval;
	}
    }

    LIMIT(value, opt->dbl_minval, opt->dbl_maxval);

    if (opt->dbl_setfunc)
	retval = opt->dbl_setfunc(opt, value);
    else
	*opt->dbl_ptr = value;

    if (retval)
	opt->origin = origin;

    return retval;
}

bool Set_string_option(xp_option_t *opt, const char *value,
		       xp_option_origin_t origin)
{
    bool retval = true;

    assert(opt);
    assert(opt->type == xp_string_option);
    assert(opt->str_ptr || (opt->str_setfunc && opt->str_getfunc));
    assert(value);		/* allow NULL ? */

    /*
     * The reason string options don't assume a static area is that that
     * would not allow dynamically allocated strings of arbitrary size.
     */
    if (opt->str_setfunc)
	retval = opt->str_setfunc(opt, value);
    else
	strlcpy(opt->str_ptr, value, opt->str_size);

    if (retval)
	opt->origin = origin;

    return retval;
}

xp_keydefs_t *keydefs = NULL;
int num_keydefs = 0;
int max_keydefs = 0;


/*
 * This function is used when platform specific code has an event where
 * the user has pressed or released the key defined by the keysym 'ks'.
 * When the key state has changed, the first call to this function should have
 * 'reset' true, then following calls related to the same event should
 * have 'reset' false. For each returned xpilot key, the calling code
 * should call some handler. The function should be called until it returns
 * KEY_DUMMY.
 */
keys_t Generic_lookup_key(xp_keysym_t ks, bool reset)
{
    keys_t ret = KEY_DUMMY;
    static int i = 0;

    if (reset)
	i = 0;

    /*
     * Variable 'i' is already initialized.
     * Use brute force linear search to find the key.
     */
    for (; i < num_keydefs; i++) {
	if (ks == keydefs[i].keysym) {
	    ret = keydefs[i].key;
	    i++;
	    break;
	}
    }

    return ret;
}

static void Store_keydef(int ks, keys_t key)
{
    int i;
    xp_keydefs_t keydef;

    /*
     * first check if pair (ks, key) already exists 
     */
    for (i = 0; i < num_keydefs; i++) {
	xp_keydefs_t *kd = &keydefs[i];

	if (kd->keysym == ks && kd->key == key) {
	    /*warn("Pair (%d, %d) exist from before", ks, (int) key);*/
	    /*
	     * already exists, no need to store 
	     */
	    return;
	}
    }

    keydef.keysym = ks;
    keydef.key = key;

    /*
     * find first KEY_DUMMY after lazy deletion 
     */
    for (i = 0; i < num_keydefs; i++) {
	xp_keydefs_t *kd = &keydefs[i];

	if (kd->key == KEY_DUMMY) {
	    assert(kd->keysym == XP_KS_UNKNOWN);
	    /*warn("Store_keydef: Found dummy at index %d", i);*/
	    *kd = keydef;
	    return;
	}
    }

    /*
     * no lazily deleted entry, ok, just store it then
     */
    STORE(xp_keydefs_t, keydefs, num_keydefs, max_keydefs, keydef);
}

static void Remove_key_from_keydefs(keys_t key)
{
    int i;

    assert(key != KEY_DUMMY);
    for (i = 0; i < num_keydefs; i++) {
	xp_keydefs_t *kd = &keydefs[i];

	/*
	 * lazy deletion 
	 */
	if (kd->key == key) {
	    /*warn("Remove_key_from_keydefs: Removing key at index %d", i);*/
	    kd->keysym = XP_KS_UNKNOWN;
	    kd->key = KEY_DUMMY;
	}
    }
}

static bool Set_key_option(xp_option_t *opt, const char *value,
			   xp_option_origin_t origin)
{
    /*bool retval = true;*/
    char *str, *valcpy;

    assert(opt);
    assert(opt->name);
    assert(opt->type == xp_key_option);
    assert(opt->key != KEY_DUMMY);
    assert(value);

    /*
     * warn("Setting key option %s to \"%s\"", opt->name, value); 
     */

    /*
     * First remove the old setting.
     */
    XFREE(opt->key_string);
    Remove_key_from_keydefs(opt->key);

    /*
     * Store the new setting.
     */
    opt->key_string = xp_safe_strdup(value);
    valcpy = xp_safe_strdup(value);
    for (str = strtok(valcpy, " \t\r\n");
	 str != NULL;
	 str = strtok(NULL, " \t\r\n")) {
	xp_keysym_t ks;

	/*
	 * You can write "none" for keys in xpilotrc to disable the key.
	 */
	if (!strcasecmp(str, "none"))
	    continue;

	ks = String_to_xp_keysym(str);
	if (ks == XP_KS_UNKNOWN) {
	    warn("Invalid keysym \"%s\" for key \"%s\".\n", str, opt->name);
	    continue;
	}

	Store_keydef(ks, opt->key);
    }

    /* in fact if we only get invalid keysyms we should return false */
    opt->origin = origin;
    XFREE(valcpy);
    return true;
}

static bool is_legal_value(xp_option_type_t type, const char *value)
{
    if (type == xp_noarg_option || type == xp_bool_option) {
	if (ON(value) || OFF(value))
	    return true;
	return false;
    }
    if (type == xp_int_option) {
	int foo;

	if (sscanf(value, "%d", &foo) <= 0)
	    return false;
	return true;
    }
    if (type == xp_double_option) {
	double foo;

	if (sscanf(value, "%lf", &foo) <= 0)
	    return false;
	return true;
    }
    return true;
}


bool Set_option(const char *name, const char *value, xp_option_origin_t origin)
{
    xp_option_t *opt;

    opt = Find_option(name);
    if (!opt)
	/* unknown */
	return false;

    if (!is_legal_value(opt->type, value)) {
	if (origin != xp_option_origin_setcmd)
	    warn("Bad value \"%s\" for option %s.", value, opt->name);
	else {
	    char msg[MSG_LEN];
	
	    snprintf(msg, sizeof(msg),
		     "Bad value \"%s\" for option %s. [*Client reply*]",
		     value, opt->name);
	    Add_message(msg);
	}
	return false;
    }

    switch (opt->type) {
    case xp_noarg_option:
	return Set_noarg_option(opt, ON(value) ? true : false, origin);
    case xp_bool_option:
	return Set_bool_option(opt, ON(value) ? true : false, origin);
    case xp_int_option:
	return Set_int_option(opt, atoi(value), origin);
    case xp_double_option:
	return Set_double_option(opt, atof(value), origin);
    case xp_string_option:
	return Set_string_option(opt, value, origin);
    case xp_key_option:
	return Set_key_option(opt, value, origin);
    default:
	assert(0 && "TODO");
    }
    return false;
}


/*
 * kps - these commands need some fine tuning. 
 * TODO - unset a value, i.e. set it to empty 
 */
/*
 * Handler for \set client command.
 */
void Set_command(const char *args)
{
    char *name, *value, *valcpy;
    xp_option_t *opt;
    char msg[MSG_LEN];

    assert(args);

    valcpy = xp_safe_strdup(args);

    name = strtok(valcpy, " \t\r\n");
    value = strtok(NULL, "");

    opt = Find_option(name);

    if (!opt) {
	snprintf(msg, sizeof(msg),
		 "Unknown option \"%s\". [*Client reply*]", name);
	Add_message(msg);
	goto out;
    }

    if (!value) {
	Add_message("Set command needs an option and a value. "
		    "[*Client reply*]");
	goto out;
    }
    else {
	const char *newvalue;
	const char *nm = Option_get_name(opt);

	Set_option(name, value, xp_option_origin_setcmd);

	newvalue = Option_value_to_string(opt);
	snprintf(msg, sizeof(msg),
		 "The value of %s is now %s. [*Client reply*]",
		 nm, newvalue);
	Add_message(msg);
    }

 out:
    XFREE(valcpy);
}

const char *Option_value_to_string(xp_option_t *opt)
{
    static char buf[MSG_LEN];

    switch (opt->type) {
    case xp_noarg_option:
	sprintf(buf, "%s", *opt->noarg_ptr ? "yes" : "no");
	break;
    case xp_bool_option:
	sprintf(buf, "%s", *opt->bool_ptr ? "yes" : "no");
	break;
    case xp_int_option:
	sprintf(buf, "%d", *opt->int_ptr);
	break;
    case xp_double_option:
	sprintf(buf, "%.3f", *opt->dbl_ptr);
	break;
    case xp_string_option:
	/*
	 * Assertion in Store_option guarantees one of these is not NULL. 
	 */
	if (opt->str_getfunc)
	    return opt->str_getfunc(opt);
	else
	    return opt->str_ptr;
    case xp_key_option:
	assert(opt->key_string);
	return opt->key_string;
    default:
	assert(0 && "Unknown option type");
    }
    return buf;
}


/*
 * Handler for \get client command.
 */
void Get_command(const char *args)
{
    char *name, *valcpy;
    xp_option_t *opt;
    char msg[MSG_LEN];

    assert(args);

    valcpy = xp_safe_strdup(args);

    name = strtok(valcpy, " \t\r\n");
    opt = Find_option(name);

    if (opt) {
	const char *val = Option_value_to_string(opt);
	const char *nm = Option_get_name(opt);

	if (val && strlen(val) > 0)
	    snprintf(msg, sizeof(msg),
		     "The value of %s is %s. [*Client reply*]", nm, val);
	else
	    snprintf(msg, sizeof(msg),
		     "The option %s has no value. [*Client reply*]", nm);
	Add_message(msg);
    } else {
	snprintf(msg, sizeof(msg),
		 "No client option named \"%s\". [*Client reply*]", name);
	Add_message(msg);
    }

    XFREE(valcpy);
}

/*
 * NOTE: Store option assumes the passed pointers will remain valid.
 */
void Store_option(xp_option_t *opt)
{
    xp_option_t option;

    assert(opt->name);
    assert(strlen(opt->name) > 0);
    assert(opt->help);
    assert(strlen(opt->help) > 0);

    /*
     * Let's not allow several options with the same name 
     */
    if (Find_option(opt->name) != NULL) {
	warn("Trying to store duplicate option \"%s\"", opt->name);
	assert(0);
    }

    /*
     * Check that default value is in range 
     * NOTE: these assertions will hold also for options of other types 
     */
    assert(opt->int_defval >= opt->int_minval);
    assert(opt->int_defval <= opt->int_maxval);
    assert(opt->dbl_defval >= opt->dbl_minval);
    assert(opt->dbl_defval <= opt->dbl_maxval);

    memcpy(&option, opt, sizeof(xp_option_t));

    STORE(xp_option_t, options, num_options, max_options, option);

    opt = Find_option(opt->name);
    assert(opt);

    /* Set the default value. */
    switch (opt->type) {
    case xp_noarg_option:
	Set_noarg_option(opt, false, xp_option_origin_default);
	break;
    case xp_bool_option:
	Set_bool_option(opt, opt->bool_defval, xp_option_origin_default);
	break;
    case xp_int_option:
	Set_int_option(opt, opt->int_defval, xp_option_origin_default);
	break;
    case xp_double_option:
	Set_double_option(opt, opt->dbl_defval, xp_option_origin_default);
	break;
    case xp_string_option:
	assert(opt->str_defval);
	assert(opt->str_ptr || (opt->str_setfunc && opt->str_getfunc));
	Set_string_option(opt, opt->str_defval, xp_option_origin_default);
	break;
    case xp_key_option:
	assert(opt->key_defval);
	assert(opt->key != KEY_DUMMY);
	Set_key_option(opt, opt->key_defval, xp_option_origin_default);
	break;
    default:
	warn("Could not set default value for option %s", opt->name);
	break;
    }

}

typedef struct xpilotrc_line {
    xp_option_t *opt;
    const char *comment;
} xpilotrc_line_t;

static xpilotrc_line_t	*xpilotrc_lines = NULL;
static int num_xpilotrc_lines = 0, max_xpilotrc_lines = 0;
static int num_ok_options = 0;

/*
 * Function to parse an xpilotrc line if it is of the right form, otherwise
 * it is treated as a comment.
 *
 * Line must have this form to be valid:
 * 1. string "xpilot", checked for case insensitively
 * 2. "." or "*"
 * 3. option name of option recognised by Find_option().
 * 4. optional whitespace
 * 5. ":"
 * 6. optional whitespace
 * 7. optional value (if it doesn't exist, set option to value "")
 * 8. optional ";" followed by comments
 *
 * Here is some sort of pseudo regular expression:
 * xpilot{.*}option[whitespace]:[whitespace][value][; comment]
 */
static void Parse_xpilotrc_line(const char *line)
{
    char *lcpy = xp_safe_strdup(line);
    char *l = lcpy, *colon, *name, *value, *semicolon, *comment = NULL;
    xpilotrc_line_t t;
    xp_option_t *opt;
    int i;

    memset(&t, 0, sizeof(xpilotrc_line_t));

    if (!(strncasecmp(l, "xpilot.", 7) == 0
	  || strncasecmp(l, "xpilot*", 7) == 0))
	goto line_is_comment;
    
    /* line starts with "xpilot." or "xpilot*" (ignoring case) */
    l += strlen("xpilot.");

    colon = strchr(l, ':');
    if (colon == NULL) {
	/* no colon on line, not ok */
	warn("Xpilotrc line %d:", num_xpilotrc_lines + 1);
	warn("Line has no colon after option name, ignoring.");
	goto line_is_comment;
    }

    /*
     * Divide line into two parts, the first part containing the option
     * name and possible whitespace, the other one containing possible
     * white space, the option value and possibly a comment.
     */
    *colon = '\0';

    /* remove trailing whitespace from option name */
    i = ((int)strlen(l)) - 1;
    while (i >= 0 && isspace(l[i]))
	l[i--] = '\0';

    name = l;
    /*warn("looking for option \"%s\": %s",
      name, Find_option(name) ? "found" : "not found");*/

    opt = Find_option(name);
    if (opt == NULL)
	goto line_is_comment;

    if (Option_get_flags(opt) & XP_OPTFLAG_NEVER_SAVE) {
	/* discard the line */
	warn("Xpilotrc line %d:", num_xpilotrc_lines + 1);
	warn("Option %s must not be specified in xpilotrc.", name);
	warn("It will be removed from xpilotrc if you save configuration.");
	XFREE(lcpy);
	return;
    }

    /* did we see this before ? */
    for (i = 0; i < num_xpilotrc_lines; i++) {
	xpilotrc_line_t *x = &xpilotrc_lines[i];
	
	if (x->opt == opt) {
	    /* same option defined several times in xpilotrc */
	    warn("Xpilotrc line %d:", num_xpilotrc_lines + 1);
	    warn("Option %s previously given on line %d, ignoring new value.",
		 name, i + 1);
	    goto line_is_comment;
	}
    }

    /* option is known: proceed to handle the value */
    l = colon + 1;

    /* skip initial whitespace in value */
    while (isspace(*l))
	l++;

    value = l;
    /* everything after semicolon is comment, ignore it. */
    semicolon = strchr(l, ';');
    if (semicolon) {
	/*warn("found comment \"%s\" on line \"%s\"\n", semicolon, line);*/
	comment = xp_safe_strdup(semicolon);
	*semicolon = '\0';
    }

    if (!Set_option(name, value, xp_option_origin_xpilotrc)) {
	warn("Xpilotrc line %d:", num_xpilotrc_lines + 1);
	warn("Failed to set option %s value \"%s\", ignoring.", name, value);
	goto line_is_comment;
    }

    /*warn("option %s value is \"%s\"", name, value);*/

    t.opt = opt;
    t.comment = comment;
    STORE(xpilotrc_line_t,
	  xpilotrc_lines, num_xpilotrc_lines, max_xpilotrc_lines, t);
    num_ok_options++;
    XFREE(lcpy);
    return;

 line_is_comment:
    /*
     * If we can't parse the line, then we just treat it as a comment.
     */
    /*warn("Comment: \"%s\"", line);*/
    XFREE(comment);
    t.opt = NULL;
    t.comment = xp_safe_strdup(line);
    STORE(xpilotrc_line_t,
	  xpilotrc_lines, num_xpilotrc_lines, max_xpilotrc_lines, t);
    XFREE(lcpy);
}

static inline bool is_noarg_option(const char *name)
{
    xp_option_t *opt = Find_option(name);

    if (!opt || opt->type != xp_noarg_option)
	return false;
    return true;
}

int Xpilotrc_read(const char *path)
{
    char buf[4096]; /* long enough max line length in xpilotrc? */
    FILE *fp;

    assert(path);
    if (strlen(path) == 0) {
	warn("Xpilotrc_read: Zero length filename.");
	return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
	error("Xpilotrc_read: Failed to open file \"%s\"", path);
	return -2;
    }

    xpinfo("Reading options from xpilotrc file %s.", path);

    while (fgets(buf, sizeof buf, fp)) {
	char *cp;

	cp = strchr(buf, '\n');
	if (cp)
	    *cp = '\0';
	cp = strchr(buf, '\r');
	if (cp)
	    *cp = '\0';
	Parse_xpilotrc_line(buf);
    }

    /*warn("Total number of nonempty lines in xpilotrc: %d",
      num_xpilotrc_lines);
      warn("Number of options set: %d\n", num_ok_options);*/

    fclose(fp);

    return 0;
}


#define TABSIZE 8
static void Xpilotrc_create_line(char *buf, size_t size,
				 xp_option_t *opt,
				 const char *comment,
				 bool comment_whole_line)
{
    int len, numtabs, i;

    assert(buf != NULL);

    if (comment_whole_line) {
	const char *s = "; ";

	assert(size > strlen(s));
	strlcpy(buf, s, size);
	buf += strlen(s);
	size -= strlen(s);
    }
    else
	strcpy(buf, "");

    if (opt) {
	const char *value;

	snprintf(buf, size, "xpilot.%s:", opt->name);
	len = (int) strlen(buf);
	/* assume tabs are max size of TABSIZE */
	numtabs = ((5 * TABSIZE - 1) - len) / TABSIZE;
	for (i = 0; i < numtabs; i++)
	    strlcat(buf, "\t", size);
	value = Option_value_to_string(opt);
	if (value)
	    strlcat(buf, value, size);
    }

    if (comment)
	strlcat(buf, comment, size);
}
#undef TABSIZE

static void Xpilotrc_write_line(FILE *fp, const char *buf)
{
#ifndef _WINDOWS
    const char *endline = "\n";
#else
    const char *endline = "\r\n"; /* CR LF */
#endif
    /*warn("writing line \"%s\"", buf);*/

    fprintf(fp, "%s%s", buf, endline);
}

int Xpilotrc_write(const char *path)
{
    FILE *fp;
    int i;

    assert(path);
    if (strlen(path) == 0) {
	warn("Xpilotrc_write: Zero length filename.");
	return -1;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
	error("Xpilotrc_write: Failed to open file \"%s\"", path);
	return -2;
    }

    /* make sure all options are in the xpilotrc */
    for (i = 0; i < num_options; i++) {
	xp_option_t *opt = Option_by_index(i);
	xp_option_origin_t origin;
	xpilotrc_line_t t;
	int j;
	bool was_in_xpilotrc = false;

	memset(&t, 0, sizeof(xpilotrc_line_t));

	for (j = 0; j < num_xpilotrc_lines; j++) {
	    xpilotrc_line_t *lp = &xpilotrc_lines[j];

	    if (lp->opt == opt)
		was_in_xpilotrc = true;
	}

	if (was_in_xpilotrc)
	    continue;

	/* If this wasn't in xpilotrc, don't save it */
	if (Option_get_flags(opt) & XP_OPTFLAG_KEEP)
	    continue;
	/* Let's not save these */
	if (Option_get_flags(opt) & XP_OPTFLAG_NEVER_SAVE)
	    continue;

	origin = Option_get_origin(opt);
	assert(origin != xp_option_origin_xpilotrc);

	if (origin == xp_option_origin_cmdline)
	    continue;
	if (origin == xp_option_origin_env)
	    continue;

	/*
	 * Let's save commented default values to xpilotrc, unless
	 * such a comment already exists.
	 */
	if (origin == xp_option_origin_default) {
	    char buf[4096];
	    bool found = false;

	    Xpilotrc_create_line(buf, sizeof(buf), opt, NULL, true);

	    for (j = 0; j < num_xpilotrc_lines; j++) {
		xpilotrc_line_t *lp = &xpilotrc_lines[j];

		if (lp->opt == NULL
		    && lp->comment != NULL
		    && !strcmp(buf, lp->comment)) {
		    found = true;
		    break;
		}
	    }		

	    if (found)
		/* was already in xpilotrc */
		continue;

	    t.comment = xp_safe_strdup(buf);
	} else
	    t.opt = opt;

	STORE(xpilotrc_line_t,
	      xpilotrc_lines, num_xpilotrc_lines, max_xpilotrc_lines, t);
    }

    for (i = 0; i < num_xpilotrc_lines; i++) {
	char buf[4096];
	xpilotrc_line_t *lp = &xpilotrc_lines[i];

	Xpilotrc_create_line(buf, sizeof(buf), lp->opt, lp->comment, false);

	Xpilotrc_write_line(fp, buf);
    }

    fclose(fp);
#ifdef __EMSCRIPTEN__
    browser_options_saved();
#endif

    return 0;
}
 

void Parse_options(int *argcp, char **argvp)
{
    int arg_ind, num_remaining_args, num_servers = 0, i;
    char path[PATH_MAX + 1];

    Xpilotrc_get_filename(path, sizeof(path));
    Xpilotrc_read(path);

    /*
     * Here we step trough argc - 1 arguments, leaving
     * only the arguments that might be server names.
     */
    arg_ind = 1;
    num_remaining_args = *argcp - 1;

    while (num_remaining_args > 0) {
	if (argvp[arg_ind][0] == '-') {
	    char *arg = &argvp[arg_ind][1];

	    /*
	     * kps -
	     * Incomplete GNU style option support, this only works for
	     * options with no argument, e.g. --version
	     * A complete implementation should also support option given
	     * like this:
	     * --option=value
	     */
	    if (arg[0] == '-')
		arg++;

	    if (is_noarg_option(arg)) {
		Set_option(arg, "true", xp_option_origin_cmdline);
		num_remaining_args--;
		for (i = 0; i < num_remaining_args; i++)
		    argvp[arg_ind + i] = argvp[arg_ind + i + 1];
	    } else {
		bool ok = false;

		if (num_remaining_args >= 2) {
		    ok = Set_option(arg, argvp[arg_ind + 1],
				    xp_option_origin_cmdline);
		    if (ok) {
			num_remaining_args -= 2;
			for (i = 0; i < num_remaining_args; i++)
			    argvp[arg_ind + i] = argvp[arg_ind + i + 2];
		    }
		}

		if (!ok) {
		    warn("Unknown or incomplete option '%s'", argvp[arg_ind]);
		    warn("Type: %s -help to see a list of options", argvp[0]);
		    exit(1);
		}
	    }
	} else {
	    /* assume this is a server name. */
	    arg_ind++;
	    num_remaining_args--;
	    num_servers++;
	}
    }

    /*
     * The remaining args are assumed to be names of servers to try to contact.
     * + 1 is for the program name.
     */
    for (i = num_servers + 1; i < *argcp; i++)
	argvp[i] = NULL;
    *argcp = num_servers + 1;

    if (xpArgs.help)
	Usage();

    if (xpArgs.version)
	Version();

#ifdef SOUND
    audioInit(connectParam.disp_name);
#endif /* SOUND */
}



const char *Get_keyHelpString(keys_t key)
{
    int i;
    char *nl;
    static char buf[MAX_CHARS];

    for (i = 0; i < num_options; i++) {
	xp_option_t *opt = Option_by_index(i);

	if (opt->key == key) {
	    strlcpy(buf, opt->help, sizeof buf);
	    if ((nl = strchr(buf, '\n')) != NULL)
		*nl = '\0';
	    return buf;
	}
    }

    return NULL;
}


const char *Get_keyResourceString(keys_t key)
{
    int i;

    for (i = 0; i < num_options; i++) {
	xp_option_t *opt = Option_by_index(i);

	if (opt->key == key)
	    return opt->name;
    }

    return NULL;
}

#ifndef _WINDOWS
void Xpilotrc_get_filename(char *path, size_t size)
{
    const char *home = getenv("HOME");
    const char *defaultFile = ".xpilotrc";
    const char *optionalFile = getenv("XPILOTRC");

    if (optionalFile != NULL)
	strlcpy(path, optionalFile, size);
    else if (home != NULL) {
	strlcpy(path, home, size);
	strlcat(path, "/", size);
	strlcat(path, defaultFile, size);
    } else
	strlcpy(path, "", size);
}
#else
void Xpilotrc_get_filename(char *path, size_t size)
{
    strlcpy(path, "xpilotrc.txt", size);
}
#endif /* _WINDOWS */
