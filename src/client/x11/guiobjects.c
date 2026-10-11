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

#include "xpclient_x11.h"

static bool texturedShips = false; /* Turned this off because the images drawn
				    * don't match the actual shipshape used
				    * for wall collisions by the server. */
static int ballColor;		/* Color index for ball drawing */
static int connColor;		/* Color index for connector drawing */
static int teamShotColor;	/* Color index for harmless shot drawing */
static int zeroLivesColor;	/* Color to associate with 0 lives */
static int oneLifeColor;	/* Color to associate with 1 life */
static int twoLivesColor;	/* Color to associate with 2 lives */
static int manyLivesColor;	/* Color to associate with >2 lives */
static int selfLWColor;		/* Color index for selfLifeWarning */
static int enemyLWColor;	/* Color index for enemyLifeWarning */
static int teamLWColor;		/* Color index for teamLifeWarning */
static int shipNameColor;	/* Color index for ship name drawing */
static int mineNameColor;	/* Color index for mine name drawing */
static int teamShipColor;   	/* Color index to associate with team 0 */
static int team0Color;		/* Color index to associate with team 0 */
static int team1Color;		/* Color index to associate with team 1 */
static int team2Color;		/* Color index to associate with team 2 */
static int team3Color;		/* Color index to associate with team 3 */
static int team4Color;		/* Color index to associate with team 4 */
static int team5Color;		/* Color index to associate with team 5 */
static int team6Color;		/* Color index to associate with team 6 */
static int team7Color;		/* Color index to associate with team 7 */
static int team8Color;		/* Color index to associate with team 8 */
static int team9Color;		/* Color index to associate with team 9 */


static int asteroidRawShapes[NUM_ASTEROID_SHAPES][NUM_ASTEROID_POINTS][2] = {
    { ASTEROID_SHAPE_0 },
    { ASTEROID_SHAPE_1 },
};


position_t *asteroidShapes[NUM_ASTEROID_SHAPES][NUM_ASTEROID_POINTS];


int Init_asteroids(void)
{
    int		shp, i;
    size_t	point_size;
    size_t	total_size;
    char	*dynmem;

    /*
     * Allocate memory for all the asteroid points.
     */
    point_size = sizeof(position_t) * RES;
    total_size = point_size * NUM_ASTEROID_POINTS * NUM_ASTEROID_SHAPES;
    if ((dynmem = (char *) malloc(total_size)) == NULL) {
	error("Not enough memory for asteroid shapes");
	return -1;
    }

    /*
     * For each asteroid-shape rotate all points.
     */
    for ( shp = 0; shp < NUM_ASTEROID_SHAPES; shp++ ) {
	for ( i = 0; i < NUM_ASTEROID_POINTS; i++ ) {
	    asteroidShapes[shp][i] = (position_t *) dynmem;
	    dynmem += point_size;
	    asteroidShapes[shp][i][0].x = asteroidRawShapes[shp][i][0];
	    asteroidShapes[shp][i][0].y = asteroidRawShapes[shp][i][1];
	    Rotate_position( &asteroidShapes[shp][i][0] );
	}
    }

    return 0;
}


void Gui_paint_item_symbol(int type, Drawable d, GC mygc, int x, int y, int c)
{
    if (!texturedObjects) {
	gcv.stipple = itemBitmaps[type];
	gcv.fill_style = FillStippled;
	gcv.ts_x_origin = x;
	gcv.ts_y_origin = y;
	XChangeGC(dpy, mygc,
		  GCStipple|GCFillStyle|GCTileStipXOrigin|GCTileStipYOrigin,
		  &gcv);
	rd.paintItemSymbol(type, d, mygc, x, y, c);
	XFillRectangle(dpy, d, mygc, x, y, ITEM_SIZE, ITEM_SIZE);
	gcv.fill_style = FillSolid;
	XChangeGC(dpy, mygc, GCFillStyle, &gcv);
    } else
	Bitmap_paint(d, BM_ALL_ITEMS, x, y, type);
}

void Gui_paint_item(int type, Drawable d, GC mygc, int x, int y)
{
    const int		SIZE = ITEM_TRIANGLE_SIZE;
    XPoint		points[5];

#ifndef NO_ITEM_TRIANGLES
    points[0].x = x - SIZE;
    points[0].y = y - SIZE;
    points[1].x = x;
    points[1].y = y + SIZE;
    points[2].x = x + SIZE;
    points[2].y = y - SIZE;
    points[3] = points[0];
    SET_FG(colors[BLUE].pixel);
    rd.drawLines(dpy, d, mygc, points, 4, CoordModeOrigin);
#endif

    SET_FG(colors[RED].pixel);
#if 0
    str[0] = itemtype_ptr[i].type + '0';
    str[1] = '\0';
    rd.drawString(dpy, d, mygc,
		  x - XTextWidth(gameFont, str, 1)/2,
		  y + SIZE - 1,
		  str, 1);
#endif
    Gui_paint_item_symbol(type, d, mygc,
			  x - ITEM_SIZE/2,
			  y - SIZE + 2, ITEM_PLAYFIELD);
}

