/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2000-2004 by
 *
 *      Uoti Urpala          <uau@users.sourceforge.net>
 *      Juha Lindström       <juhal@users.sourceforge.net>
 *      Kristian Söderblom   <kps@users.sourceforge.net>
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

#include <zlib.h>
#include "xpserver.h"

#define DEFAULT_POS { -1, -1 }

/*
 * The world whose map we are currently parsing.
 */
static bool parsing_general_options = false;
static cannon_t *current_cannon = NULL;
static base_t *current_base = NULL;

static void tagstart(void *data, const char *el, const char **attr)
{
    static double scale = 1;
    static bool xptag = false;

    UNUSED_PARAM(data);
    if (!strcasecmp(el, "XPilotMap")) {
	double version = 0;
	while (*attr) {
	    if (!strcasecmp(*attr, "version"))
		version = atof(*(attr + 1));
	    attr += 2;
	}
	if (version == 0) {
	    warn("Old(?) map file with no version number");
	    warn("Not guaranteed to work");
	}
	else if (version < 1)
	    warn("Impossible version in map file");
	else if (version > 1.2) {
	    warn("Map file has newer version than this server recognizes.");
	    warn("The map file might use unsupported features.");
	}
	xptag = true;
	return;
    }

    if (!xptag) {
	fatal("This doesn't look like a map file "
	      " (XPilotMap must be first tag).");
	return; /* not reached */
    }

    if (!strcasecmp(el, "Polystyle")) {
	char id[100];
	int color = 0, texture_id = 0, defedge_id = 0, flags = 0;

	while (*attr) {
	    if (!strcasecmp(*attr, "id"))
		strlcpy(id, *(attr + 1), sizeof(id));
	    if (!strcasecmp(*attr, "color"))
		color = strtol(*(attr + 1), NULL, 16);
	    if (!strcasecmp(*attr, "texture"))
		texture_id = P_get_bmp_id(*(attr + 1));
	    if (!strcasecmp(*attr, "defedge"))
		defedge_id = P_get_edge_id(*(attr + 1));
	    if (!strcasecmp(*attr, "flags"))
		flags = atoi(*(attr + 1)); /* names @!# */
	    attr += 2;
	}
	P_polystyle(id, color, texture_id, defedge_id, flags);
	return;
    }

    if (!strcasecmp(el, "Edgestyle")) {
	char id[100];
	int width = 0, color = 0, style = 0;

	while (*attr) {
	    if (!strcasecmp(*attr, "id"))
		strlcpy(id, *(attr + 1), sizeof(estyles[0].id));
	    if (!strcasecmp(*attr, "width"))
		width = atoi(*(attr + 1));
	    if (!strcasecmp(*attr, "color"))
		color = strtol(*(attr + 1), NULL, 16);
	    if (!strcasecmp(*attr, "style")) /* !@# names later */
		style = atoi(*(attr + 1));
	    attr += 2;
	}
	P_edgestyle(id, width, color, style);
	return;
    }

    if (!strcasecmp(el, "Bmpstyle")) {
	char id[100];
	char filename[30];
	int flags = 0;

	/* add checks that these are filled !@# */
	while (*attr) {
	    if (!strcasecmp(*attr, "id"))
		strlcpy(id, *(attr + 1), sizeof(id));
	    if (!strcasecmp(*attr, "filename"))
		strlcpy(filename, *(attr + 1), sizeof(filename));
	    if (!strcasecmp(*attr, "scalable"))
		if (!strcasecmp(*(attr + 1), "yes"))
		    flags |= 1;
	    attr += 2;
	}
	P_bmpstyle(id, filename, flags);
	return;
    }

    if (!strcasecmp(el, "Scale")) { /* "Undocumented feature" */
	if (!*attr || strcasecmp(*attr, "value"))
	    warn("Invalid Scale");
	else
	    scale = atof(*(attr + 1));
	return;
    }

    if (!strcasecmp(el, "BallArea")) {
	P_start_ballarea();
	return;
    }

    if (!strcasecmp(el, "BallTarget")) {
	int team = TEAM_NOT_SET;
	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    attr += 2;
	}
	/*
	 * kps - Currently we don't know mapobj for balltargets,
	 * this means that options.captureTheFlag stuff does not work
	 * on xp2 maps.
	 */
	P_start_balltarget(team, NO_IND);
	return;
    }

    if (!strcasecmp(el, "Decor")) {
	P_start_decor();
	return;
    }

    if (!strcasecmp(el, "Polygon")) {
	clpos_t pos = DEFAULT_POS;
	int style = -1;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "style"))
		style = P_get_poly_id(*(attr + 1));
	    attr += 2;
	}
	P_start_polygon(pos, style);
	return;
    }

    if (!strcasecmp(el, "Style")) {
	char state[100];
	int style = -1;

	while (*attr) {
	    if (!strcasecmp(*attr, "state"))
		strlcpy(state, *(attr + 1), sizeof(state));
	    if (!strcasecmp(*attr, "id"))
		style = P_get_poly_id(*(attr + 1));
	    attr += 2;
	}
	P_style(state, style);
	return;
    }

    if (!strcasecmp(el, "Check")) {
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    attr += 2;
	}
	World_place_check(pos, -1);
	return;
    }

    if (!strcasecmp(el, "Fuel")) {
	int team = TEAM_NOT_SET;
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    attr += 2;
	}
	World_place_fuel(pos, team);
	return;
    }

    if (!strcasecmp(el, "Base")) {
	int team = TEAM_NOT_SET, dir = DIR_UP, order = 0;
	clpos_t pos = DEFAULT_POS;
	int ind;

	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "dir"))
		dir = atoi(*(attr + 1));
	    if (!strcasecmp(*attr, "order"))
		order = atoi(*(attr + 1));
	    attr += 2;
	}
	if (team < 0 || team >= MAX_TEAMS) {
	    warn("Illegal team number in base tag.\n");
	    exit(1);
	}
	ind = World_place_base(pos, dir, team, order);
	current_base = Base_by_index(ind);
	return;
    }

    if (!strcasecmp(el, "Ball")) {
	int team = TEAM_NOT_SET;
	clpos_t pos = DEFAULT_POS;
	int style = 0xff; /* default - client draws ball however it wants */

	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "style"))
		style = P_get_poly_id(*(attr + 1));
	    attr += 2;
	}
	World_place_treasure(pos, team, false, style);
	return;
    }

    if (!strcasecmp(el, "Cannon")) {
	int team = TEAM_NOT_SET, dir = DIR_UP, cannon_ind;
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    else if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "dir"))
		dir = atoi(*(attr + 1));
	    attr += 2;
	}
	cannon_ind = World_place_cannon(pos, dir, team);
	P_start_cannon(cannon_ind);
	current_cannon = Cannon_by_index(cannon_ind);
	return;
    }

    if (!strcasecmp(el, "Target")) {
	int team = TEAM_NOT_SET, target_ind;
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "team"))
		team = atoi(*(attr + 1));
	    else if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    attr += 2;
	}
	target_ind = World_place_target(pos, team);
	P_start_target(target_ind);
	return;
    }

    if (!strcasecmp(el, "ItemConcentrator")) {
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    attr += 2;
	}
	World_place_item_concentrator(pos);
	return;
    }

    if (!strcasecmp(el, "AsteroidConcentrator")) {
	clpos_t pos = DEFAULT_POS;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    attr += 2;
	}
	World_place_asteroid_concentrator(pos);
	return;
    }

    if (!strcasecmp(el, "Grav")) {
	clpos_t pos = DEFAULT_POS;
	double force = 0.0;
	int type = SPACE;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "force"))
		force = atof(*(attr + 1));
	    else if (!strcasecmp(*attr, "type")) {
		const char *s = *(attr + 1);

		if (!strcasecmp(s, "pos"))
		    type = POS_GRAV;
		else if (!strcasecmp(s, "neg"))
		    type = NEG_GRAV;
		else if (!strcasecmp(s, "cwise"))
		    type = CWISE_GRAV;
		else if (!strcasecmp(s, "acwise"))
		    type = ACWISE_GRAV;
		else if (!strcasecmp(s, "up"))
		    type = UP_GRAV;
		else if (!strcasecmp(s, "down"))
		    type = DOWN_GRAV;
		else if (!strcasecmp(s, "right"))
		    type = RIGHT_GRAV;
		else if (!strcasecmp(s, "left"))
		    type = LEFT_GRAV;
	    }

	    attr += 2;
	}
	if (type == SPACE) {
	    warn("Illegal type in grav tag.\n");
	    exit(1);
	}
	World_place_grav(pos, force, type);
	return;
    }

    if (!strcasecmp(el, "Wormhole")) {
	clpos_t pos = DEFAULT_POS;
	wormtype_t type = WORM_NORMAL;
	int wh_ind;

	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		pos.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "y"))
		pos.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    else if (!strcasecmp(*attr, "type")) {
		const char *s = *(attr + 1);

		if (!strcasecmp(s, "normal"))
		    type = WORM_NORMAL;
		else if (!strcasecmp(s, "in"))
		    type = WORM_IN;
		else if (!strcasecmp(s, "out"))
		    type = WORM_OUT;
		else if (!strcasecmp(s, "fixed"))
		    type = WORM_FIXED;
	    }

	    attr += 2;
	}
	wh_ind = World_place_wormhole(pos, type);
	P_start_wormhole(wh_ind);
	return;
    }

    if (!strcasecmp(el, "FrictionArea")) {
	double fric = 0.0;
	int area_ind;
	clpos_t pos = { 0, 0 }; /* unused place holder */

	while (*attr) {
	    if (!strcasecmp(*attr, "friction"))
		fric = atof(*(attr + 1));
	    attr += 2;
	}
	area_ind = World_place_friction_area(pos, fric);
	P_start_friction_area(area_ind);
	return;
    }

    if (!strcasecmp(el, "Option")) {
	const char *name = NULL, *value = NULL;
	while (*attr) {
	    if (!strcasecmp(*attr, "name"))
		name = *(attr + 1);
	    if (!strcasecmp(*attr, "value"))
		value = *(attr + 1);
	    attr += 2;
	}
	if (parsing_general_options)
	    Option_set_value(name, value, 0, OPT_MAP);
	else if (current_base)
	    Base_set_option(current_base, name, value);
	else if (current_cannon)
	    Cannon_set_option(current_cannon, name, value);	    
	else {
	    warn("Options can be specified for:");
	    warn("<GeneralOptions>, <Base> or <Cannon>.");
	    warn("Option %s given out of context in map.", name);
	    exit(1);
	}
	return;
    }

    if (!strcasecmp(el, "Offset")) {
	clpos_t offset = DEFAULT_POS;
	int edgestyle = -1;
	while (*attr) {
	    if (!strcasecmp(*attr, "x"))
		offset.cx = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "y"))
		offset.cy = (click_t)(atoi(*(attr + 1)) * scale);
	    if (!strcasecmp(*attr, "style"))
		edgestyle = P_get_edge_id(*(attr + 1));
	    attr += 2;
	}
	P_offset(offset, edgestyle);
	return;
    }

    if (!strcasecmp(el, "GeneralOptions")) {
	parsing_general_options = true;
	return;
    }

    warn("Unknown map tag: \"%s\"", el);
    return;
}