void Gui_paint_item_object(int type, int x, int y)
{
    Gui_paint_item(type, drawPixmap, gameGC, WINSCALE(X(x)), WINSCALE(Y(y)));
}

void Gui_paint_ball(int x, int y, int style)
{
    unsigned long rgb = 0;

    x = X(x);
    y = Y(y);

    /*
     * kps - currently style 0xff means no style.
     * This code assumes num_polygon_styles < 256.
     */
    if (style >= 0 && style < num_polygon_styles)
	rgb = polygon_styles[style].rgb;

    if (!texturedObjects) {
	/* hack */
	if (rgb == 0)
	    Arc_add(ballColor, x - BALL_RADIUS, y - BALL_RADIUS,
		    2 * BALL_RADIUS, 2 * BALL_RADIUS, 0, 64 * 360);
	else
	    Arc_add_rgb(rgb, ballColor, x - BALL_RADIUS, y - BALL_RADIUS,
			2 * BALL_RADIUS, 2 * BALL_RADIUS, 0, 64 * 360);
    } else {
	if (style == 0xff) {
	    Bitmap_paint(drawPixmap, BM_BALL, WINSCALE(x - BALL_RADIUS),
			 WINSCALE(y - BALL_RADIUS), 0);
	} else {
	    Bitmap_paint_blended(drawPixmap, BM_BALL_GRAY,
				 WINSCALE(x - BALL_RADIUS),
				 WINSCALE(y - BALL_RADIUS), rgb);
	}
    }
}


void Gui_paint_ball_connector(int x_1, int y_1, int x_2, int y_2)
{
    x_2 = X(x_2);
    y_2 = Y(y_2);
    x_1 = X(x_1);
    y_1 = Y(y_1);
    Segment_add(connColor, x_1, y_1, x_2, y_2);
}

static void Gui_paint_mine_name(int x, int y, char *name)
{
    int		name_len, name_width;

    if (!name || !mineNameColor)
	return;

    SET_FG(colors[mineNameColor].pixel);

    name_len = strlen(name);
    name_width = XTextWidth(gameFont, name, name_len);

    rd.drawString(dpy, drawPixmap, gameGC,
		  WINSCALE(x) - name_width / 2,
		  WINSCALE(y + 4) + gameFont->ascent + 1,
		  name, name_len);
}

void Gui_paint_mine(int x, int y, int teammine, char *name)
{
    if (!texturedObjects) {
	static double	lastScaleFactor;
	static XPoint	mine_points[21];
	static XPoint	world_mine_points[21] = {
	    { 0, 0 },
	    { 1, 0 },
	    { 0, -1 },
	    { 4, 0 },
	    { 0, -1 },
	    { 6, 0 },
	    { 0, 1 },
	    { 4, 0 },
	    { 0, 1 },
	    { 1, 0 },
	    { 0, 2 },
	    { -1, 0 },
	    { 0, 1 },
	    { -4, 0 },
	    { 0, 1 },
	    { -6, 0 },
	    { 0, -1 },
	    { -4, 0 },
	    { 0, -1 },
	    { -1, 0 },
	    { 0, -2 }
	};

	if (lastScaleFactor != clData.scaleFactor) {
	    int			i;
	    lastScaleFactor = clData.scaleFactor;
	    for (i = 1; i < 21; ++i) {
		mine_points[i].x = WINSCALE(world_mine_points[i].x);
		mine_points[i].y = WINSCALE(world_mine_points[i].y);
	    }
	}

	x = X(x);
	y = Y(y);
	mine_points[0].x = WINSCALE(x - 8);
	mine_points[0].y = WINSCALE(y - 1);
	if (teammine == 0) {
	    SET_FG(colors[BLUE].pixel);
	    rd.fillRectangle(dpy, drawPixmap, gameGC,
			     WINSCALE(x - 7), WINSCALE(y - 2),
			     UWINSCALE(15), UWINSCALE(5));
	}

	SET_FG(colors[WHITE].pixel);
	rd.drawLines(dpy, drawPixmap, gameGC,
		   mine_points, 21, CoordModePrevious);

	if (name)
	    Gui_paint_mine_name(x, y, name);
    }
    else {
	x = X(x);
	y = Y(y);
	if (teammine == 0) {
	    SET_FG(colors[BLUE].pixel);
	    Bitmap_paint(drawPixmap, BM_MINE_OTHER, WINSCALE(x - 10),
			 WINSCALE(y - 7), 0);
	}
	else {
	    SET_FG(colors[WHITE].pixel);
	    Bitmap_paint(drawPixmap, BM_MINE_TEAM, WINSCALE(x - 10),
			 WINSCALE(y - 7), 0);
	}

	if (name)
	    Gui_paint_mine_name(x, y, name);
    }
}


void Gui_paint_sparks_begin(void)
{
}


void Gui_paint_spark(int color, int x, int y)
{
    color = spark_color[color];

    Rectangle_add(color,
		  x - sparkSize/2,
		  y - sparkSize/2,
		  sparkSize, sparkSize);

}