static void tagend(void *data, const char *el)
{
    UNUSED_PARAM(data);
    if (!strcasecmp(el, "Decor"))
	P_end_decor();
    else if (!strcasecmp(el, "Base"))
	current_base = NULL;
    else if (!strcasecmp(el, "BallArea"))
	P_end_ballarea();
    else if (!strcasecmp(el, "BallTarget"))
	P_end_balltarget();
    else if (!strcasecmp(el, "Cannon")) {
	P_end_cannon();
	Cannon_init(current_cannon);
	current_cannon = NULL;
    } else if (!strcasecmp(el, "FrictionArea"))
	P_end_friction_area();
    else if (!strcasecmp(el, "Target"))
	P_end_target();
    else if (!strcasecmp(el, "Wormhole"))
	P_end_wormhole();
    else if (!strcasecmp(el, "Polygon"))
	P_end_polygon();

    if (!strcasecmp(el, "GeneralOptions")) {
	parsing_general_options = false;
	/* ok, got to the end of options */
	Options_parse();
	/* kps - this can fail - fix */
	Grok_map_options();
    }
    return;
}


bool isXp2MapFile(FILE* ifile)
{
    char start[] = "<XPilotMap";
    char buf[16];
    int n;

    n = fread(buf, 1, sizeof(buf), ifile);
    if (n < 0) {
	error("Error reading map!");
	return false;
    }
    if (n == 0)
	return false;

    /* assume this works */
    fseek(ifile, 0, SEEK_SET);
    /* gz magic from gzio.h */
    if (buf[0] == (char)0x1f && buf[1] == (char)0x8b)
	return true;
    if (!strncmp(start, buf, strlen(start)))
	return true;
    return false;
}