void Gui_paint_sparks_end(void)
{
}


void Gui_paint_wreck(int x, int y, bool deadly, int wtype, int rot, int size)
{
    int color, cnt, tx, ty;
    static XPoint points[NUM_WRECKAGE_POINTS+2];

    for (cnt = 0; cnt < NUM_WRECKAGE_POINTS; cnt++) {
	tx = (int)(wreckageShapes[wtype][cnt][rot].x * size) >> 8;
	ty = (int)(wreckageShapes[wtype][cnt][rot].y * size) >> 8;

	points[cnt].x = WINSCALE(X(x + tx));
	points[cnt].y = WINSCALE(Y(y + ty));

    }
    points[cnt++] = points[0];

    color = (deadly) ? WHITE: RED;
    SET_FG(colors[color].pixel);
    rd.drawLines(dpy, drawPixmap, gameGC, points, cnt, 0);
}

void Gui_paint_asteroids_begin(void)
{
}

void Gui_paint_asteroids_end(void)
{
}

void Gui_paint_asteroid(int x, int y, int type, int rot, int size)
{
    int cnt, tx, ty;
    static XPoint points[NUM_ASTEROID_POINTS+2];

    type = type % NUM_ASTEROID_SHAPES;
    for (cnt = 0; cnt < NUM_ASTEROID_POINTS; cnt++) {
	tx = (int)(asteroidShapes[type][cnt][rot].x * size * 1.4);
	ty = (int)(asteroidShapes[type][cnt][rot].y * size * 1.4);

	points[cnt].x = WINSCALE(X(x + tx));
	points[cnt].y = WINSCALE(Y(y + ty));
    }
    points[cnt++] = points[0];

    SET_FG(colors[WHITE].pixel);
    rd.drawLines(dpy, drawPixmap, gameGC, points, cnt, 0);
}


static void Gui_paint_nastyshot(int color, int x, int y, int size)
{
    int z = size;

    if (rfrac() < 0.5) {
	Segment_add(color,
		    x - z, y - z,
		    x + z, y + z);
	Segment_add(color,
		    x + z, y - z,
		    x - z, y + z);
    } else {
	Segment_add(color,
		    x - z, y,
		    x  + z, y);
	Segment_add(color,
		    x, y - z,
		    x, y + z);
    }
}


void Gui_paint_fastshot(int color, int x, int y)
{
    /* this is for those pesky invisible shots */
    if (color == 0)
	return;

    if (!texturedObjects) {
        int z = shotSize/2;

	if (instruments.showNastyShots)
	    Gui_paint_nastyshot(color, x, y, z);
	else {
	    /* Show round shots - jiman392 */
	    if (shotSize > 2) {
		SET_FG(colors[color].pixel);
		rd.fillArc(dpy, drawPixmap, gameGC,
			   WINSCALE(x - z), WINSCALE(y - z),
			   UWINSCALE(shotSize), UWINSCALE(shotSize), 
			   0, 64*360);
	    } else
		Rectangle_add(color,
			      x - z,
			      y - z,
			      shotSize, shotSize);
	}
    }
    else {
	int s_size = MIN(shotSize, 16);
	int z = s_size / 2;

	Bitmap_paint(drawPixmap, BM_BULLET, WINSCALE(x) - z,
		     WINSCALE(y) - z, s_size - 1);
    }
}

void Gui_paint_teamshot(int x, int y)
{
    int color = teamShotColor;

    if (color == 0)
	return;

    if (!texturedObjects) {
        int z = teamShotSize/2;

	if (instruments.showNastyShots)
	    Gui_paint_nastyshot(color, x, y, z);
	else {
	    /* Show round shots - jiman392 */
	    if (teamShotSize > 2) {
		SET_FG(colors[color].pixel);
		rd.fillArc(dpy, drawPixmap, gameGC,
			   WINSCALE(x - z), WINSCALE(y - z),
			   UWINSCALE(teamShotSize), UWINSCALE(teamShotSize), 
			   0, 64*360);
	    } else
		Rectangle_add(color,
			      x - z,
			      y - z,
			      teamShotSize, teamShotSize);
	}
    }
    else {
	int s_size = MIN(teamShotSize, 16);
	int z = s_size / 2;
	Bitmap_paint(drawPixmap, BM_BULLET_OWN, WINSCALE(x) - z,
		     WINSCALE(y) - z, s_size - 1);
    }
}


void Gui_paint_missiles_begin(void)
{
    SET_FG(colors[WHITE].pixel);
    XSetLineAttributes(dpy, gameGC, 4,
		       LineSolid, CapButt, JoinMiter);
}


void Gui_paint_missiles_end(void)
{
    XSetLineAttributes(dpy, gameGC, 0,
		       LineSolid, CapButt, JoinMiter);
}


void Gui_paint_missile(int x, int y, int len, int dir)
{
   int		x_1, x_2, y_1, y_2;

    x_1 = X(x);
    y_1 = Y(y);
    x_2 = (int)(x_1 - tcos(dir) * len);
    y_2 = (int)(y_1 + tsin(dir) * len);
    rd.drawLine(dpy, drawPixmap, gameGC,
		WINSCALE(x_1), WINSCALE(y_1), WINSCALE(x_2), WINSCALE(y_2));
}


void Gui_paint_lasers_begin(void)
{
    XSetLineAttributes(dpy, gameGC, 3,
		       LineSolid, CapButt, JoinMiter);
}


void Gui_paint_lasers_end(void)
{
    XSetLineAttributes(dpy, gameGC, 0,
		       LineSolid, CapButt, JoinMiter);
}


void Gui_paint_laser(int color, int x_1, int y_1, int len, int dir)
{
    int		x_2, y_2;

    x_2 = (int)(x_1 + len * tcos(dir));
    y_2 = (int)(y_1 + len * tsin(dir));
    if ((unsigned)(color) >= NUM_COLORS)
	color = WHITE;
    SET_FG(colors[color].pixel);
    rd.drawLine(dpy, drawPixmap, gameGC,
		WINSCALE(X(x_1)), WINSCALE(Y(y_1)),
		WINSCALE(X(x_2)), WINSCALE(Y(y_2)));
}


void Gui_paint_paused(int x, int y, int count)
{
    if (!texturedObjects) {
	int		x_0, y_0;
	static int	pauseCharWidth = -1;
	const unsigned	half_pause_size = 3*BLOCK_SZ/7;

	if (pauseCharWidth < 0)
	    pauseCharWidth = XTextWidth(gameFont, "P", 1);

	SET_FG(colors[BLUE].pixel);
	x_0 = X(x - half_pause_size);
	y_0 = Y(y + half_pause_size);
	rd.fillRectangle(dpy, drawPixmap, gameGC,
			 WINSCALE(x_0), WINSCALE(y_0),
			 UWINSCALE(2*half_pause_size+1),
			 UWINSCALE(2*half_pause_size+1));
	if (count <= 0 || loopsSlow % 10 >= 5) {
	    SET_FG(colors[WHITE].pixel);
	    rd.drawRectangle(dpy, drawPixmap, gameGC,
			     WINSCALE(x_0 - 1),
			     WINSCALE(y_0 - 1),
			     UWINSCALE(2*(half_pause_size+1)),
			     UWINSCALE(2*(half_pause_size+1)));
	    rd.drawString(dpy, drawPixmap, gameGC,
			  WINSCALE(X(x)) - pauseCharWidth/2,
			  WINSCALE(Y(y-1)) + gameFont->ascent/2,
			  "P", 1);
	}
    } else
	Bitmap_paint(drawPixmap, BM_PAUSED, WINSCALE(X(x - BLOCK_SZ / 2)),
		     WINSCALE(Y(y + BLOCK_SZ / 2)),
		     (count <= 0 || loopsSlow % 10 >= 5) ? 1 : 0);
}


/* Create better graphics for this. */
void Gui_paint_appearing_begin(void)
{
}


void Gui_paint_appearing(int x, int y, int id, int count)
{
    const unsigned hsize = 3 * BLOCK_SZ / 7;
    other_t *other = Other_by_id(id);
    int color = other ? Life_color(other) : 0;

    if (!color)
	color = WHITE;

    /* Make a note we are doing the base warning */
    if (version >= 0x4F12) {
	homebase_t *base = Homebase_by_id(id);
	if (base != NULL)
	    base->appeartime = (long)(loops + (count * clientFPS) / 120);
    }

    SET_FG(colors[color].pixel);
    rd.fillRectangle(dpy, drawPixmap, gameGC,
		     SCALEX(x - (int)hsize),
		     SCALEY(y - (int)hsize + (int)(count / 180. * hsize + 1)),
		     UWINSCALE(2 * hsize + 1),
		     UWINSCALE((unsigned)(count / 180. * hsize + 1)));
}


void Gui_paint_appearing_end(void)
{
}


void Gui_paint_ecm(int x, int y, int size)
{
    Arc_add(WHITE,
	    X(x - size / 2),
	    Y(y + size / 2),
	    size, size, 0, 64 * 360);
}


void Gui_paint_refuel(int x_0, int y_0, int x_1, int y_1)
{
    if (!texturedObjects) {
	rd.drawLine(dpy, drawPixmap, gameGC,
		    WINSCALE(X(x_0)), WINSCALE(Y(y_0)),
		    WINSCALE(X(x_1)), WINSCALE(Y(y_1)));
    }
    else {
	int size = WINSCALE(8);
	double dx, dy;
	int i;
	int fuel[16] = { 1, 2, 3, 3, 2, 1, 0, 1, 2, 3, 2, 1, 2, 3, 3, 2 };

	x_0 = WINSCALE(X(x_0));
	y_0 = WINSCALE(Y(y_0));
	x_1 = WINSCALE(X(x_1));
	y_1 = WINSCALE(Y(y_1));
	dx = (double)(x_1 - x_0) / 16;
	dy = (double)(y_1 - y_0) / 16;
	for (i = 0; i < 16; i++) {
	    Bitmap_paint(drawPixmap, BM_REFUEL,
			 (int)(x_0 + (dx * i) - size / 2),
			 (int)(y_0 + (dy * i) - size / 2),
			 fuel[(loops + 16 - i) % 16]);
	}
    }
}