bool parseXp2MapFile(char* fname, optOrigin opt_origin)
{
    gzFile in;
    char buff[8192];
    int len, last_chunk;
    unsigned left;
    XML_Parser p = XML_ParserCreate(NULL);

    UNUSED_PARAM(opt_origin);
    if (!p) {
	warn("Creating Expat instance for map parsing failed.\n");
	return false;
    }
    XML_SetElementHandler(p, tagstart, tagend);
    
    int descriptor = Xp_open_read(fname);
    in = descriptor >= 0 ? gzdopen(descriptor, "rb") : NULL;
    if (in == NULL) {
	if (descriptor >= 0) close(descriptor);
	XML_ParserFree(p);
	error("Error reading map!");
	return false;
    }
    if (gzgets(in, buff, 8192) == Z_NULL) {
	error("Error reading map!");
	gzclose(in);
	return false;
    }
    left = 1 << 30;
    if (strncmp("XPD ", buff, 4) == 0) {
	if (gzgets(in, buff, 8192) == Z_NULL
	    || sscanf(buff, "%*s %u", &left) != 1) {
	    error("Bad xpd file header");
	    gzclose(in);
	    return false;
	}
    } else {
	if (gzrewind(in) == -1) {
	    error("Error reading map!");
	    gzclose(in);
	    return false;
	}
    }
    do {
        len = gzread(in, buff, MIN(8192, left));
	if (len < 0) {
	    error("Error reading map!");
	    gzclose(in);
	    return false;
	}
	left -= len;
	last_chunk = (left == 0 || len < 8192);
	if (!XML_Parse(p, buff, len, last_chunk)) {
	    warn("Parse error reading map at line %d:\n%s\n",
		  XML_GetCurrentLineNumber(p),
		  XML_ErrorString(XML_GetErrorCode(p)));
	    gzclose(in);
	    return false;
	}
    } while (!last_chunk);
    gzclose(in);
    return true;
}