void Gui_paint_connector(int x_0, int y_0, int x_1, int y_1, int tractor)
{
    if (tractor)
	rd.setDashes(dpy, gameGC, 0, cdashes, NUM_CDASHES);
    else
	rd.setDashes(dpy, gameGC, 0, dashes, NUM_DASHES);

    rd.drawLine(dpy, drawPixmap, gameGC,
		WINSCALE(X(x_0)), WINSCALE(Y(y_0)),
		WINSCALE(X(x_1)), WINSCALE(Y(y_1)));
    if (tractor)
	rd.setDashes(dpy, gameGC, 0, dashes, NUM_DASHES);
}


void Gui_paint_transporter(int x_0, int y_0, int x_1, int y_1)
{
    rd.drawLine(dpy, drawPixmap, gameGC,
		WINSCALE(X(x_0)), WINSCALE(Y(y_0)),
		WINSCALE(X(x_1)), WINSCALE(Y(y_1)));
}


void Gui_paint_all_connectors_begin(void)
{
    unsigned long	mask;

    SET_FG(colors[connColor].pixel);
    if (gcv.line_style != LineOnOffDash) {
	gcv.line_style = LineOnOffDash;
	mask = GCLineStyle;
#ifndef NO_ROTATING_DASHES
	mask |= GCDashOffset;
#endif
	XChangeGC(dpy, gameGC, mask, &gcv);
    }

}


void Gui_paint_ships_begin(void)
{
    gcv.dash_offset = WINSCALE(DASHES_LENGTH - (loops % DASHES_LENGTH));
}


void Gui_paint_ships_end(void)
{
   unsigned long	mask;
   if (gcv.line_style != LineSolid) {
	gcv.line_style = LineSolid;
	mask = GCLineStyle;
	XChangeGC(dpy, gameGC, mask, &gcv);
    }
    gcv.dash_offset = 0;
}


static void Gui_paint_rounddelay(int x, int y)
{
    char s[12];
    int	 t, text_width;

    sprintf(s, "%d", roundDelay / FPS);
    t = strlen(s);
    SET_FG(colors[WHITE].pixel);
    text_width = XTextWidth(gameFont, s, t);
    rd.drawString(dpy, drawPixmap, gameGC,
		  WINSCALE(X(x)) - text_width / 2,
		  WINSCALE(Y(y)) + gameFont->ascent/2,
		  s, t);
}


/*  Here starts the paint functions for ships  (MM) */
static void Gui_paint_ship_name(int x, int y, other_t *other)
{
    Check_name_string(other);
    if (shipNameColor) {
	int color = Life_color(other);
	if (!color)
	    color = shipNameColor;

	SET_FG(colors[color].pixel);
	rd.drawString(dpy, drawPixmap, gameGC,
		      WINSCALE(X(x)) - other->name_width / 2,
		      WINSCALE(Y(y) + 16) + gameFont->ascent,
		      other->id_string, other->name_len);
    } else
	SET_FG(colors[BLUE].pixel);

    if (instruments.showLivesByShip
	&& BIT(Setup->mode, LIMITED_LIVES)) {
	char keff[16] = "";

	sprintf(keff, "%03d", other->life);
	rd.drawString(dpy, drawPixmap, gameGC,
		      WINSCALE(X(x) + SHIP_SZ),
		      WINSCALE(Y(y) - SHIP_SZ) + gameFont->ascent,
		      &keff[2], 1);
    }
}


static int Gui_is_my_tank(other_t *other)
{
    char	tank_name[MAX_NAME_LEN];

    if (self == NULL
	|| other == NULL
	|| other->mychar != 'T'
	|| (BIT(Setup->mode, TEAM_PLAY)
	&& self->team != other->team)) {
	    return 0;
    }

    if (strlcpy(tank_name, self->nick_name, MAX_NAME_LEN) < MAX_NAME_LEN)
	strlcat(tank_name, "'s tank", MAX_NAME_LEN);

    if (strcmp(tank_name, other->nick_name))
	return 0;

    return 1;
}

static int Gui_calculate_ship_color(int id, other_t *other)
{
    int ship_color = WHITE;

    if (BIT(Setup->mode, TEAM_PLAY)
	&& eyesId != id
	&& other != NULL
	&& eyeTeam == other->team) {
	/* Paint teammates and allies ships with last life in teamLWColor */
	if (BIT(Setup->mode, LIMITED_LIVES)
	    && (other->life == 0))
	    ship_color = teamLWColor;
	else
	    ship_color = teamShipColor;
    }

    if (eyes != NULL
	&& eyesId != id
	&& other != NULL
	&& eyes->alliance != ' '
	&& eyes->alliance == other->alliance) {
	/* Paint teammates and allies ships with last life in teamLWColor */
	if (BIT(Setup->mode, LIMITED_LIVES)
	    && (other->life == 0))
	    ship_color = teamLWColor;
	else
	    ship_color = teamShipColor;
    }

    if (Gui_is_my_tank(other))
	ship_color = BLUE;

    if (roundDelay > 0 && ship_color == WHITE)
	ship_color = RED;

    /* Check for team color */
    if (other && BIT(Setup->mode, TEAM_PLAY)) {
	int team_color = Team_color(other->team);
	if (team_color)
	    return team_color;
    }

    /* Vato color hack start, edited by mara & kps */
    if (BIT(Setup->mode, LIMITED_LIVES)) {
	/* Paint your ship in selfLWColor when on last life */
	if (eyes != NULL
	    && eyes->id == id
	    && eyes->life == 0) {
	    ship_color = selfLWColor;
	}

	/* Paint enemy ships with last life in enemyLWColor */
	if (eyes != NULL
	    && eyes->id != id
	    && other != NULL
	    && eyeTeam != other->team
	    && other->life == 0) {
	    ship_color = enemyLWColor;
	}
    }
    /* Vato color hack end */

    return ship_color;
}


static void Gui_paint_marking_lights(int id, int x, int y,
				     shipshape_t *ship, int dir)
{
    int lcnt;

    if (((loopsSlow + id) & 0xF) == 0) {
	for (lcnt = 0; lcnt < ship->num_l_light; lcnt++) {
	    position_t l_light = Ship_get_l_light_position(ship, lcnt, dir);
	    Rectangle_add(RED,
			  X(x + l_light.x) - 2,
			  Y(y + l_light.y) - 2,
			  6, 6);
	    Segment_add(RED,
			X(x + l_light.x)-8,
			Y(y + l_light.y),
			X(x + l_light.x)+8,
			Y(y + l_light.y));
	    Segment_add(RED,
			X(x + l_light.x),
			Y(y + l_light.y)-8,
			X(x + l_light.x),
			Y(y + l_light.y)+8);
	}
    } else if (((loopsSlow + id) & 0xF) == 2) {
	for (lcnt = 0; lcnt < ship->num_r_light; lcnt++) {
	    int rightLightColor = maxColors > 4 ? 4 : BLUE;
	    position_t r_light = Ship_get_r_light_position(ship, lcnt, dir);
	    Rectangle_add(rightLightColor,
			  X(x + r_light.x)-2,
			  Y(y + r_light.y)-2,
			  6, 6);
	    Segment_add(rightLightColor,
			X(x + r_light.x)-8,
			Y(y + r_light.y),
			X(x + r_light.x)+8,
			Y(y + r_light.y));
	    Segment_add(rightLightColor,
			X(x + r_light.x),
			Y(y + r_light.y)-8,
			X(x + r_light.x),
			Y(y + r_light.y)+8);
	}
    }
}


static void Gui_paint_shields_deflectors(int x, int y, int radius, int shield,
					 int deflector, int eshield,
					 int ship_color)
{
    int		e_radius = radius + 4;
    int		half_radius = radius >> 1;
    int		half_e_radius = e_radius >> 1;
    int		scolor = -1;
    int		ecolor = -1;

    if (shield)
	scolor = ship_color;
    if (deflector)
	ecolor = loopsSlow & 0x02 ? RED : BLUE;
    if (eshield && shield) {
	if (ecolor != -1) {
	    scolor = ecolor;
	    ecolor = ship_color;
	} else
	    scolor = ecolor = ship_color;
    }

    if (ecolor != -1) {		/* outer shield */
	SET_FG(colors[ecolor].pixel);
	rd.drawArc(dpy, drawPixmap, gameGC,
		   WINSCALE(X(x - half_e_radius)),
		   WINSCALE(Y(y + half_e_radius)),
		   (unsigned)WINSCALE(e_radius),
		   (unsigned)WINSCALE(e_radius),
		   0, 64 * 360);
    }
    if (scolor != -1) {
	SET_FG(colors[scolor].pixel);
	rd.drawArc(dpy, drawPixmap, gameGC,
		   WINSCALE(X(x - half_radius)),
		   WINSCALE(Y(y + half_radius)),
		   (unsigned)WINSCALE(radius),
		   (unsigned)WINSCALE(radius),
		   0, 64 * 360);
    }
}

static void Set_drawstyle_dashed(int ship_color);

static void Gui_paint_ship_cloaked(int ship_color, XPoint *points,
				   int point_count)
{
    Set_drawstyle_dashed(ship_color);
    rd.drawLines(dpy, drawPixmap, gameGC, points, point_count, 0);
}

static void Gui_paint_ship_phased(int ship_color, XPoint *points,
				  int point_count)
{
    Gui_paint_ship_cloaked(ship_color, points, point_count);
}

static void generic_paint_ship(int x, int y, int ang, int ship)
{
    Bitmap_paint(drawPixmap, ship,
		 WINSCALE(X(x) - 16), WINSCALE(Y(y) - 16), ang);
}


static void Gui_paint_ship_uncloaked(int id, XPoint *points,
				     int ship_color, int point_count)
{
    if (gcv.line_style != LineSolid) {
	gcv.line_style = LineSolid;
	XChangeGC(dpy, gameGC, GCLineStyle, &gcv);
    }
    SET_FG(colors[ship_color].pixel);
    rd.drawLines(dpy, drawPixmap, gameGC, points, point_count, 0);

    if (lock_id == id && id != -1 && lock_dist != 0)
	rd.fillPolygon(dpy, drawPixmap, gameGC,
		       points, point_count,
		       Complex, CoordModeOrigin);
}


static void Set_drawstyle_dashed(int ship_color)
{
    unsigned long mask;
    if (gcv.line_style != LineOnOffDash) {
	gcv.line_style = LineOnOffDash;
	mask = GCLineStyle;
#ifndef NO_ROTATING_DASHES
	mask |= GCDashOffset;
#endif
	XChangeGC(dpy, gameGC, mask, &gcv);
    }
    SET_FG(colors[ship_color].pixel);
}

static int set_shipshape(int world_x, int world_y,
			 int dir, shipshape_t *ship, XPoint *points)
{
    int cnt;
    position_t ship_point_pos;
    XPoint *xpts = points;
    double x, y;

    for (cnt = 0; cnt < ship->num_points; cnt++) {
	ship_point_pos = Ship_get_point_position(ship, cnt, dir);
	x = (world_x - world.x + ship_point_pos.x) / clData.scaleFactor;
	y = (world.y + ext_view_height - world_y - ship_point_pos.y)
		/ clData.scaleFactor;
	xpts->x = (short)rint(x);
	xpts->y = (short)rint(y);
	xpts++;
    }
    points[cnt++] = points[0];

    return cnt;
}


void Gui_paint_ship(int x, int y, int dir, int id, int cloak, int phased,
		    int shield, int deflector, int eshield)
{
    int			cnt, ship_color;
    other_t		*other;
    shipshape_t		*ship;
    XPoint		points[64];
    int			ship_shape;

    ship = Ship_by_id(id);
    other = Other_by_id(id);
    ship_color = WHITE;

    /* mara attempts similar behaviour to the kth ss hack */
    if ((!instruments.showShipShapes)
	&& (self != NULL)
	&& (self->id != id))
	cnt = set_shipshape(x, y, dir, Default_ship(), points);
    else if ((!instruments.showMyShipShape)
	       && (self != NULL)
	       && (self->id == id))
	cnt = set_shipshape(x, y, dir, Default_ship(), points);
    else
	cnt = set_shipshape(x, y, dir, ship, points);

    /*
     * Determine if the name of the player should be drawn below
     * his/her ship.
     */
    if (self != NULL
	&& self->id != id
	&& other != NULL)
	Gui_paint_ship_name(x, y, other);

    if (roundDelay > 0 && roundDelay % FPS < FPS/2) {
	Gui_paint_rounddelay(x, y);
	return;
    }

    if (!(ship_color = Gui_calculate_ship_color(id, other))) return;

    if (cloak == 0 && phased == 0) {
	if (!texturedObjects || !texturedShips) {
	    Gui_paint_ship_uncloaked(id, points, ship_color, cnt);
	    /* shipshapeshack by Mara */
	    if (instruments.showShipShapesHack) {
		Segment_add(ship_color,
			    (X(x + SHIP_SZ * tcos(dir))),
			    (Y(y + SHIP_SZ * tsin(dir))),
			    (X(x + (SHIP_SZ + 12) * tcos(dir))),
			    (Y(y + (SHIP_SZ + 12) * tsin(dir))));
		Arc_add(ship_color,
			X(x - SHIP_SZ), Y(y + SHIP_SZ),
			2 * SHIP_SZ, 2 * SHIP_SZ,
			0, 64 * 360);
	    }
	} else {
	    if (ship_color == BLUE)
		ship_shape = BM_SHIP_FRIEND;
	    else if (self != NULL && self->id != id)
		ship_shape = BM_SHIP_ENEMY;
	    else
		ship_shape = BM_SHIP_SELF;

	    generic_paint_ship(x, y, dir, ship_shape);
	}

    }

    if (phased)
	Gui_paint_ship_phased(ship_color, points, cnt);
    else if (cloak)
	Gui_paint_ship_cloaked(ship_color, points, cnt);

    if (markingLights)
        Gui_paint_marking_lights(id, x, y, ship, dir);

    if (shield || deflector) {
	Set_drawstyle_dashed(ship_color);
	Gui_paint_shields_deflectors(x, y, ship->shield_radius,
				     shield, deflector,
				     eshield, ship_color);
    }
}


int Team_color(int team)
{
    /* This code assumes we have max 10 teams. */
    assert(MAX_TEAMS == 10);
    switch (team) {
    case 0:	return team0Color;
    case 1:	return team1Color;
    case 2:	return team2Color;
    case 3:	return team3Color;
    case 4:	return team4Color;
    case 5:	return team5Color;
    case 6:	return team6Color;
    case 7:	return team7Color;
    case 8:	return team8Color;
    case 9:	return team9Color;
    default:    break;
    }
    return 0;
}

int Life_color(other_t *other)
{
    int color = 0; /* default is 'no special color' */

    if (other
	&& (other->mychar == ' ' || other->mychar == 'R')
	&& BIT(Setup->mode, LIMITED_LIVES))
	color = Life_color_by_life(other->life);
    return color;
}

int Life_color_by_life(int life)
{
    int color;
	
    if (life > 2)
	color = manyLivesColor;
    else if (life == 2)
	color = twoLivesColor;
    else if (life == 1)
	color = oneLifeColor;
    else /* we catch all */
	color = zeroLivesColor;
    return color;
}



static xp_option_t guiobject_options[] = {
    COLOR_INDEX_OPTION(
	"teamShotColor",
	2,
	&teamShotColor,
	"Which color number to use for drawing harmless shots.\n"),

    COLOR_INDEX_OPTION(
	"ballColor",
	1,
	&ballColor,
	"Which color number to use for drawing balls.\n"),

    COLOR_INDEX_OPTION(
	"connColor",
	2,
	&connColor,
	"Which color number to use for drawing connectors.\n"),

    COLOR_INDEX_OPTION(
	"zeroLivesColor",
	5,
	&zeroLivesColor,
	"Which color to associate with ships with zero lives left.\n"
	"This can be used to paint for example ship and base names.\n"),

    COLOR_INDEX_OPTION(
	"oneLifeColor",
	11,
	&oneLifeColor,
	"Which color to associate with ships with one life left.\n"
	"This can be used to paint for example ship and base names.\n"),

    COLOR_INDEX_OPTION(
	"twoLivesColor",
	4,
	&twoLivesColor,
	"Which color to associate with ships with two lives left.\n"
	"This can be used to paint for example ship and base names.\n"),

    COLOR_INDEX_OPTION(
	"manyLivesColor",
	0,
	&manyLivesColor,
	"Which color to associate with ships with more than two lives left.\n"
	"This can be used to paint for example ship and base names.\n"),

    COLOR_INDEX_OPTION(
	"selfLWColor",
	3,
	&selfLWColor,
	"Which color to use to paint your ship in when on last life.\n"
	"Original color for this is red.\n"),

    COLOR_INDEX_OPTION(
	"enemyLWColor",
	3,
	&enemyLWColor,
	"Which color to use to paint enemy ships in when on last life.\n"
	"Original color for this is red.\n"),

    COLOR_INDEX_OPTION(
	"teamLWColor",
	2,
	&teamLWColor,
	"Which color to use to paint teammate ships in when on last life.\n"
	"Original color for this is green.\n"),

    COLOR_INDEX_OPTION(
	"shipNameColor",
	2,
	&shipNameColor,
	"Which color number to use for drawing names of ships\n"
	"(unless drawn in one of the life colors).\n"),

    COLOR_INDEX_OPTION(
	"mineNameColor",
	2,
	&mineNameColor,
	"Which color number to use for drawing names of mines.\n"),

    COLOR_INDEX_OPTION(
	"teamShipColor",
	2,
	&teamShipColor,
	"Which color number to use for drawing your teammates.\n"),

    COLOR_INDEX_OPTION(
	"team0Color",
	0,
	&team0Color,
	"Which color number to use for drawing team 0 objects.\n"),

    COLOR_INDEX_OPTION(
	"team1Color",
	0,
	&team1Color,
	"Which color number to use for drawing team 1 objects.\n"),

    COLOR_INDEX_OPTION(
	"team2Color",
	0,
	&team2Color,
	"Which color number to use for drawing team 2 objects.\n"),

    COLOR_INDEX_OPTION(
	"team3Color",
	0,
	&team3Color,
	"Which color number to use for drawing team 3 objects.\n"),

    COLOR_INDEX_OPTION(
	"team4Color",
	0,
	&team4Color,
	"Which color number to use for drawing team 4 objects.\n"),

    COLOR_INDEX_OPTION(
	"team5Color",
	0,
	&team5Color,
	"Which color number to use for drawing team 5 objects.\n"),

    COLOR_INDEX_OPTION(
	"team6Color",
	0,
	&team6Color,
	"Which color number to use for drawing team 6 objects.\n"),

    COLOR_INDEX_OPTION(
	"team7Color",
	0,
	&team7Color,
	"Which color number to use for drawing team 7 objects.\n"),

    COLOR_INDEX_OPTION(
	"team8Color",
	0,
	&team8Color,
	"Which color number to use for drawing team 8 objects.\n"),

    COLOR_INDEX_OPTION(
	"team9Color",
	0,
	&team9Color,
	"Which color number to use for drawing team 9 objects.\n"),
};

void Store_guiobject_options(void)
{
    STORE_OPTIONS(guiobject_options);
}
