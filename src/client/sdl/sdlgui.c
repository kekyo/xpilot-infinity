/*
 * XPilot Infinity/SDL, an SDL/OpenGL XPilot client.
 *
 * Copyright (C) 2003-2004 by 
 *
 *      Juha Lindström       <juhal@users.sourceforge.net>
 *      Erik Andersson       <deity_at_home.se>
 *
 * Copyright (C) 1991-2002 by
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

#include "xpclient_sdl.h"

#include "sdlpaint.h"
#include "sdlinit.h"
#include "images.h"
#include "glwidgets.h"
#include "text.h"
#include "asteroid_data.h"
#include "polygon_geometry.h"

#ifdef XPILOT_APPEARING_BATCH_TEST_HOOKS
#include "appearing_batch_test_support.h"
#endif
#ifdef XPILOT_SPARK_BATCH_TEST_HOOKS
#include "spark_batch_test_support.h"
#endif
#ifdef XPILOT_ASTEROID_BATCH_TEST_HOOKS
#include "asteroid_batch_test_support.h"
#endif
#ifdef XPILOT_POLYGON_CACHE_TEST_HOOKS
#include "polygon_cache_test_support.h"
#endif

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

Uint32 nullRGBA     = 0x00000000;
Uint32 blackRGBA    = 0x000000ff;
Uint32 whiteRGBA    = 0xffffffff;
Uint32 blueRGBA     = 0x0000ffff;
Uint32 redRGBA	    = 0xff0000ff;
Uint32 greenRGBA    = 0x00ff00ff;
Uint32 yellowRGBA   = 0xffff00ff;

Uint32 wallColorRGBA;
Uint32 decorColorRGBA;
Uint32 hudColorRGBA;
Uint32 connColorRGBA;
Uint32 scoreObjectColorRGBA;
Uint32 fuelColorRGBA;
Uint32 messagesColorRGBA;
Uint32 oldmessagesColorRGBA;
Uint32 msgScanBallColorRGBA;
Uint32 msgScanSafeColorRGBA;
Uint32 msgScanCoverColorRGBA;
Uint32 msgScanPopColorRGBA;
Uint32 ballColorRGBA;

Uint32 meterBorderColorRGBA;
Uint32 fuelMeterColorRGBA;
Uint32 powerMeterColorRGBA;
Uint32 turnSpeedMeterColorRGBA;
Uint32 packetSizeMeterColorRGBA;
Uint32 packetLossMeterColorRGBA;
Uint32 packetDropMeterColorRGBA;
Uint32 packetLagMeterColorRGBA;
Uint32 temporaryMeterColorRGBA;

Uint32 dirPtrColorRGBA;
Uint32 hudHLineColorRGBA;
Uint32 hudVLineColorRGBA;
Uint32 hudItemsColorRGBA;
Uint32 fuelGaugeColorRGBA;
Uint32 selfLWColorRGBA;
Uint32 teamLWColorRGBA;
Uint32 enemyLWColorRGBA;
Uint32 teamShipColorRGBA;
Uint32 team0ColorRGBA;
Uint32 team1ColorRGBA;
Uint32 team2ColorRGBA;
Uint32 team3ColorRGBA;
Uint32 team4ColorRGBA;
Uint32 team5ColorRGBA;
Uint32 team6ColorRGBA;
Uint32 team7ColorRGBA;
Uint32 team8ColorRGBA;
Uint32 team9ColorRGBA;
Uint32 shipNameColorRGBA;
Uint32 baseNameColorRGBA;
Uint32 manyLivesColorRGBA;
Uint32 twoLivesColorRGBA;
Uint32 oneLifeColorRGBA;
Uint32 zeroLivesColorRGBA;

Uint32 hudRadarEnemyColorRGBA;
Uint32 hudRadarOtherColorRGBA;
Uint32 hudRadarObjectColorRGBA;

Uint32 scoreInactiveSelfColorRGBA;
Uint32 scoreInactiveColorRGBA;
Uint32 scoreSelfColorRGBA;
Uint32 scoreColorRGBA;
Uint32 scoreOwnTeamColorRGBA;
Uint32 scoreEnemyTeamColorRGBA;

Uint32 selectionColorRGBA;

static int meterWidth	= 60;
static int meterHeight	= 10;

float hudRadarMapScale;
int   hudRadarEnemyShape;
int   hudRadarOtherShape;
int   hudRadarObjectShape;
float hudRadarDotScale;

static double shipLineWidth;
static bool texturedBalls;
static bool texturedShips;

typedef struct SemanticPolygonLocalPoint {
    int64_t x;
    int64_t y;
} SemanticPolygonLocalPoint;

typedef struct SemanticPolygonCacheEntry {
    /* Geometry and edge visibility are immutable scene snapshots.  Style and
     * texture table entries are intentionally resolved for every paint. */
    PolygonGeometry fill_geometry;
    SemanticPolygonLocalPoint *local_points;
    RendererPoint2D *world_points;
    RendererVertex2D *draw_vertices;
    unsigned char *visible_edges;
    size_t point_count;
    int has_special_edges;
    int64_t anchor_x;
    int64_t anchor_y;
    int64_t min_x;
    int64_t min_y;
    int bounds_height;
} SemanticPolygonCacheEntry;

static SemanticPolygonCacheEntry *semanticPolygonCache;
static size_t semanticPolygonCacheCount;
static const xp_polygon_t *semanticPolygonCacheSource;
static int semanticPolygonCacheReady;

irec_t *select_bounds;

string_tex_t score_object_texs[MAX_SCORE_OBJECTS];
string_tex_t meter_texs[MAX_METERS];
string_tex_t HUD_texs[MAX_HUD_TEXS+MAX_SCORE_OBJECTS];

int Gui_init(void);
void Gui_cleanup(void);
static void Discard_semantic_asteroid_batch(void);
static RendererStatus Prepare_semantic_polygon_cache(void);
static void Discard_semantic_polygon_cache(void);

static bool Ensure_cached_text(font_data *font, const char *text,
			       string_tex_t *cache)
{
    if (font == NULL || text == NULL || cache == NULL)
	return false;
    if (cache->cache != NULL && cache->text != NULL
	&& strcmp(cache->text, text) == 0) {
	return true;
    }
    return render_text(font, text, cache);
}

static Uint8 get_alpha(Uint32 color)
{
    return (color & 255);
}

static int wrap(int *xp, int *yp)
{
    int			x = *xp, y = *yp;
    int returnval =1;

    if (x < world.x || x > world.x + ext_view_width) {
	if (x < realWorld.x || x > realWorld.x + ext_view_width)
	    returnval = 0;
	*xp += world.x - realWorld.x;
    }
    if (y < world.y || y > world.y + ext_view_height) {
	if (y < realWorld.y || y > realWorld.y + ext_view_height)
	    returnval = 0;
	*yp += world.y - realWorld.y;
    }
    return returnval;
}

int Gui_init(void)
{
    RendererStatus status = Prepare_semantic_polygon_cache();

    if (status == RENDERER_STATUS_OK)
	return 0;
    error("failed to prepare polygon geometry cache");
    return -1;
}

void Gui_cleanup(void)
{
    Discard_semantic_asteroid_batch();
    Discard_semantic_polygon_cache();
}

#ifdef XPILOT_POLYGON_CACHE_TEST_HOOKS
RendererStatus Sdlgui_test_prepare_polygon_cache(void)
{
    return Prepare_semantic_polygon_cache();
}

void Sdlgui_test_discard_polygon_cache(void)
{
    Discard_semantic_polygon_cache();
}
#endif

/* Map painting */

void Gui_paint_cannon(int x, int y, int type)
{
    switch (type) {
    case SETUP_CANNON_UP:
        Image_paint(IMG_CANNON_DOWN, x, y, 0, whiteRGBA);
        break;
    case SETUP_CANNON_DOWN:
        Image_paint(IMG_CANNON_UP, x, y + 1, 0, whiteRGBA);
        break;
    case SETUP_CANNON_LEFT:
        Image_paint(IMG_CANNON_RIGHT, x, y, 0, whiteRGBA);
        break;
    case SETUP_CANNON_RIGHT:
        Image_paint(IMG_CANNON_LEFT, x - 1, y, 0, whiteRGBA);
        break;
    default:
        errno = 0;
        error("Bad cannon dir.");
        return;
    }
}

void Gui_paint_fuel(int x, int y, double fuel)
{
#define FUEL_BORDER 3

    int size, frame;
    irec_t area;
    image_t *img;

    img = Image_get(IMG_FUEL);
    if (img == NULL) return;

    /* x + x * y will give a pseudo random number,
     * so different fuelcells will not be displayed with the same
     * image-frame. */
    frame = ABS(loopsSlow + x + x * y) % (img->num_frames * 2);

    /* the animation is played from image 0-15 then back again
     * from image 15-0 */
    if (frame >= img->num_frames)
	frame = (2 * img->num_frames - 1) - frame;

    size = (int)((BLOCK_SZ - 2 * FUEL_BORDER) * fuel / MAX_STATION_FUEL);

    Image_paint(IMG_FUELCELL, x, y, 0, fuelColorRGBA);

    /* A station with no visible fill has no fuel sprite to draw. */
    if (size <= 0)
	return;

    area.x = 0;
    area.y = (int)((BLOCK_SZ - 2 * FUEL_BORDER)
		   * (1 - fuel / MAX_STATION_FUEL));
    area.w = BLOCK_SZ - 2 * FUEL_BORDER;
    area.h = size;
    Image_paint_area(IMG_FUEL,
		     x + FUEL_BORDER,
		     y + FUEL_BORDER,
		     frame,
		     &area,
		     fuelColorRGBA);
}

void Gui_paint_base(int x, int y, int id, int team, int type)
{
    Uint32 color;
    homebase_t *base = NULL;
    other_t *other;
    bool do_basewarning = false;

    switch (type) {
    case SETUP_BASE_UP:
        Image_paint(IMG_BASE_DOWN, x, y, 0, whiteRGBA);
        break;
    case SETUP_BASE_DOWN:
        Image_paint(IMG_BASE_UP, x, y + 1, 0, whiteRGBA);
        break;
    case SETUP_BASE_LEFT:
        Image_paint(IMG_BASE_RIGHT, x, y, 0, whiteRGBA);
        break;
    case SETUP_BASE_RIGHT:
        Image_paint(IMG_BASE_LEFT, x - 1, y, 0, whiteRGBA);
        break;
    default:
        errno = 0;
        error("Bad base dir.");
        return;
    }

    if (!(other = Other_by_id(id))) return;

    if (baseNameColorRGBA) {
	if (!(color = Life_color(other)))
	    color = baseNameColorRGBA;
    } else
	color = whiteRGBA;

    x = x + BLOCK_SZ / 2;
    y = y + BLOCK_SZ / 2;

    base = Homebase_by_id(id);
    if (base != NULL) {
	/*
	 * Hacks to support Mara's base warning on new servers and
	 * the red "meter" basewarning on old servers.
	 */
	if (loops < base->appeartime)
	    do_basewarning = true;

	if (version < 0x4F12 && do_basewarning) {
	    if (baseWarningType & 1) {
		/* We assume the ship will appear after 3 seconds. */
		int count = (int)(360 * (base->appeartime - loops)
				  / (3 * clientFPS));
		LIMIT(count, 0, 360);
		/* red box basewarning */
		if (count > 0 && (baseWarningType & 1))
		    Gui_paint_appearing(x, y,
					id, count);
	    }
	}
    }
    /* Mara's flashy basewarning */
    if (do_basewarning && (baseWarningType & 2)) {
	if (loopsSlow & 1) {
	    if (color != whiteRGBA)
		color = whiteRGBA;
	    else
		color = redRGBA;
	}
    }

    switch (type) {
    case SETUP_BASE_UP:
	mapnprint(&mapfont,color,CENTER,DOWN ,(x) ,(y - BLOCK_SZ / 2),maxCharsInNames,"%s",other->nick_name);
        break;
    case SETUP_BASE_DOWN:
	mapnprint(&mapfont,color,CENTER,UP   ,(x) ,(int)(y + BLOCK_SZ / 1.5),maxCharsInNames,"%s",other->nick_name);
        break;
    case SETUP_BASE_LEFT:
	mapnprint(&mapfont,color,RIGHT,UP    ,(x + BLOCK_SZ / 2) ,(y),maxCharsInNames,"%s",other->nick_name);
        break;
    case SETUP_BASE_RIGHT:
	mapnprint(&mapfont,color,LEFT,UP     ,(x - BLOCK_SZ / 2) ,(y),maxCharsInNames,"%s",other->nick_name);
        break;
    default:
        errno = 0;
        error("Bad base dir.");
    }
}

#define SEMANTIC_WORLD_LINE_MAX_PATHS 5
#define SEMANTIC_WORLD_LINE_MAX_POINTS 16
#define SEMANTIC_WORLD_DECOR_TYPE_COUNT 256

typedef struct SemanticWorldLinePath {
    RendererPoint2D points[SEMANTIC_WORLD_LINE_MAX_POINTS];
    size_t point_count;
    int closed;
} SemanticWorldLinePath;

typedef struct SemanticWorldLineGeometry {
    SemanticWorldLinePath paths[SEMANTIC_WORLD_LINE_MAX_PATHS];
    size_t path_count;
} SemanticWorldLineGeometry;

typedef struct SemanticWorldLineBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    int has_accepted_commands;
} SemanticWorldLineBatch;

static RendererStatus Begin_semantic_world_line_batch(
    SemanticWorldLineBatch *batch)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    batch->sdl_renderer = Get_sdl_renderer();
    batch->renderer = NULL;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->has_accepted_commands = 0;
    if (batch->sdl_renderer == NULL)
	return batch->status;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return batch->status;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	batch->status = Sdl_renderer_track_frame_result(
	    batch->sdl_renderer, RENDERER_STATUS_INVALID_STATE);
    }
    return batch->status;
}

static RendererStatus Track_semantic_world_line_batch(
    SemanticWorldLineBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static RendererStatus Accept_semantic_world_line_command(
    SemanticWorldLineBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (status == RENDERER_STATUS_OK)
	batch->has_accepted_commands = 1;
    return Track_semantic_world_line_batch(batch, status);
}

static RendererStatus Finish_semantic_world_line_batch(
    SemanticWorldLineBatch *batch)
{
    RendererStatus flush_status;

    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!batch->has_accepted_commands || batch->sdl_renderer == NULL)
	return batch->status;
    flush_status = Sdl_renderer_flush(
	batch->sdl_renderer);
    batch->has_accepted_commands = 0;
    return Track_semantic_world_line_batch(batch, flush_status);
}

static RendererStatus Set_semantic_world_line_point(
    RendererPoint2D *point, int64_t x, int64_t y)
{
    if (point == NULL
	|| x < INT_MIN || x > INT_MAX
	|| y < INT_MIN || y > INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    point->x = (float)x;
    point->y = (float)y;
    if (!isfinite(point->x) || !isfinite(point->y))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    return RENDERER_STATUS_OK;
}

static int Semantic_world_line_extent_preserved(
    int64_t first, int64_t second, float first_float, float second_float)
{
    /* Equal integer coordinates are valid; only conversion collapse is bad. */
    return first == second || first_float != second_float;
}

static int Semantic_polygon_size_fits(size_t count, size_t item_size)
{
    return item_size == 0
	|| (count <= SIZE_MAX / item_size
	    && count <= (size_t)PTRDIFF_MAX / item_size);
}

static int Semantic_polygon_add_int64(
    int64_t first, int64_t second, int64_t *result)
{
    if (result == NULL
	|| (second > 0 && first > INT64_MAX - second)
	|| (second < 0 && first < INT64_MIN - second)) {
	return 0;
    }
    *result = first + second;
    return 1;
}

static void Cleanup_semantic_polygon_cache_entries(
    SemanticPolygonCacheEntry *entries, size_t entry_count)
{
    size_t index;

    if (entries == NULL)
	return;
    for (index = 0; index < entry_count; index++) {
	Polygon_geometry_cleanup(&entries[index].fill_geometry);
	free(entries[index].local_points);
	free(entries[index].world_points);
	free(entries[index].draw_vertices);
	free(entries[index].visible_edges);
    }
    free(entries);
}

static void Discard_semantic_polygon_cache(void)
{
    Cleanup_semantic_polygon_cache_entries(
	semanticPolygonCache, semanticPolygonCacheCount);
    semanticPolygonCache = NULL;
    semanticPolygonCacheCount = 0;
    semanticPolygonCacheSource = NULL;
    semanticPolygonCacheReady = 0;
}

static RendererStatus Set_semantic_polygon_contour_point(
    RendererPoint2D *point, int64_t origin_x, int64_t origin_y,
    SemanticPolygonLocalPoint local)
{
    int64_t x;
    int64_t y;

    if (!Semantic_polygon_add_int64(origin_x, local.x, &x)
	|| !Semantic_polygon_add_int64(origin_y, local.y, &y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return Set_semantic_world_line_point(point, x, y);
}

static RendererStatus Validate_semantic_polygon_contour(
    SemanticPolygonCacheEntry *entry, int64_t origin_x, int64_t origin_y)
{
    size_t index;

    if (entry == NULL || entry->local_points == NULL
	|| entry->world_points == NULL || entry->point_count < 3) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    for (index = 0; index < entry->point_count; index++) {
	RendererStatus status = Set_semantic_polygon_contour_point(
	    &entry->world_points[index], origin_x, origin_y,
	    entry->local_points[index]);

	if (status != RENDERER_STATUS_OK)
	    return status;
    }
    for (index = 0; index < entry->point_count; index++) {
	size_t next = (index + 1) % entry->point_count;
	SemanticPolygonLocalPoint first = entry->local_points[index];
	SemanticPolygonLocalPoint second = entry->local_points[next];
	RendererPoint2D first_world = entry->world_points[index];
	RendererPoint2D second_world = entry->world_points[next];

	if (!Semantic_world_line_extent_preserved(
		first.x, second.x, first_world.x, second_world.x)
	    || !Semantic_world_line_extent_preserved(
		first.y, second.y, first_world.y, second_world.y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_polygon_cache_entry(
    const xp_polygon_t *polygon, SemanticPolygonCacheEntry *entry)
{
    size_t source_point_count;
    size_t index;
    int64_t current_x = 0;
    int64_t current_y = 0;
    RendererStatus status;

    if (polygon == NULL || entry == NULL || polygon->points == NULL
	|| polygon->num_points < 3 || polygon->bounds.h < 0) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    source_point_count = (size_t)polygon->num_points;
    if (!Semantic_polygon_size_fits(
	    source_point_count, sizeof(*entry->local_points))
	|| !Semantic_polygon_size_fits(
	    source_point_count, sizeof(*entry->world_points))) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }

    entry->local_points = malloc(
	source_point_count * sizeof(*entry->local_points));
    if (entry->local_points == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    entry->local_points[0].x = 0;
    entry->local_points[0].y = 0;
    for (index = 1; index < source_point_count; index++) {
	if (!Semantic_polygon_add_int64(
		current_x, (int64_t)polygon->points[index].x, &current_x)
	    || !Semantic_polygon_add_int64(
		current_y, (int64_t)polygon->points[index].y, &current_y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	entry->local_points[index].x = current_x;
	entry->local_points[index].y = current_y;
    }

    entry->point_count = source_point_count;
    /* Accept an optional repeated first vertex at the end.  Keep a single
     * canonical vertex and let both tessellation and path stroking close it;
     * the last retained selector remains the outgoing close edge. */
    if (current_x == 0 && current_y == 0)
	entry->point_count--;
    if (entry->point_count < 3)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    entry->world_points = malloc(
	entry->point_count * sizeof(*entry->world_points));
    if (entry->world_points == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    entry->has_special_edges = polygon->edge_styles != NULL;
    if (entry->has_special_edges) {
	entry->visible_edges = malloc(
	    entry->point_count * sizeof(*entry->visible_edges));
	if (entry->visible_edges == NULL)
	    return RENDERER_STATUS_OUT_OF_MEMORY;
	for (index = 0; index < entry->point_count; index++) {
	    int selector = polygon->edge_styles[index];

	    if (selector < 0 || selector > 255)
		return RENDERER_STATUS_INVALID_ARGUMENT;
	    /* SDL historically used special selectors only as a visibility mask;
	     * the current polygon default still supplies color and width. */
	    entry->visible_edges[index] = selector != 0;
	}
    }

    entry->anchor_x = polygon->points[0].x;
    entry->anchor_y = polygon->points[0].y;
    entry->min_x = entry->local_points[0].x;
    entry->min_y = entry->local_points[0].y;
    entry->bounds_height = polygon->bounds.h;
    for (index = 0; index < entry->point_count; index++) {
	SemanticPolygonLocalPoint local = entry->local_points[index];

	entry->world_points[index].x = (float)local.x;
	entry->world_points[index].y = (float)local.y;
	if (!isfinite(entry->world_points[index].x)
	    || !isfinite(entry->world_points[index].y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	if (local.x < entry->min_x)
	    entry->min_x = local.x;
	if (local.y < entry->min_y)
	    entry->min_y = local.y;
    }
    status = Polygon_geometry_tessellate_odd(
	entry->world_points, entry->point_count, &entry->fill_geometry);
    if (status != RENDERER_STATUS_OK)
	return status;

    if (entry->fill_geometry.triangle_point_count != 0) {
	if (!Semantic_polygon_size_fits(
		entry->fill_geometry.triangle_point_count,
		sizeof(*entry->draw_vertices))) {
	    return RENDERER_STATUS_OUT_OF_MEMORY;
	}
	entry->draw_vertices = malloc(
	    entry->fill_geometry.triangle_point_count
		* sizeof(*entry->draw_vertices));
	if (entry->draw_vertices == NULL)
	    return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    return Validate_semantic_polygon_contour(
	entry, entry->anchor_x, entry->anchor_y);
}

static RendererStatus Prepare_semantic_polygon_cache(void)
{
    SemanticPolygonCacheEntry *candidate;
    size_t candidate_count;
    size_t index;
    RendererStatus status;

    if (semanticPolygonCacheReady || semanticPolygonCache != NULL)
	return RENDERER_STATUS_INVALID_STATE;
    if (num_polygons < 0 || (num_polygons > 0 && polygons == NULL))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    candidate_count = (size_t)num_polygons;
    if (candidate_count == 0) {
	semanticPolygonCacheSource = polygons;
	semanticPolygonCacheReady = 1;
	return RENDERER_STATUS_OK;
    }
    if (!Semantic_polygon_size_fits(candidate_count, sizeof(*candidate)))
	return RENDERER_STATUS_OUT_OF_MEMORY;

    candidate = calloc(candidate_count, sizeof(*candidate));
    if (candidate == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    for (index = 0; index < candidate_count; index++) {
	status = Build_semantic_polygon_cache_entry(
	    &polygons[index], &candidate[index]);
	if (status != RENDERER_STATUS_OK) {
	    Cleanup_semantic_polygon_cache_entries(
		candidate, candidate_count);
	    return status;
	}
    }

    semanticPolygonCache = candidate;
    semanticPolygonCacheCount = candidate_count;
    semanticPolygonCacheSource = polygons;
    semanticPolygonCacheReady = 1;
    return RENDERER_STATUS_OK;
}

static RendererStatus Append_semantic_world_line_path(
    SemanticWorldLineGeometry *geometry,
    int64_t x_0, int64_t y_0, int64_t x_1, int64_t y_1)
{
    SemanticWorldLinePath *path;
    RendererStatus status;

    if (geometry == NULL
	|| geometry->path_count >= NELEM(geometry->paths)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    path = &geometry->paths[geometry->path_count];
    status = Set_semantic_world_line_point(
	&path->points[0], x_0, y_0);
    if (status != RENDERER_STATUS_OK)
	return status;
    status = Set_semantic_world_line_point(
	&path->points[1], x_1, y_1);
    if (status != RENDERER_STATUS_OK)
	return status;
    if (!Semantic_world_line_extent_preserved(
	    x_0, x_1, path->points[0].x, path->points[1].x)
	|| !Semantic_world_line_extent_preserved(
	    y_0, y_1, path->points[0].y, path->points[1].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    path->point_count = 2;
    path->closed = 0;
    geometry->path_count++;
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_world_segment_geometry(
    int x_0, int y_0, int x_1, int y_1,
    SemanticWorldLineGeometry *geometry)
{
    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    /* A legacy zero-length line has no coverage or state worth flushing. */
    if (x_0 == x_1 && y_0 == y_1)
	return RENDERER_STATUS_OK;
    return Append_semantic_world_line_path(
	geometry, (int64_t)x_0, (int64_t)y_0,
	(int64_t)x_1, (int64_t)y_1);
}

static RendererStatus Build_semantic_world_rectangle_geometry(
    int x, int y, int xi, int yi,
    SemanticWorldLineGeometry *geometry)
{
    SemanticWorldLinePath *path;
    RendererStatus status;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    if (x == xi && y == yi)
	return RENDERER_STATUS_OK;
    path = &geometry->paths[0];
    status = Set_semantic_world_line_point(
	&path->points[0], (int64_t)x, (int64_t)y);
    if (status == RENDERER_STATUS_OK) {
	status = Set_semantic_world_line_point(
	    &path->points[1], (int64_t)x, (int64_t)yi);
    }
    if (status == RENDERER_STATUS_OK) {
	status = Set_semantic_world_line_point(
	    &path->points[2], (int64_t)xi, (int64_t)yi);
    }
    if (status == RENDERER_STATUS_OK) {
	status = Set_semantic_world_line_point(
	    &path->points[3], (int64_t)xi, (int64_t)y);
    }
    if (status != RENDERER_STATUS_OK)
	return status;
    if (!Semantic_world_line_extent_preserved(
	    (int64_t)x, (int64_t)xi,
	    path->points[0].x, path->points[2].x)
	|| !Semantic_world_line_extent_preserved(
	    (int64_t)y, (int64_t)yi,
	    path->points[0].y, path->points[2].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    path->point_count = 4;
    path->closed = 1;
    geometry->path_count = 1;
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_item_outline_geometry(
    int x, int y, SemanticWorldLineGeometry *geometry)
{
    SemanticWorldLinePath *path;
    RendererStatus status;
    int64_t center_x;
    int64_t left;
    int64_t right;
    int64_t top;
    int64_t bottom;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    center_x = (int64_t)x;
    left = center_x - INT64_C(16);
    right = center_x + INT64_C(16);
    top = (int64_t)y - INT64_C(16);
    bottom = (int64_t)y + INT64_C(16);
    path = &geometry->paths[0];
    status = Set_semantic_world_line_point(
	&path->points[0], right, bottom);
    if (status == RENDERER_STATUS_OK) {
	status = Set_semantic_world_line_point(
	    &path->points[1], center_x, top);
    }
    if (status == RENDERER_STATUS_OK) {
	status = Set_semantic_world_line_point(
	    &path->points[2], left, bottom);
    }
    if (status != RENDERER_STATUS_OK)
	return status;
    if (!Semantic_world_line_extent_preserved(
	    right, center_x, path->points[0].x, path->points[1].x)
	|| !Semantic_world_line_extent_preserved(
	    center_x, left, path->points[1].x, path->points[2].x)
	|| !Semantic_world_line_extent_preserved(
	    bottom, top, path->points[0].y, path->points[1].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    path->point_count = 3;
    path->closed = 1;
    geometry->path_count = 1;
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_ball_outline_geometry(
    int x, int y, SemanticWorldLineGeometry *geometry)
{
    SemanticWorldLinePath *path;
    float center_x;
    float center_y;
    int angle;
    int point_index;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    path = &geometry->paths[0];
    center_x = (float)x;
    center_y = (float)y;
    if (!isfinite(center_x) || !isfinite(center_y))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    angle = RES / (int)NELEM(path->points);
    for (point_index = 0;
	 point_index < (int)NELEM(path->points);
	 point_index++) {
	double cosine = tcos(point_index * angle);
	double sine = tsin(point_index * angle);
	double point_x;
	double point_y;

	if (!isfinite(cosine) || !isfinite(sine))
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	point_x = (double)x + cosine * (double)BALL_RADIUS;
	point_y = (double)y + sine * (double)BALL_RADIUS;
	if (!isfinite(point_x) || !isfinite(point_y)
	    || point_x < (double)INT_MIN || point_x > (double)INT_MAX
	    || point_y < (double)INT_MIN || point_y > (double)INT_MAX) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	path->points[point_index].x = (float)point_x;
	path->points[point_index].y = (float)point_y;
	if (!isfinite(path->points[point_index].x)
	    || !isfinite(path->points[point_index].y)
	    || (point_x != (double)x
		&& path->points[point_index].x == center_x)
	    || (point_y != (double)y
		&& path->points[point_index].y == center_y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    path->point_count = NELEM(path->points);
    path->closed = 1;
    geometry->path_count = 1;
    return RENDERER_STATUS_OK;
}

static int Floor_divide_semantic_wreck_value_by_256(int value)
{
    int quotient = value / 256;

    /* C99 division truncates toward zero; legacy GCC signed shifts floored. */
    if (value < 0 && value % 256 != 0)
	quotient--;
    return quotient;
}

static RendererStatus Scale_semantic_wreck_component(
    float component, int size, int *offset)
{
    float product;
    int truncated_product;

    if (offset == NULL || !isfinite(component))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    product = component * (float)size;
    if (!isfinite(product)
	|| (double)product < (double)INT_MIN
	|| (double)product > (double)INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    /* The cast preserves the legacy C99 truncation before division. */
    truncated_product = (int)product;
    *offset = Floor_divide_semantic_wreck_value_by_256(
	truncated_product);
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_wreck_outline_geometry(
    int x, int y, int wtype, int rot, int size,
    SemanticWorldLineGeometry *geometry)
{
    SemanticWorldLinePath *path;
    RendererPoint2D center;
    int64_t point_x_coordinates[NUM_WRECKAGE_POINTS];
    int64_t point_y_coordinates[NUM_WRECKAGE_POINTS];
    int64_t first_x = 0;
    int64_t first_y = 0;
    int has_extent = 0;
    int edge_index;
    int point_index;
    RendererStatus status;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    if (wtype < 0 || wtype >= NUM_WRECKAGE_SHAPES
	|| rot < 0 || rot >= RES
	|| size < 0 || size > 255
	|| NUM_WRECKAGE_POINTS > SEMANTIC_WORLD_LINE_MAX_POINTS) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    status = Set_semantic_world_line_point(
	&center, (int64_t)x, (int64_t)y);
    if (status != RENDERER_STATUS_OK)
	return status;

    path = &geometry->paths[0];
    for (point_index = 0;
	 point_index < NUM_WRECKAGE_POINTS;
	 point_index++) {
	position_t *rotated_points = wreckageShapes[wtype][point_index];
	int x_offset;
	int y_offset;
	int64_t point_x;
	int64_t point_y;

	if (rotated_points == NULL)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	status = Scale_semantic_wreck_component(
	    rotated_points[rot].x, size, &x_offset);
	if (status == RENDERER_STATUS_OK) {
	    status = Scale_semantic_wreck_component(
		rotated_points[rot].y, size, &y_offset);
	}
	if (status != RENDERER_STATUS_OK)
	    return status;

	point_x = (int64_t)x + (int64_t)x_offset;
	point_y = (int64_t)y + (int64_t)y_offset;
	status = Set_semantic_world_line_point(
	    &path->points[point_index], point_x, point_y);
	if (status != RENDERER_STATUS_OK)
	    return status;
	point_x_coordinates[point_index] = point_x;
	point_y_coordinates[point_index] = point_y;

	if (point_index == 0) {
	    first_x = point_x;
	    first_y = point_y;
	} else if (point_x != first_x || point_y != first_y) {
	    has_extent = 1;
	}
    }
    if (!has_extent)
	return RENDERER_STATUS_OK;

    for (edge_index = 0;
	 edge_index < NUM_WRECKAGE_POINTS;
	 edge_index++) {
	int next_index = (edge_index + 1) % NUM_WRECKAGE_POINTS;

	if (!Semantic_world_line_extent_preserved(
		x, point_x_coordinates[edge_index],
		center.x, path->points[edge_index].x)
	    || !Semantic_world_line_extent_preserved(
		y, point_y_coordinates[edge_index],
		center.y, path->points[edge_index].y)
	    || !Semantic_world_line_extent_preserved(
		point_x_coordinates[edge_index],
		point_x_coordinates[next_index],
		path->points[edge_index].x,
		path->points[next_index].x)
	    || !Semantic_world_line_extent_preserved(
		point_y_coordinates[edge_index],
		point_y_coordinates[next_index],
		path->points[edge_index].y,
		path->points[next_index].y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }

    path->point_count = NUM_WRECKAGE_POINTS;
    path->closed = 1;
    geometry->path_count = 1;
    return RENDERER_STATUS_OK;
}

static RendererStatus Semantic_world_decor_mask(int type, int *mask)
{
    if (mask == NULL || type < 0
	|| type >= SEMANTIC_WORLD_DECOR_TYPE_COUNT) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    *mask = 0;
    switch (type) {
    case SETUP_DECOR_FILLED:
	*mask = DECOR_UP | DECOR_LEFT | DECOR_DOWN | DECOR_RIGHT;
	break;
    case SETUP_DECOR_RU:
	*mask = DECOR_UP | DECOR_RIGHT | DECOR_CLOSED;
	break;
    case SETUP_DECOR_RD:
	*mask = DECOR_DOWN | DECOR_RIGHT | DECOR_OPEN | DECOR_BELOW;
	break;
    case SETUP_DECOR_LU:
	*mask = DECOR_UP | DECOR_LEFT | DECOR_OPEN;
	break;
    case SETUP_DECOR_LD:
	*mask = DECOR_LEFT | DECOR_DOWN | DECOR_CLOSED | DECOR_BELOW;
	break;
    default:
	break;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_world_decor_geometry(
    int x, int y, int type, SemanticWorldLineGeometry *geometry)
{
    int mask;
    int64_t left;
    int64_t right;
    int64_t bottom;
    int64_t top;
    RendererStatus status;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    status = Semantic_world_decor_mask(type, &mask);
    if (status != RENDERER_STATUS_OK)
	return status;
    left = (int64_t)x;
    right = left + (int64_t)BLOCK_SZ;
    bottom = (int64_t)y;
    top = bottom + (int64_t)BLOCK_SZ;

#define APPEND_DECOR_PATH(x0, y0, x1, y1) \
    do { \
	status = Append_semantic_world_line_path( \
	    geometry, (x0), (y0), (x1), (y1)); \
	if (status != RENDERER_STATUS_OK) \
	    return status; \
    } while (0)

    if (mask & DECOR_LEFT)
	APPEND_DECOR_PATH(left, bottom, left, top);
    if (mask & DECOR_DOWN)
	APPEND_DECOR_PATH(left, bottom, right, bottom);
    if (mask & DECOR_RIGHT)
	APPEND_DECOR_PATH(right, bottom, right, top);
    if (mask & DECOR_UP)
	APPEND_DECOR_PATH(left, top, right, top);
    if (mask & DECOR_OPEN) {
	APPEND_DECOR_PATH(left, bottom, right, top);
    } else if (mask & DECOR_CLOSED) {
	APPEND_DECOR_PATH(left, top, right, bottom);
    }
#undef APPEND_DECOR_PATH
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_world_wall_geometry(
    int x, int y, int type, SemanticWorldLineGeometry *geometry)
{
    int64_t left;
    int64_t right;
    int64_t bottom;
    int64_t top;
    RendererStatus status;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    geometry->path_count = 0;
    left = (int64_t)x;
    right = left + (int64_t)BLOCK_SZ;
    bottom = (int64_t)y;
    top = bottom + (int64_t)BLOCK_SZ;

#define APPEND_WALL_PATH(x0, y0, x1, y1) \
    do { \
	status = Append_semantic_world_line_path( \
	    geometry, (x0), (y0), (x1), (y1)); \
	if (status != RENDERER_STATUS_OK) \
	    return status; \
    } while (0)

    if (type & BLUE_LEFT)
	APPEND_WALL_PATH(left, bottom, left, top);
    if (type & BLUE_DOWN)
	APPEND_WALL_PATH(left, bottom, right, bottom);
    if (type & BLUE_RIGHT)
	APPEND_WALL_PATH(right, bottom, right, top);
    if (type & BLUE_UP)
	APPEND_WALL_PATH(left, top, right, top);
    if ((type & BLUE_FUEL) == BLUE_FUEL) {
	/* Fuel stations deliberately have no diagonal. */
    } else if (type & BLUE_OPEN) {
	APPEND_WALL_PATH(left, bottom, right, top);
    } else if (type & BLUE_CLOSED) {
	APPEND_WALL_PATH(left, top, right, bottom);
    }
#undef APPEND_WALL_PATH
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_world_line_geometry(
    SemanticWorldLineBatch *batch,
    const SemanticWorldLineGeometry *geometry, Uint32 packed_color,
    RendererBlendMode blend)
{
    RendererColor color;
    RendererStatus operation_status;
    size_t path_index;

    if (batch == NULL || geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->status != RENDERER_STATUS_OK)
	return batch->status;
    if (geometry->path_count == 0)
	return batch->status;

    operation_status = Renderer_set_blend(batch->renderer, blend);
    if (Track_semantic_world_line_batch(batch, operation_status)
	!= RENDERER_STATUS_OK) {
	return batch->status;
    }

    color = Renderer_color_from_rgba32(packed_color);
    for (path_index = 0;
	 batch->status == RENDERER_STATUS_OK
	     && path_index < geometry->path_count;
	 path_index++) {
	const SemanticWorldLinePath *path = &geometry->paths[path_index];

	operation_status = Renderer_stroke_path(
	    batch->renderer, path->points, path->point_count, 1.0f,
	    color, path->closed);
	(void)Accept_semantic_world_line_command(batch, operation_status);
    }
    return Finish_semantic_world_line_batch(batch);
}

static RendererStatus Paint_semantic_world_stippled_geometry(
    SemanticWorldLineBatch *batch,
    const SemanticWorldLineGeometry *geometry, Uint32 packed_color,
    unsigned int factor)
{
    const SemanticWorldLinePath *path;
    RendererStatus operation_status;

    if (batch == NULL || geometry == NULL || factor == 0
	|| geometry->path_count > 1) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (batch->status != RENDERER_STATUS_OK)
	return batch->status;
    if (geometry->path_count == 0)
	return batch->status;

    operation_status = Renderer_set_blend(
	batch->renderer, RENDERER_BLEND_ADDITIVE);
    if (Track_semantic_world_line_batch(batch, operation_status)
	!= RENDERER_STATUS_OK) {
	return batch->status;
    }

    /* Semantic line coverage intentionally does not mirror GL_LINE_SMOOTH. */
    path = &geometry->paths[0];
    operation_status = Renderer_stroke_stippled_path(
	batch->renderer, path->points, path->point_count, 1.0f,
	Renderer_color_from_rgba32(packed_color), path->closed,
	factor, UINT16_C(0xAAAA));
    (void)Accept_semantic_world_line_command(batch, operation_status);
    return Finish_semantic_world_line_batch(batch);
}

void Gui_paint_decor(int x, int y, int xi, int yi, int type,
		     bool last, bool more_y)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    (void)xi;
    (void)yi;
    (void)last;
    (void)more_y;
    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_decor_geometry(x, y, type, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, decorColorRGBA, RENDERER_BLEND_ALPHA);
}

void Gui_paint_border(int x, int y, int xi, int yi)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_segment_geometry(
	x, y, xi, yi, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, wallColorRGBA, RENDERER_BLEND_ALPHA);
}

void Gui_paint_visible_border(int x, int y, int xi, int yi)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    const SemanticWorldLinePath *path;
    RendererStatus status;
    RendererStatus blend_status;
    RendererStatus stroke_status = RENDERER_STATUS_OK;
    RendererStatus stationary_status;
    int stroke_attempted = 0;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_rectangle_geometry(
	x, y, xi, yi, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    if (geometry.path_count == 0)
	return;
    status = setupPaint_moving();
    if (status != RENDERER_STATUS_OK) {
	if (Sdl_renderer_frame_result(batch.sdl_renderer)
	    == RENDERER_STATUS_OK) {
	    (void)Track_semantic_world_line_batch(&batch, status);
	}
	return;
    }

    path = &geometry.paths[0];
    blend_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ALPHA);
    if (blend_status == RENDERER_STATUS_OK) {
	stroke_attempted = 1;
	stroke_status = Renderer_stroke_path(
	    batch.renderer, path->points, path->point_count, 1.0f,
	    Renderer_color_from_rgba32(hudColorRGBA), path->closed);
    }

    /* Restore the stationary setup before retaining any renderer failure. */
    stationary_status = setupPaint_stationary_cleanup();
    (void)Track_semantic_world_line_batch(&batch, blend_status);
    if (stroke_attempted) {
	if (stroke_status == RENDERER_STATUS_OK)
	    batch.has_accepted_commands = 1;
	(void)Track_semantic_world_line_batch(&batch, stroke_status);
    }
    if (stationary_status != RENDERER_STATUS_OK)
	(void)Track_semantic_world_line_batch(&batch, stationary_status);
    (void)Finish_semantic_world_line_batch(&batch);
}

void Gui_paint_hudradar_limit(int x, int y, int xi, int yi)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_rectangle_geometry(
	x, y, xi, yi, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, blueRGBA, RENDERER_BLEND_ALPHA);
}

void Gui_paint_setup_check(int x, int y, bool isNext)
{
    if (isNext) {
	Image_paint(IMG_CHECKPOINT, x, y, 0, whiteRGBA);
    } else {
	Image_paint(IMG_CHECKPOINT, x, y, 0, whiteRGBA - 128);
    }
}

void Gui_paint_setup_acwise_grav(int x, int y)
{
    Image_paint(IMG_ACWISEGRAV, x, y, loopsSlow % 6, whiteRGBA);
}

void Gui_paint_setup_cwise_grav(int x, int y)
{
    Image_paint(IMG_CWISEGRAV, x, y, loopsSlow % 6, whiteRGBA);
}

void Gui_paint_setup_pos_grav(int x, int y)
{
    Image_paint(IMG_PLUSGRAVITY, x, y, 0, whiteRGBA);
}

void Gui_paint_setup_neg_grav(int x, int y)
{
    Image_paint(IMG_MINUSGRAVITY, x, y, 0, whiteRGBA);
}

static void Set_semantic_map_vertex(
    RendererVertex2D *vertex, RendererPoint2D point, RendererColor color);

typedef struct SemanticDirectionalGravityGeometry {
    RendererVertex2D vertices[18];
} SemanticDirectionalGravityGeometry;

static RendererStatus Build_semantic_directional_gravity_geometry(
    int x, int y, int dir, SemanticDirectionalGravityGeometry *geometry)
{
    static const unsigned char section_indices[18] = {
	0, 0, 1, 0, 1, 1,
	1, 1, 2, 1, 2, 2,
	2, 2, 3, 2, 3, 3
    };
    static const unsigned char side_indices[18] = {
	0, 1, 1, 0, 1, 0,
	0, 1, 1, 0, 1, 0,
	0, 1, 1, 0, 1, 0
    };
    const int size = BLOCK_SZ;
    long phase;
    int first_phase;
    int second_phase;
    int swap;
    int64_t section_coordinates[4][2][2];
    RendererPoint2D section_points[4][2];
    uint32_t base_color;
    uint32_t section_colors[4];
    size_t section;
    size_t side;
    size_t vertex_index;
    RendererStatus status;

    if (geometry == NULL || dir < 0 || dir > 3 || size <= 0)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    phase = loops % (long)size;
    if (phase < 0)
	phase += size;
    first_phase = (int)phase;
    second_phase = first_phase + size / 2;
    if (second_phase >= size)
	second_phase -= size;

    base_color = (uint32_t)redRGBA & UINT32_C(0xffffff00);
    if (first_phase < second_phase) {
	section_colors[0] = base_color
	    | (uint32_t)(first_phase * 128 / (size / 2));
	section_colors[1] = base_color;
	section_colors[2] = base_color | UINT32_C(128);
    } else {
	section_colors[0] = base_color
	    | (uint32_t)((size - first_phase) * 128 / (size / 2));
	section_colors[1] = base_color | UINT32_C(128);
	section_colors[2] = base_color;
	swap = first_phase;
	first_phase = second_phase;
	second_phase = swap;
    }
    section_colors[3] = section_colors[0];

    for (section = 0; section < 4; section++) {
	int offset = section == 0 ? 0
	    : section == 1 ? first_phase
	    : section == 2 ? second_phase : size;
	int64_t low_x = (int64_t)x;
	int64_t low_y = (int64_t)y;
	int64_t high_x = low_x + (int64_t)size;
	int64_t high_y = low_y + (int64_t)size;

	switch (dir) {
	case 0: /* up */
	    low_y += offset;
	    high_y = low_y;
	    break;
	case 1: /* right */
	    low_x += offset;
	    high_x = low_x;
	    break;
	case 2: /* down */
	    high_y -= offset;
	    low_y = high_y;
	    break;
	case 3: /* left */
	    high_x -= offset;
	    low_x = high_x;
	    break;
	}
	section_coordinates[section][0][0] = low_x;
	section_coordinates[section][0][1] = low_y;
	section_coordinates[section][1][0] = high_x;
	section_coordinates[section][1][1] = high_y;
    }

    for (section = 0; section < 4; section++) {
	for (side = 0; side < 2; side++) {
	    status = Set_semantic_world_line_point(
		&section_points[section][side],
		section_coordinates[section][side][0],
		section_coordinates[section][side][1]);
	    if (status != RENDERER_STATUS_OK)
		return status;
	}
	if (!Semantic_world_line_extent_preserved(
		section_coordinates[section][0][0],
		section_coordinates[section][1][0],
		section_points[section][0].x,
		section_points[section][1].x)
	    || !Semantic_world_line_extent_preserved(
		section_coordinates[section][0][1],
		section_coordinates[section][1][1],
		section_points[section][0].y,
		section_points[section][1].y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    for (section = 0; section < 3; section++) {
	for (side = 0; side < 2; side++) {
	    /* Equal animation sections are intentional at phase boundaries. */
	    if (!Semantic_world_line_extent_preserved(
		    section_coordinates[section][side][0],
		    section_coordinates[section + 1][side][0],
		    section_points[section][side].x,
		    section_points[section + 1][side].x)
		|| !Semantic_world_line_extent_preserved(
		    section_coordinates[section][side][1],
		    section_coordinates[section + 1][side][1],
		    section_points[section][side].y,
		    section_points[section + 1][side].y)) {
		return RENDERER_STATUS_INVALID_ARGUMENT;
	    }
	}
    }

    for (vertex_index = 0;
	 vertex_index < NELEM(geometry->vertices); vertex_index++) {
	section = section_indices[vertex_index];
	side = side_indices[vertex_index];
	Set_semantic_map_vertex(
	    &geometry->vertices[vertex_index], section_points[section][side],
	    Renderer_color_from_rgba32(section_colors[section]));
    }
    return RENDERER_STATUS_OK;
}

static void paint_dir_grav(int x, int y, int dir)
{
    SemanticWorldLineBatch batch;
    SemanticDirectionalGravityGeometry geometry;
    SdlRenderer *sdl_renderer;
    RendererStatus status;

    status = Build_semantic_directional_gravity_geometry(
	x, y, dir, &geometry);
    if (status != RENDERER_STATUS_OK) {
	sdl_renderer = Get_sdl_renderer();
	if (sdl_renderer != NULL) {
	    (void)Sdl_renderer_track_frame_result(sdl_renderer, status);
	}
	return;
    }
    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Renderer_set_blend(batch.renderer, RENDERER_BLEND_ALPHA);
    if (Track_semantic_world_line_batch(&batch, status)
	!= RENDERER_STATUS_OK) {
	return;
    }
    status = Renderer_draw_triangles(
	batch.renderer, NULL, geometry.vertices, NELEM(geometry.vertices));
    (void)Accept_semantic_world_line_command(&batch, status);
    (void)Finish_semantic_world_line_batch(&batch);
}

void Gui_paint_setup_up_grav(int x, int y)
{
    paint_dir_grav(x, y, 0);
}

void Gui_paint_setup_down_grav(int x, int y)
{
    paint_dir_grav(x, y, 2);
}

void Gui_paint_setup_right_grav(int x, int y)
{
    paint_dir_grav(x, y, 1);
}

void Gui_paint_setup_left_grav(int x, int y)
{
    paint_dir_grav(x, y, 3);
}

void Gui_paint_setup_worm(int x, int y)
{
    Image_paint_rotated(IMG_WORMHOLE, x, y, (loopsSlow << 3) % TABLE_SIZE, whiteRGBA);
}

void Gui_paint_setup_item_concentrator(int x, int y)
{
    Image_paint_rotated(IMG_CONCENTRATOR, x, y, (loopsSlow << 3) % TABLE_SIZE, whiteRGBA);
}

void Gui_paint_setup_asteroid_concentrator(int x, int y)
{
    Image_paint_rotated(IMG_ASTEROIDCONC, x, y, (loopsSlow << 4) % TABLE_SIZE, whiteRGBA);
}

static void Set_semantic_map_vertex(
    RendererVertex2D *vertex, RendererPoint2D point, RendererColor color)
{
    vertex->x = point.x;
    vertex->y = point.y;
    vertex->u = 0.0f;
    vertex->v = 0.0f;
    vertex->color = color;
}

static RendererStatus Paint_semantic_map_quad(
    const RendererPoint2D points[4], Uint32 packed_color)
{
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererVertex2D vertices[6];
    RendererColor color;
    RendererStatus status;
    RendererStatus operation_status;
    size_t vertex_index;
    static const size_t point_indices[6] = {0, 1, 2, 0, 2, 3};

    if (points == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    sdl_renderer = Get_sdl_renderer();
    if (sdl_renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, RENDERER_STATUS_OK);
    if (status != RENDERER_STATUS_OK)
	return status;
    renderer = Sdl_renderer_frontend(sdl_renderer);
    if (renderer == NULL) {
	return Sdl_renderer_track_frame_result(
	    sdl_renderer, RENDERER_STATUS_INVALID_STATE);
    }

    color = Renderer_color_from_rgba32(packed_color);
    for (vertex_index = 0; vertex_index < NELEM(vertices); vertex_index++) {
	Set_semantic_map_vertex(
	    &vertices[vertex_index], points[point_indices[vertex_index]], color);
    }
    operation_status = Renderer_set_blend(
	renderer, RENDERER_BLEND_ALPHA);
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, operation_status);
    if (status != RENDERER_STATUS_OK)
	return status;
    operation_status = Renderer_draw_triangles(
	renderer, NULL, vertices, NELEM(vertices));
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, operation_status);
    if (operation_status != RENDERER_STATUS_OK)
	return status;
    operation_status = Sdl_renderer_flush(sdl_renderer);
    return Sdl_renderer_track_frame_result(
	sdl_renderer, operation_status);
}

static int64_t Semantic_map_arithmetic_half(int64_t value)
{
    int64_t half = value / INT64_C(2);

    if (value < 0 && value % INT64_C(2) != 0)
	half--;
    return half;
}

void Gui_paint_decor_dot(int x, int y, int size)
{
    RendererPoint2D points[4];
    int64_t low_x = (int64_t)x
	+ Semantic_map_arithmetic_half(
	    (int64_t)BLOCK_SZ - (int64_t)size);
    int64_t low_y = (int64_t)y
	+ Semantic_map_arithmetic_half(
	    (int64_t)BLOCK_SZ - (int64_t)size);
    int64_t high_x = (int64_t)x
	+ Semantic_map_arithmetic_half(
	    (int64_t)BLOCK_SZ + (int64_t)size);
    int64_t high_y = (int64_t)y
	+ Semantic_map_arithmetic_half(
	    (int64_t)BLOCK_SZ + (int64_t)size);

    points[0] = (RendererPoint2D){(float)low_x, (float)low_y};
    points[1] = (RendererPoint2D){(float)low_x, (float)high_y};
    points[2] = (RendererPoint2D){(float)high_x, (float)high_y};
    points[3] = (RendererPoint2D){(float)high_x, (float)low_y};
    (void)Paint_semantic_map_quad(points, wallColorRGBA);
}

typedef struct SemanticTargetGeometry {
    RendererPoint2D outline[4];
    float fill_x;
    float fill_y;
    float fill_width;
    float fill_height;
    int draw_fill;
} SemanticTargetGeometry;

static int Semantic_target_float(int64_t value, float *result)
{
    if (result == NULL || (double)value < -(double)FLT_MAX
	|| (double)value > (double)FLT_MAX) {
	return 0;
    }
    *result = (float)value;
    return isfinite(*result);
}

static int Semantic_target_coordinate_float(int64_t value, float *result)
{
    return value >= INT_MIN && value <= INT_MAX
	&& Semantic_target_float(value, result);
}

static int Semantic_target_float_rect_valid(
    float left, float top, float right, float bottom)
{
    return isfinite(left) && isfinite(top)
	&& isfinite(right) && isfinite(bottom)
	&& right > left && bottom > top;
}

static RendererStatus Build_semantic_target_geometry(
    int x, int y, double damage, SemanticTargetGeometry *geometry)
{
    double scaled_damage;
    int damage_offset;
    int64_t left;
    int64_t right;
    int64_t top;
    int64_t bottom;
    int64_t damage_y;
    int64_t fill_top;
    int64_t fill_height;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    scaled_damage = (BLOCK_SZ - 3) * (damage / TARGET_DAMAGE);
    if (!isfinite(scaled_damage)
	|| scaled_damage < (double)INT_MIN
	|| scaled_damage > (double)INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    damage_offset = (int)scaled_damage;

    left = (int64_t)x;
    right = left + INT64_C(5);
    top = (int64_t)y + INT64_C(3);
    bottom = (int64_t)y + (int64_t)BLOCK_SZ;
    damage_y = (int64_t)y + (int64_t)damage_offset;
    if (!Semantic_target_coordinate_float(left, &geometry->outline[0].x)
	|| !Semantic_target_coordinate_float(top, &geometry->outline[0].y)
	|| !Semantic_target_coordinate_float(left, &geometry->outline[1].x)
	|| !Semantic_target_coordinate_float(bottom,
					       &geometry->outline[1].y)
	|| !Semantic_target_coordinate_float(right, &geometry->outline[2].x)
	|| !Semantic_target_coordinate_float(bottom,
					       &geometry->outline[2].y)
	|| !Semantic_target_coordinate_float(right, &geometry->outline[3].x)
	|| !Semantic_target_coordinate_float(top, &geometry->outline[3].y)
	|| !Semantic_target_coordinate_float(damage_y,
					       &geometry->fill_y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (!Semantic_target_float_rect_valid(
	    geometry->outline[0].x, geometry->outline[0].y,
	    geometry->outline[2].x, geometry->outline[1].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    geometry->draw_fill = damage_y != top;
    if (!geometry->draw_fill)
	return RENDERER_STATUS_OK;

    fill_top = damage_y < top ? damage_y : top;
    fill_height = damage_y < top ? top - damage_y : damage_y - top;
    if (!Semantic_target_coordinate_float(left, &geometry->fill_x)
	|| !Semantic_target_coordinate_float(fill_top, &geometry->fill_y)
	|| !Semantic_target_float(right - left, &geometry->fill_width)
	|| !Semantic_target_float(fill_height, &geometry->fill_height)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (!Semantic_target_float_rect_valid(
	    geometry->fill_x, geometry->fill_y,
	    geometry->fill_x + geometry->fill_width,
	    geometry->fill_y + geometry->fill_height)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

void Gui_paint_setup_target(int x, int y, int team, double damage, bool own)
{
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    SemanticTargetGeometry geometry;
    RendererStatus status;
    RendererStatus operation_status;
    RendererColor color;
    int has_accepted_commands = 0;

    Image_paint(IMG_TARGET, x, y, 0, whiteRGBA);
    sdl_renderer = Get_sdl_renderer();
    if (sdl_renderer == NULL)
	return;
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, RENDERER_STATUS_OK);
    if (status != RENDERER_STATUS_OK)
	return;

    if (BIT(Setup->mode, TEAM_PLAY)) {
	int64_t map_x = (int64_t)x + (int64_t)BLOCK_SZ;

	if (map_x < INT_MIN || map_x > INT_MAX) {
	    (void)Sdl_renderer_track_frame_result(
		sdl_renderer, RENDERER_STATUS_INVALID_ARGUMENT);
	    return;
	}
	operation_status = mapprint(
	    &mapfont, whiteRGBA, RIGHT, UP, (int)map_x, y, "%d", team);
	status = Sdl_renderer_track_frame_result(
	    sdl_renderer, operation_status);
	if (status != RENDERER_STATUS_OK)
	    return;
    }

    if (damage == TARGET_DAMAGE)
	return;
    operation_status = Build_semantic_target_geometry(
	x, y, damage, &geometry);
    if (operation_status != RENDERER_STATUS_OK) {
	(void)Sdl_renderer_track_frame_result(
	    sdl_renderer, operation_status);
	return;
    }

    renderer = Sdl_renderer_frontend(sdl_renderer);
    if (renderer == NULL) {
	(void)Sdl_renderer_track_frame_result(
	    sdl_renderer, RENDERER_STATUS_INVALID_STATE);
	return;
    }
    operation_status = Renderer_set_blend(
	renderer, RENDERER_BLEND_ALPHA);
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, operation_status);
    if (status != RENDERER_STATUS_OK)
	return;

    color = Renderer_color_from_rgba32(own ? blueRGBA : redRGBA);
    operation_status = Renderer_stroke_path(
	renderer, geometry.outline, NELEM(geometry.outline), 1.0f,
	color, 1);
    if (operation_status == RENDERER_STATUS_OK)
	has_accepted_commands = 1;
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, operation_status);
    if (status == RENDERER_STATUS_OK && geometry.draw_fill) {
	operation_status = Renderer_fill_rect(
	    renderer, geometry.fill_x, geometry.fill_y,
	    geometry.fill_width, geometry.fill_height, color);
	if (operation_status == RENDERER_STATUS_OK)
	    has_accepted_commands = 1;
	(void)Sdl_renderer_track_frame_result(
	    sdl_renderer, operation_status);
    }

    if (has_accepted_commands) {
	operation_status = Sdl_renderer_flush(
	    sdl_renderer);
	(void)Sdl_renderer_track_frame_result(
	    sdl_renderer, operation_status);
    }
}

void Gui_paint_setup_treasure(int x, int y, int team, bool own)
{
    Image_paint(own ? IMG_HOLDER_FRIEND : IMG_HOLDER_ENEMY, x, y, 0, whiteRGBA);
}

void Gui_paint_walls(int x, int y, int type)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_wall_geometry(
	x, y, type, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, wallColorRGBA, RENDERER_BLEND_ALPHA);
}

void Gui_paint_filled_slice(int bl, int tl, int tr, int br, int y)
{
    RendererPoint2D points[4];
    float bottom = (float)y;
    float top = (float)((int64_t)y + (int64_t)BLOCK_SZ);

    points[0] = (RendererPoint2D){(float)bl, bottom};
    points[1] = (RendererPoint2D){(float)tl, top};
    points[2] = (RendererPoint2D){(float)tr, top};
    points[3] = (RendererPoint2D){(float)br, bottom};
    (void)Paint_semantic_map_quad(points, wallColorRGBA);
}

static RendererStatus Get_semantic_polygon_color(
    int rgb, RendererColor *color)
{
    uint32_t rgba;

    if (color == NULL || rgb < 0 || rgb > 0x00ffffff)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    rgba = ((uint32_t)rgb << 8) | UINT32_C(0xff);
    *color = Renderer_color_from_rgba32(rgba);
    return RENDERER_STATUS_OK;
}

static RendererStatus Set_semantic_polygon_triangle_position(
    RendererVertex2D *vertex, int64_t origin_x, int64_t origin_y,
    RendererPoint2D local)
{
    double x;
    double y;

    if (vertex == NULL || !isfinite(local.x) || !isfinite(local.y))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    x = (double)origin_x + (double)local.x;
    y = (double)origin_y + (double)local.y;
    if (!isfinite(x) || !isfinite(y)
	|| x < INT_MIN || x > INT_MAX
	|| y < INT_MIN || y > INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    vertex->x = (float)x;
    vertex->y = (float)y;
    if (!isfinite(vertex->x) || !isfinite(vertex->y))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    return RENDERER_STATUS_OK;
}

static int Semantic_polygon_triangle_extent_preserved(
    RendererPoint2D first_local, RendererPoint2D second_local,
    const RendererVertex2D *first_world,
    const RendererVertex2D *second_world)
{
    return (first_local.x == second_local.x
	    || first_world->x != second_world->x)
	&& (first_local.y == second_local.y
	    || first_world->y != second_world->y);
}

static double Semantic_polygon_triangle_twice_area(
    const RendererVertex2D *first, const RendererVertex2D *second,
    const RendererVertex2D *third)
{
    return ((double)second->x - first->x)
	    * ((double)third->y - first->y)
	- ((double)second->y - first->y)
	    * ((double)third->x - first->x);
}

static RendererStatus Prepare_semantic_polygon_triangle_vertices(
    SemanticPolygonCacheEntry *entry, int64_t origin_x, int64_t origin_y,
    RendererColor color)
{
    size_t index;

    if (entry == NULL
	|| (entry->fill_geometry.triangle_point_count != 0
	    && entry->draw_vertices == NULL)) {
	return RENDERER_STATUS_INVALID_STATE;
    }
    for (index = 0;
	 index < entry->fill_geometry.triangle_point_count;
	 index++) {
	RendererStatus status = Set_semantic_polygon_triangle_position(
	    &entry->draw_vertices[index], origin_x, origin_y,
	    entry->fill_geometry.triangle_points[index]);

	if (status != RENDERER_STATUS_OK)
	    return status;
	entry->draw_vertices[index].u = 0.0f;
	entry->draw_vertices[index].v = 0.0f;
	entry->draw_vertices[index].color = color;
    }
    for (index = 0;
	 index < entry->fill_geometry.triangle_point_count;
	 index += 3) {
	RendererPoint2D *local = &entry->fill_geometry.triangle_points[index];
	RendererVertex2D *world_vertex = &entry->draw_vertices[index];
	double twice_area;

	if (!Semantic_polygon_triangle_extent_preserved(
		local[0], local[1], &world_vertex[0], &world_vertex[1])
	    || !Semantic_polygon_triangle_extent_preserved(
		local[1], local[2], &world_vertex[1], &world_vertex[2])
	    || !Semantic_polygon_triangle_extent_preserved(
		local[2], local[0], &world_vertex[2], &world_vertex[0])) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	twice_area = Semantic_polygon_triangle_twice_area(
	    &world_vertex[0], &world_vertex[1], &world_vertex[2]);
	if (!isfinite(twice_area) || twice_area == 0.0)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Prepare_semantic_polygon_texture_vertices(
    SemanticPolygonCacheEntry *entry, const image_t *image)
{
    RendererColor white = {255, 255, 255, 255};
    float frame_width;
    float height;
    float min_x;
    float min_y;
    float vertical_offset;
    size_t index;

    if (entry == NULL || image == NULL
	|| image->width <= 0 || image->height <= 0
	|| image->frame_width <= 0 || image->frame_width > image->width) {
	return RENDERER_STATUS_INVALID_STATE;
    }
    frame_width = (float)image->frame_width;
    height = (float)image->height;
    min_x = (float)entry->min_x;
    min_y = (float)entry->min_y;
    vertical_offset = (float)(entry->bounds_height % image->height);
    for (index = 0;
	 index < entry->fill_geometry.triangle_point_count;
	 index++) {
	RendererPoint2D local = entry->fill_geometry.triangle_points[index];
	RendererVertex2D *vertex = &entry->draw_vertices[index];

	vertex->u = (local.x - min_x) / frame_width;
	/* Convert the legacy bottom-origin texture coordinate to the semantic
	 * top-origin convention without removing intentional repeat intervals. */
	vertex->v = 1.0f
	    - (local.y - min_y - vertical_offset) / height;
	vertex->color = white;
	if (!isfinite(vertex->u) || !isfinite(vertex->v))
	    return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static int Semantic_polygon_has_visible_edge(
    const SemanticPolygonCacheEntry *entry)
{
    size_t index;

    if (entry == NULL)
	return 0;
    for (index = 0; index < entry->point_count; index++) {
	size_t next = (index + 1) % entry->point_count;
	RendererPoint2D first = entry->world_points[index];
	RendererPoint2D second = entry->world_points[next];

	if ((!entry->has_special_edges || entry->visible_edges[index])
	    && (first.x != second.x || first.y != second.y)) {
	    return 1;
	}
    }
    return 0;
}

static RendererStatus Paint_semantic_polygon_outline(
    SemanticWorldLineBatch *batch, SemanticPolygonCacheEntry *entry,
    float width, RendererColor color)
{
    size_t index;

    if (batch == NULL || entry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!entry->has_special_edges) {
	return Accept_semantic_world_line_command(
	    batch, Renderer_stroke_path(
		batch->renderer, entry->world_points, entry->point_count,
		width, color, 1));
    }
    for (index = 0; index < entry->point_count; index++) {
	RendererPoint2D segment[2];
	RendererStatus status;

	if (!entry->visible_edges[index])
	    continue;
	segment[0] = entry->world_points[index];
	segment[1] = entry->world_points[
	    (index + 1) % entry->point_count];
	if (segment[0].x == segment[1].x
	    && segment[0].y == segment[1].y) {
	    continue;
	}
	status = Accept_semantic_world_line_command(
	    batch, Renderer_stroke_path(
		batch->renderer, segment, 2, width, color, 0));
	if (status != RENDERER_STATUS_OK)
	    return status;
    }
    return RENDERER_STATUS_OK;
}

void Gui_paint_polygon(int i, int xoff, int yoff)
{
    SemanticWorldLineBatch batch;
    SemanticPolygonCacheEntry *entry;
    polygon_style_t p_style;
    edge_style_t e_style;
    RendererColor fill_color;
    RendererColor outline_color;
    RendererTexture *texture = NULL;
    image_t *image = NULL;
    RendererStatus status;
    int64_t origin_x;
    int64_t origin_y;
    int64_t offset_x;
    int64_t offset_y;
    int did_fill;
    int draw_fill;
    int draw_outline;
    float outline_width = 0.0f;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    if (!semanticPolygonCacheReady || semanticPolygonCache == NULL
	|| semanticPolygonCacheSource != polygons
	|| num_polygons < 0
	|| (size_t)num_polygons != semanticPolygonCacheCount) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_STATE);
	return;
    }
    if (i < 0 || (size_t)i >= semanticPolygonCacheCount
	|| polygon_styles == NULL || num_polygon_styles <= 0
	|| polygons[i].style < 0
	|| polygons[i].style >= num_polygon_styles) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    entry = &semanticPolygonCache[i];
    p_style = polygon_styles[polygons[i].style];
    if (BIT(p_style.flags, STYLE_INVISIBLE))
	return;
    if (edge_styles == NULL || num_edge_styles <= 0
	|| p_style.def_edge_style < 0
	|| p_style.def_edge_style >= num_edge_styles) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    e_style = edge_styles[p_style.def_edge_style];
    status = Get_semantic_polygon_color(p_style.rgb, &fill_color);
    if (status == RENDERER_STATUS_OK)
	status = Get_semantic_polygon_color(e_style.rgb, &outline_color);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    if (Setup == NULL || Setup->width <= 0 || Setup->height <= 0) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    offset_x = (int64_t)xoff * (int64_t)Setup->width;
    offset_y = (int64_t)yoff * (int64_t)Setup->height;
    if (!Semantic_polygon_add_int64(
	    entry->anchor_x, offset_x, &origin_x)
	|| !Semantic_polygon_add_int64(
	    entry->anchor_y, offset_y, &origin_y)) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    status = Validate_semantic_polygon_contour(entry, origin_x, origin_y);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }

    did_fill = (instruments.texturedWalls || instruments.filledWorld)
	&& BIT(p_style.flags, STYLE_TEXTURED | STYLE_FILLED);
    draw_fill = did_fill
	&& entry->fill_geometry.triangle_point_count != 0;
    if (draw_fill) {
	status = Prepare_semantic_polygon_triangle_vertices(
	    entry, origin_x, origin_y, fill_color);
	if (status != RENDERER_STATUS_OK) {
	    (void)Track_semantic_world_line_batch(&batch, status);
	    return;
	}
    }

    if (draw_fill && BIT(p_style.flags, STYLE_TEXTURED)
	&& instruments.texturedWalls) {
	if (p_style.texture < 0 || p_style.texture > 255) {
	    (void)Track_semantic_world_line_batch(
		&batch, RENDERER_STATUS_INVALID_ARGUMENT);
	    return;
	}
	image = Image_get_texture(p_style.texture);
	if (image != NULL) {
	    if (image->state != IMG_STATE_READY || image->texture == NULL) {
		(void)Track_semantic_world_line_batch(
		    &batch, RENDERER_STATUS_INVALID_STATE);
		return;
	    }
	    if (image->renderer != batch.renderer) {
		(void)Track_semantic_world_line_batch(
		    &batch, RENDERER_STATUS_RESOURCE_MISMATCH);
		return;
	    }
	    status = Prepare_semantic_polygon_texture_vertices(entry, image);
	    if (status != RENDERER_STATUS_OK) {
		(void)Track_semantic_world_line_batch(&batch, status);
		return;
	    }
	    texture = image->texture;
	}
    }

    if (e_style.width < -1) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    draw_outline = e_style.width != -1 || !did_fill;
    if (draw_outline) {
	double physical_width;

	if (!isfinite(clData.scale) || clData.scale <= 0.0) {
	    (void)Track_semantic_world_line_batch(
		&batch, RENDERER_STATUS_INVALID_ARGUMENT);
	    return;
	}
	if (e_style.width == 0)
	    physical_width = 1.0;
	else if (e_style.width == -1)
	    physical_width = clData.scale;
	else
	    physical_width = (double)e_style.width * clData.scale;
	outline_width = (float)physical_width;
	if (!isfinite(physical_width) || physical_width <= 0.0
	    || !isfinite(outline_width) || outline_width <= 0.0f) {
	    (void)Track_semantic_world_line_batch(
		&batch, RENDERER_STATUS_INVALID_ARGUMENT);
	    return;
	}
    }
    draw_outline = draw_outline
	&& Semantic_polygon_has_visible_edge(entry);
    if (!draw_fill && !draw_outline)
	return;

    /* Everything derived from mutable scene tables and external texture
     * resources is validated before the first renderer state command. */
    status = Track_semantic_world_line_batch(
	&batch, Renderer_set_blend(batch.renderer, RENDERER_BLEND_ALPHA));
    if (status != RENDERER_STATUS_OK)
	return;
    if (draw_fill) {
	status = Accept_semantic_world_line_command(
	    &batch, Renderer_draw_triangles(
		batch.renderer, texture, entry->draw_vertices,
		entry->fill_geometry.triangle_point_count));
	if (status != RENDERER_STATUS_OK) {
	    (void)Finish_semantic_world_line_batch(&batch);
	    return;
	}
    }
    if (draw_outline) {
	status = Paint_semantic_polygon_outline(
	    &batch, entry, outline_width, outline_color);
	if (status != RENDERER_STATUS_OK) {
	    (void)Finish_semantic_world_line_batch(&batch);
	    return;
	}
    }
    (void)Finish_semantic_world_line_batch(&batch);
}


/* Object painting */


void Gui_paint_item_object(int type, int x, int y)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;
    int64_t image_x;
    int64_t image_y;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    image_x = (int64_t)x - INT64_C(8);
    image_y = (int64_t)y - INT64_C(4);
    if (image_x < INT_MIN || image_x > INT_MAX
	|| image_y < INT_MIN || image_y > INT_MAX) {
	(void)Track_semantic_world_line_batch(
	    &batch, RENDERER_STATUS_INVALID_ARGUMENT);
	return;
    }
    Image_paint(
	IMG_ALL_ITEMS, (int)image_x, (int)image_y, type, whiteRGBA);
    /* Do not submit the outline if the semantic image draw failed. */
    if (Track_semantic_world_line_batch(&batch, RENDERER_STATUS_OK)
	!= RENDERER_STATUS_OK) {
	return;
    }
    status = Build_semantic_item_outline_geometry(x, y, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, blueRGBA, RENDERER_BLEND_ALPHA);
}

#ifdef XPILOT_SDLGUI_TEST_HOOKS
void Sdlgui_test_set_textured_balls(int enabled)
{
    texturedBalls = enabled != 0;
}
#endif

void Gui_paint_ball(int x, int y, int style)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;
    int rgba = ballColorRGBA;

    if (style >= 0 && style < num_polygon_styles)
	rgba = (polygon_styles[style].rgb << 8) | 0xff;

    if (texturedBalls)
	Image_paint(IMG_BALL, x - BALL_RADIUS, y - BALL_RADIUS, 0, rgba);
    else {
	if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	    return;
	status = Build_semantic_ball_outline_geometry(x, y, &geometry);
	if (status != RENDERER_STATUS_OK) {
	    (void)Track_semantic_world_line_batch(&batch, status);
	    return;
	}
	(void)Paint_semantic_world_line_geometry(
	    &batch, &geometry, ballColorRGBA, RENDERER_BLEND_ADDITIVE);
    }
}

void Gui_paint_ball_connector(int x_1, int y_1, int x_2, int y_2)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_segment_geometry(
	x_1, y_1, x_2, y_2, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, connColorRGBA, RENDERER_BLEND_ADDITIVE);
}

void Gui_paint_mine(int x, int y, int teammine, char *name)
{
    Image_paint(teammine ? IMG_MINE_TEAM : IMG_MINE_OTHER,
		x - 10, y - 7, 0, whiteRGBA);
    if (name) {
	mapnprint(  &mapfont, 
    	    	    teammine ? blueRGBA : whiteRGBA,
    	    	    CENTER, DOWN,
    	    	    x, y - 15, 
    	    	    maxCharsInNames,"%s", name	);
    }
}

typedef struct SemanticSparkBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    RendererColoredPoint2D *points;
    size_t point_count;
    size_t point_capacity;
    float point_size;
    int active;
} SemanticSparkBatch;

static SemanticSparkBatch semantic_spark_batch;

static size_t Maximum_semantic_spark_points(void)
{
    size_t maximum_points = SIZE_MAX / sizeof(RendererColoredPoint2D);
    size_t maximum_vertices = SIZE_MAX / sizeof(RendererVertex2D);
    size_t pointer_points = (size_t)PTRDIFF_MAX
	/ sizeof(RendererColoredPoint2D);
    size_t pointer_vertices = (size_t)PTRDIFF_MAX
	/ sizeof(RendererVertex2D);

    if (maximum_points > pointer_points)
	maximum_points = pointer_points;
    if (maximum_vertices > pointer_vertices)
	maximum_vertices = pointer_vertices;
    if (maximum_vertices > (size_t)INT_MAX)
	maximum_vertices = (size_t)INT_MAX;
    if (maximum_points > maximum_vertices / 6)
	maximum_points = maximum_vertices / 6;
    return maximum_points;
}

static RendererStatus Track_semantic_spark_batch(
    SemanticSparkBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	if (batch->status == RENDERER_STATUS_OK)
	    batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static RendererStatus Build_semantic_spark_point(
    int color, int x, int y, float point_size,
    RendererColoredPoint2D *point)
{
    int64_t point_x;
    int64_t point_y;
    float half_size;
    float left;
    float top;
    float right;
    float bottom;

    if (point == NULL || color < 0 || color >= 8
	|| !isfinite(point_size) || point_size <= 0.0f) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    point_x = (int64_t)x + (int64_t)world.x;
    point_y = (int64_t)world.y + (int64_t)ext_view_height - (int64_t)y;
    if (point_x < INT_MIN || point_x > INT_MAX
	|| point_y < INT_MIN || point_y > INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    point->position.x = (float)point_x;
    point->position.y = (float)point_y;
    half_size = point_size * 0.5f;
    left = point->position.x - half_size;
    top = point->position.y - half_size;
    right = point->position.x + half_size;
    bottom = point->position.y + half_size;
    if (!isfinite(point->position.x) || !isfinite(point->position.y)
	|| !isfinite(half_size) || !isfinite(left) || !isfinite(top)
	|| !isfinite(right) || !isfinite(bottom)
	|| left == right || top == bottom) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    point->color.red = (uint8_t)(255 * (color + 1) / 8);
    point->color.green = (uint8_t)(255 * color * color / 64);
    point->color.blue = 0;
    point->color.alpha = 255;
    return RENDERER_STATUS_OK;
}

static RendererStatus Reserve_semantic_spark_points(
    SemanticSparkBatch *batch, size_t additional_points)
{
    const size_t maximum_points = Maximum_semantic_spark_points();
    RendererColoredPoint2D *replacement;
    size_t needed;
    size_t capacity;

    if (batch == NULL || batch->point_count > maximum_points
	|| additional_points > maximum_points - batch->point_count) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    needed = batch->point_count + additional_points;
    if (needed <= batch->point_capacity)
	return RENDERER_STATUS_OK;
    capacity = batch->point_capacity == 0 ? 16 : batch->point_capacity;
    if (capacity > maximum_points)
	capacity = maximum_points;
    while (capacity < needed) {
	if (capacity > maximum_points / 2) {
	    capacity = maximum_points;
	    break;
	}
	capacity *= 2;
    }
    if (capacity < needed
	|| capacity > SIZE_MAX / sizeof(*batch->points)
	|| capacity > (size_t)PTRDIFF_MAX / sizeof(*batch->points)) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    replacement = realloc(batch->points, capacity * sizeof(*batch->points));
    if (replacement == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    batch->points = replacement;
    batch->point_capacity = capacity;
    return RENDERER_STATUS_OK;
}

void Gui_paint_sparks_begin(void)
{
    SemanticSparkBatch *batch = &semantic_spark_batch;

    if (batch->active)
	Gui_paint_sparks_end();
    free(batch->points);
    memset(batch, 0, sizeof(*batch));
    batch->active = 1;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->sdl_renderer = Get_sdl_renderer();
    if (batch->sdl_renderer == NULL)
	return;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return;
    batch->point_size = (float)sparkSize;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	(void)Track_semantic_spark_batch(
	    batch, RENDERER_STATUS_INVALID_STATE);
    }
}

void Gui_paint_spark(int color, int x, int y)
{
    SemanticSparkBatch *batch = &semantic_spark_batch;
    RendererColoredPoint2D point;
    RendererStatus status = RENDERER_STATUS_OK;
    int standalone = !batch->active;

    if (standalone)
	Gui_paint_sparks_begin();
    if (batch->status == RENDERER_STATUS_OK) {
	status = Build_semantic_spark_point(
	    color, x, y, batch->point_size, &point);
    }
    if (status == RENDERER_STATUS_OK && batch->status == RENDERER_STATUS_OK)
	status = Reserve_semantic_spark_points(batch, 1);
    if (status == RENDERER_STATUS_OK && batch->status == RENDERER_STATUS_OK) {
	batch->points[batch->point_count] = point;
	batch->point_count++;
    } else if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_spark_batch(batch, status);
    }
    if (standalone)
	Gui_paint_sparks_end();
}

void Gui_paint_sparks_end(void)
{
    SemanticSparkBatch *batch = &semantic_spark_batch;
    RendererStatus status;

    if (!batch->active)
	return;
    if (batch->status == RENDERER_STATUS_OK && batch->point_count != 0) {
	status = Renderer_set_blend(batch->renderer, RENDERER_BLEND_ALPHA);
	if (Track_semantic_spark_batch(batch, status) == RENDERER_STATUS_OK) {
	    status = Renderer_draw_colored_points(
		batch->renderer, batch->points, batch->point_count,
		batch->point_size);
	    if (Track_semantic_spark_batch(batch, status)
		== RENDERER_STATUS_OK) {
		status = Sdl_renderer_flush(
		    batch->sdl_renderer);
		(void)Track_semantic_spark_batch(batch, status);
	    }
	}
    }
    free(batch->points);
    batch->points = NULL;
    batch->point_count = 0;
    batch->point_capacity = 0;
    batch->active = 0;
    batch->renderer = NULL;
    batch->sdl_renderer = NULL;
}

#ifdef XPILOT_SPARK_BATCH_TEST_HOOKS
RendererStatus Sdlgui_test_paint_spark_batch(
    const SdlguiTestSpark *entries, size_t entry_count)
{
    SdlRenderer *renderer;
    size_t entry_index;

    renderer = Get_sdl_renderer();
    if (renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    if (entries == NULL && entry_count != 0) {
	return Sdl_renderer_track_frame_result(
	    renderer, RENDERER_STATUS_INVALID_ARGUMENT);
    }
    if (entry_count > Maximum_semantic_spark_points()) {
	return Sdl_renderer_track_frame_result(
	    renderer, RENDERER_STATUS_OUT_OF_MEMORY);
    }
    Gui_paint_sparks_begin();
    for (entry_index = 0; entry_index < entry_count; entry_index++) {
	Gui_paint_spark(
	    entries[entry_index].color,
	    entries[entry_index].x, entries[entry_index].y);
    }
    Gui_paint_sparks_end();
    return Sdl_renderer_frame_result(renderer);
}
#endif

void Gui_paint_wreck(int x, int y, bool deadly, int wtype, int rot, int size)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_wreck_outline_geometry(
	x, y, wtype, rot, size, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_line_geometry(
	&batch, &geometry, deadly ? whiteRGBA : redRGBA,
	RENDERER_BLEND_ALPHA);
}

typedef struct SemanticAsteroidVector3 {
    double x;
    double y;
    double z;
} SemanticAsteroidVector3;

typedef struct SemanticAsteroidCandidate {
    double world_x;
    double world_y;
    RendererVertex2D vertex;
} SemanticAsteroidCandidate;

typedef struct SemanticAsteroidBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    RendererVertex2D *vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    int active;
} SemanticAsteroidBatch;

static SemanticAsteroidBatch semantic_asteroid_batch;

static size_t Maximum_semantic_asteroid_vertices(void)
{
    size_t maximum_vertices = SIZE_MAX / sizeof(RendererVertex2D);
    size_t pointer_vertices = (size_t)PTRDIFF_MAX
	/ sizeof(RendererVertex2D);

    if (maximum_vertices > pointer_vertices)
	maximum_vertices = pointer_vertices;
    if (maximum_vertices > (size_t)INT_MAX)
	maximum_vertices = (size_t)INT_MAX;
    maximum_vertices -= maximum_vertices % 3;
    return maximum_vertices;
}

static void Discard_semantic_asteroid_batch(void)
{
    SemanticAsteroidBatch *batch = &semantic_asteroid_batch;

    free(batch->vertices);
    memset(batch, 0, sizeof(*batch));
}

static RendererStatus Track_semantic_asteroid_batch(
    SemanticAsteroidBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	if (batch->status == RENDERER_STATUS_OK)
	    batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static void Fail_semantic_asteroid_batch(
    SemanticAsteroidBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return;
    (void)Track_semantic_asteroid_batch(batch, status);
    free(batch->vertices);
    batch->vertices = NULL;
    batch->vertex_count = 0;
    batch->vertex_capacity = 0;
}

static RendererStatus Rotate_semantic_asteroid_vector(
    const SemanticAsteroidVector3 *vector,
    const SemanticAsteroidVector3 *axis,
    double cosine, double sine, SemanticAsteroidVector3 *result)
{
    SemanticAsteroidVector3 cross;
    double dot;
    double one_minus_cosine;

    if (vector == NULL || axis == NULL || result == NULL
	|| !isfinite(vector->x) || !isfinite(vector->y)
	|| !isfinite(vector->z) || !isfinite(axis->x)
	|| !isfinite(axis->y) || !isfinite(axis->z)
	|| !isfinite(cosine) || !isfinite(sine)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    one_minus_cosine = 1.0 - cosine;
    cross.x = axis->y * vector->z - axis->z * vector->y;
    cross.y = axis->z * vector->x - axis->x * vector->z;
    cross.z = axis->x * vector->y - axis->y * vector->x;
    dot = axis->x * vector->x + axis->y * vector->y
	+ axis->z * vector->z;
    if (!isfinite(one_minus_cosine)
	|| !isfinite(cross.x) || !isfinite(cross.y)
	|| !isfinite(cross.z) || !isfinite(dot)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    result->x = vector->x * cosine + cross.x * sine
	+ axis->x * dot * one_minus_cosine;
    result->y = vector->y * cosine + cross.y * sine
	+ axis->y * dot * one_minus_cosine;
    result->z = vector->z * cosine + cross.z * sine
	+ axis->z * dot * one_minus_cosine;
    if (!isfinite(result->x) || !isfinite(result->y)
	|| !isfinite(result->z)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static int Semantic_asteroid_edge_extent_preserved(
    double first, double second, float first_float, float second_float)
{
    return first == second || first_float != second_float;
}

static RendererStatus Build_semantic_asteroid_vertices(
    int x, int y, int type, int rot, int size,
    RendererVertex2D vertices[VERTEX_COUNT], size_t *vertex_count)
{
    SemanticAsteroidCandidate candidates[VERTEX_COUNT];
    SemanticAsteroidVector3 axis;
    const double axis_length = sqrt(21.0);
    double cosine;
    double sine;
    double scale;
    size_t output_count = 0;
    int source_index;
    int triangle_start;

    if (vertices == NULL || vertex_count == NULL
	|| type < 0 || type > UINT8_MAX
	|| rot < 0 || rot > UINT8_MAX
	|| size < 0 || size > UINT8_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    *vertex_count = 0;
    if (size == 0)
	return RENDERER_STATUS_OK;

    /* Visibility must not hide malformed source data on back-facing faces. */
    for (source_index = 0; source_index < VERTEX_COUNT; source_index++) {
	if (!isfinite(vertex_vectors[source_index][0])
	    || !isfinite(vertex_vectors[source_index][1])
	    || !isfinite(vertex_vectors[source_index][2])
	    || !isfinite(normal_vectors[source_index][0])
	    || !isfinite(normal_vectors[source_index][1])
	    || !isfinite(normal_vectors[source_index][2])
	    || !isfinite(uv_vectors[source_index][0])
	    || !isfinite(uv_vectors[source_index][1])) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }

    cosine = tbl_cos[rot % TABLE_SIZE];
    sine = tbl_sin[rot % TABLE_SIZE];
    scale = 0.9 * (double)SHIP_SZ * (double)size;
    axis.x = ((type & 1) != 0 ? 1.0 : -1.0) / axis_length;
    axis.y = ((type & 2) != 0 ? 2.0 : -2.0) / axis_length;
    axis.z = ((type & 4) != 0 ? 4.0 : -4.0) / axis_length;
    if (!isfinite(axis_length) || axis_length <= 0.0
	|| !isfinite(cosine) || !isfinite(sine) || !isfinite(scale)
	|| !isfinite(axis.x) || !isfinite(axis.y) || !isfinite(axis.z)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    for (source_index = 0; source_index < VERTEX_COUNT; source_index++) {
	SemanticAsteroidVector3 source_position;
	SemanticAsteroidVector3 source_normal;
	SemanticAsteroidVector3 position;
	SemanticAsteroidVector3 normal;
	SemanticAsteroidCandidate *candidate = &candidates[source_index];
	double local_x;
	double local_y;
	double positive_normal_z;
	double intensity;
	double quantized_intensity;
	RendererStatus status;

	source_position.x = vertex_vectors[source_index][0];
	source_position.y = vertex_vectors[source_index][1];
	source_position.z = vertex_vectors[source_index][2];
	source_normal.x = normal_vectors[source_index][0];
	source_normal.y = normal_vectors[source_index][1];
	source_normal.z = normal_vectors[source_index][2];
	status = Rotate_semantic_asteroid_vector(
	    &source_position, &axis, cosine, sine, &position);
	if (status != RENDERER_STATUS_OK)
	    return status;
	status = Rotate_semantic_asteroid_vector(
	    &source_normal, &axis, cosine, sine, &normal);
	if (status != RENDERER_STATUS_OK)
	    return status;

	local_x = position.x * scale;
	local_y = position.y * scale;
	candidate->world_x = (double)x + local_x;
	candidate->world_y = (double)y + local_y;
	positive_normal_z = fmax(normal.z, 0.0);
	intensity = 0.18 + 0.8 * positive_normal_z;
	if (!isfinite(local_x) || !isfinite(local_y)
	    || !isfinite(candidate->world_x)
	    || !isfinite(candidate->world_y)
	    || !isfinite(positive_normal_z) || !isfinite(intensity)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	if (intensity < 0.0)
	    intensity = 0.0;
	if (intensity > 1.0)
	    intensity = 1.0;
	quantized_intensity = floor(intensity * 255.0 + 0.5);
	if (!isfinite(quantized_intensity)
	    || quantized_intensity < 0.0 || quantized_intensity > 255.0) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}

	candidate->vertex.x = (float)candidate->world_x;
	candidate->vertex.y = (float)candidate->world_y;
	candidate->vertex.u = uv_vectors[source_index][0];
	candidate->vertex.v = 1.0f - uv_vectors[source_index][1];
	if (!isfinite(candidate->vertex.x)
	    || !isfinite(candidate->vertex.y)
	    || !isfinite(candidate->vertex.u)
	    || !isfinite(candidate->vertex.v)
	    || (local_x != 0.0 && candidate->vertex.x == (float)x)
	    || (local_y != 0.0 && candidate->vertex.y == (float)y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	candidate->vertex.color.red = (uint8_t)quantized_intensity;
	candidate->vertex.color.green = (uint8_t)quantized_intensity;
	candidate->vertex.color.blue = (uint8_t)quantized_intensity;
	candidate->vertex.color.alpha = 255;
    }

    /* Cull in the same final float coordinate space consumed by the backend. */
    for (triangle_start = 0; triangle_start < VERTEX_COUNT;
	 triangle_start += 3) {
	SemanticAsteroidCandidate *first = &candidates[triangle_start];
	SemanticAsteroidCandidate *second = &candidates[triangle_start + 1];
	SemanticAsteroidCandidate *third = &candidates[triangle_start + 2];
	double area;

	if (!Semantic_asteroid_edge_extent_preserved(
		first->world_x, second->world_x,
		first->vertex.x, second->vertex.x)
	    || !Semantic_asteroid_edge_extent_preserved(
		first->world_y, second->world_y,
		first->vertex.y, second->vertex.y)
	    || !Semantic_asteroid_edge_extent_preserved(
		second->world_x, third->world_x,
		second->vertex.x, third->vertex.x)
	    || !Semantic_asteroid_edge_extent_preserved(
		second->world_y, third->world_y,
		second->vertex.y, third->vertex.y)
	    || !Semantic_asteroid_edge_extent_preserved(
		third->world_x, first->world_x,
		third->vertex.x, first->vertex.x)
	    || !Semantic_asteroid_edge_extent_preserved(
		third->world_y, first->world_y,
		third->vertex.y, first->vertex.y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	area = ((double)second->vertex.x - first->vertex.x)
	    * ((double)third->vertex.y - first->vertex.y)
	    - ((double)second->vertex.y - first->vertex.y)
	    * ((double)third->vertex.x - first->vertex.x);
	if (!isfinite(area))
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	if (!(area > 0.0))
	    continue;
	vertices[output_count++] = first->vertex;
	vertices[output_count++] = second->vertex;
	vertices[output_count++] = third->vertex;
    }
    *vertex_count = output_count;
    return RENDERER_STATUS_OK;
}

static RendererStatus Reserve_semantic_asteroid_vertices(
    SemanticAsteroidBatch *batch, size_t additional_vertices)
{
    const size_t maximum_vertices = Maximum_semantic_asteroid_vertices();
    RendererVertex2D *replacement;
    size_t needed;
    size_t capacity;

    if (batch == NULL || batch->vertex_count > maximum_vertices
	|| additional_vertices > maximum_vertices - batch->vertex_count) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    needed = batch->vertex_count + additional_vertices;
    if (needed <= batch->vertex_capacity)
	return RENDERER_STATUS_OK;
    capacity = batch->vertex_capacity == 0
	? (size_t)VERTEX_COUNT : batch->vertex_capacity;
    if (capacity > maximum_vertices)
	capacity = maximum_vertices;
    while (capacity < needed) {
	if (capacity > maximum_vertices / 2) {
	    capacity = maximum_vertices;
	    break;
	}
	capacity *= 2;
    }
    if (capacity < needed
	|| capacity > SIZE_MAX / sizeof(*batch->vertices)
	|| capacity > (size_t)PTRDIFF_MAX / sizeof(*batch->vertices)) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    replacement = realloc(
	batch->vertices, capacity * sizeof(*batch->vertices));
    if (replacement == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    batch->vertices = replacement;
    batch->vertex_capacity = capacity;
    return RENDERER_STATUS_OK;
}

static RendererStatus Resolve_semantic_asteroid_texture(
    SemanticAsteroidBatch *batch, RendererTexture **texture)
{
    image_t *image;

    if (batch == NULL || texture == NULL || batch->renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    *texture = NULL;
    image = Image_get(IMG_ASTEROID);
    if (image == NULL)
	return RENDERER_STATUS_OK;
    if (image->texture == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    if (image->renderer != batch->renderer)
	return RENDERER_STATUS_RESOURCE_MISMATCH;
    *texture = image->texture;
    return RENDERER_STATUS_OK;
}

void Gui_paint_asteroids_begin(void)
{
    SemanticAsteroidBatch *batch = &semantic_asteroid_batch;

    Discard_semantic_asteroid_batch();
    batch->active = 1;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->sdl_renderer = Get_sdl_renderer();
    if (batch->sdl_renderer == NULL)
	return;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	Fail_semantic_asteroid_batch(
	    batch, RENDERER_STATUS_INVALID_STATE);
    }
}

void Gui_paint_asteroid(int x, int y, int type, int rot, int size)
{
    SemanticAsteroidBatch *batch = &semantic_asteroid_batch;
    RendererVertex2D vertices[VERTEX_COUNT];
    size_t vertex_count;
    RendererStatus status;

    if (!batch->active || batch->status != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_asteroid_vertices(
	x, y, type, rot, size, vertices, &vertex_count);
    if (status == RENDERER_STATUS_OK && vertex_count != 0)
	status = Reserve_semantic_asteroid_vertices(batch, vertex_count);
    if (status != RENDERER_STATUS_OK) {
	Fail_semantic_asteroid_batch(batch, status);
	return;
    }
    if (vertex_count == 0)
	return;
    memcpy(&batch->vertices[batch->vertex_count], vertices,
	vertex_count * sizeof(vertices[0]));
    batch->vertex_count += vertex_count;
}

void Gui_paint_asteroids_end(void)
{
    SemanticAsteroidBatch *batch = &semantic_asteroid_batch;
    RendererTexture *texture = NULL;
    RendererStatus status;

    if (!batch->active)
	return;
    if (batch->status == RENDERER_STATUS_OK && batch->vertex_count != 0) {
	status = Resolve_semantic_asteroid_texture(batch, &texture);
	if (Track_semantic_asteroid_batch(batch, status)
	    == RENDERER_STATUS_OK) {
	    status = Renderer_set_blend(
		batch->renderer, RENDERER_BLEND_OPAQUE);
	    if (Track_semantic_asteroid_batch(batch, status)
		== RENDERER_STATUS_OK) {
		status = Renderer_draw_triangles(
		    batch->renderer, texture, batch->vertices,
		    batch->vertex_count);
		if (Track_semantic_asteroid_batch(batch, status)
		    == RENDERER_STATUS_OK) {
		    status = Sdl_renderer_flush(
			batch->sdl_renderer);
		    (void)Track_semantic_asteroid_batch(batch, status);
		}
	    }
	}
    }
    Discard_semantic_asteroid_batch();
}

#ifdef XPILOT_ASTEROID_BATCH_TEST_HOOKS
RendererStatus Sdlgui_test_paint_asteroid_batch(
    const SdlguiTestAsteroid *entries, size_t entry_count)
{
    SdlRenderer *renderer;
    size_t entry_index;

    renderer = Get_sdl_renderer();
    if (renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    if (entries == NULL && entry_count != 0) {
	return Sdl_renderer_track_frame_result(
	    renderer, RENDERER_STATUS_INVALID_ARGUMENT);
    }
    if (entry_count
	> Maximum_semantic_asteroid_vertices() / (size_t)VERTEX_COUNT) {
	return Sdl_renderer_track_frame_result(
	    renderer, RENDERER_STATUS_OUT_OF_MEMORY);
    }
    Gui_paint_asteroids_begin();
    for (entry_index = 0; entry_index < entry_count; entry_index++) {
	Gui_paint_asteroid(
	    entries[entry_index].x, entries[entry_index].y,
	    entries[entry_index].type, entries[entry_index].rotation,
	    entries[entry_index].size);
    }
    Gui_paint_asteroids_end();
    return Sdl_renderer_frame_result(renderer);
}
#endif


/*
 * It seems that currently the screen coordinates are calculated already
 * in paintobjects.c. This should be changed.
 */
void Gui_paint_fastshot(int color, int x, int y)
{
    int size = MIN(shotSize, 16);

    Image_paint(IMG_BULLET,
		x + world.x - size/2,
		world.y - 16 + size/2 - 1 + ext_view_height - y,
		size - 1, whiteRGBA);
}

void Gui_paint_teamshot(int x, int y)
{
    int size = MIN(teamShotSize, 16);

    Image_paint(IMG_BULLET_OWN,
		x + world.x - size/2,
		world.y - 16 + size/2 - 1 + ext_view_height - y,
		size - 1, whiteRGBA);
}

void Gui_paint_missiles_begin(void)
{
}

void Gui_paint_missiles_end(void)
{
}

void Gui_paint_missile(int x, int y, int len, int dir)
{
    Image_paint_rotated(IMG_MISSILE, x - 16, y - 16, dir, whiteRGBA);
}

typedef struct SemanticLaserBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    int has_accepted_commands;
} SemanticLaserBatch;

typedef struct SemanticLaserGeometry {
    RendererPoint2D points[2];
} SemanticLaserGeometry;

static SemanticLaserBatch semantic_laser_batch;

static RendererStatus Track_semantic_laser_batch(
    SemanticLaserBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static RendererStatus Accept_semantic_laser_command(
    SemanticLaserBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (status == RENDERER_STATUS_OK)
	batch->has_accepted_commands = 1;
    return Track_semantic_laser_batch(batch, status);
}

static RendererStatus Build_semantic_laser_geometry(
    int x, int y, int length, int direction,
    SemanticLaserGeometry *geometry)
{
    double cosine;
    double sine;
    double end_x;
    double end_y;
    int integer_end_x;
    int integer_end_y;

    if (geometry == NULL || direction < 0 || direction >= TABLE_SIZE)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    cosine = tbl_cos[direction];
    sine = tbl_sin[direction];
    if (!isfinite(cosine) || !isfinite(sine))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    end_x = (double)x + (double)length * cosine;
    end_y = (double)y + (double)length * sine;
    if (!isfinite(end_x) || !isfinite(end_y)
	|| end_x < (double)INT_MIN || end_x > (double)INT_MAX
	|| end_y < (double)INT_MIN || end_y > (double)INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    integer_end_x = (int)end_x;
    integer_end_y = (int)end_y;
    geometry->points[0].x = (float)x;
    geometry->points[0].y = (float)y;
    geometry->points[1].x = (float)integer_end_x;
    geometry->points[1].y = (float)integer_end_y;
    return RENDERER_STATUS_OK;
}

void Gui_paint_lasers_begin(void)
{
    SemanticLaserBatch *batch = &semantic_laser_batch;
    RendererStatus operation_status;

    /* Semantic strokes use the same backend-independent quad coverage as
     * other migrated lines; legacy GL_LINE_SMOOTH state is not mirrored. */
    batch->sdl_renderer = Get_sdl_renderer();
    batch->renderer = NULL;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->has_accepted_commands = 0;
    if (batch->sdl_renderer == NULL)
	return;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	(void)Track_semantic_laser_batch(
	    batch, RENDERER_STATUS_INVALID_STATE);
	return;
    }
    operation_status = Renderer_set_blend(
	batch->renderer, RENDERER_BLEND_ALPHA);
    (void)Track_semantic_laser_batch(batch, operation_status);
}

void Gui_paint_lasers_end(void)
{
    SemanticLaserBatch *batch = &semantic_laser_batch;
    RendererStatus flush_status;

    if (!batch->has_accepted_commands || batch->sdl_renderer == NULL)
	return;
    flush_status = Sdl_renderer_flush(
	batch->sdl_renderer);
    batch->has_accepted_commands = 0;
    (void)Track_semantic_laser_batch(batch, flush_status);
}

void Gui_paint_laser(int color, int x_1, int y_1, int len, int dir)
{
    SemanticLaserBatch *batch = &semantic_laser_batch;
    SemanticLaserGeometry geometry;
    RendererStatus operation_status;
    Uint32 rgba;

    if (batch->status != RENDERER_STATUS_OK)
	return;
    operation_status = Build_semantic_laser_geometry(
	x_1, y_1, len, dir, &geometry);
    if (operation_status != RENDERER_STATUS_OK) {
	(void)Track_semantic_laser_batch(batch, operation_status);
	return;
    }
    rgba =
	(color == RED) ? redRGBA :
	(color == BLUE) ? blueRGBA :
	whiteRGBA;

    operation_status = Renderer_stroke_path(
	batch->renderer, geometry.points, NELEM(geometry.points), 5.0f,
	Renderer_color_from_rgba32(rgba - UINT32_C(128)), 0);
    if (Accept_semantic_laser_command(batch, operation_status)
	!= RENDERER_STATUS_OK) {
	return;
    }
    operation_status = Renderer_stroke_path(
	batch->renderer, geometry.points, NELEM(geometry.points), 1.0f,
	Renderer_color_from_rgba32(rgba), 0);
    (void)Accept_semantic_laser_command(batch, operation_status);
}

void Gui_paint_paused(int x, int y, int count)
{
    Image_paint(IMG_PAUSED,
		x - BLOCK_SZ / 2,
		y - BLOCK_SZ / 2,
		(count <= 0 || loopsSlow % 10 >= 5) ? 1 : 0, whiteRGBA);
}

typedef struct SemanticAppearingBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    RendererVertex2D *vertices;
    size_t vertex_count;
    size_t vertex_capacity;
    int active;
} SemanticAppearingBatch;

static SemanticAppearingBatch semantic_appearing_batch;

static RendererStatus Track_semantic_appearing_batch(
    SemanticAppearingBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	if (batch->status == RENDERER_STATUS_OK)
	    batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static RendererStatus Build_semantic_appearing_vertices(
    int x, int y, int count, Uint32 packed_color,
    RendererVertex2D vertices[6])
{
    const int64_t hsize = INT64_C(3) * BLOCK_SZ / INT64_C(7);
    const int64_t left = (int64_t)x - hsize;
    const int64_t top = (int64_t)y - hsize;
    const int64_t right = left + INT64_C(2) * hsize + INT64_C(1);
    double height_value;
    int64_t height;
    int64_t bottom;
    RendererPoint2D points[4];
    RendererColor color;

    if (vertices == NULL || count < 0)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    height_value = ((double)count / 180.0) * (double)hsize + 1.0;
    if (!isfinite(height_value)
	|| (long double)height_value > (long double)INT64_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    height = (int64_t)height_value;
    if (height <= 0 || top > INT64_MAX - height)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    bottom = top + height;
    if (left < INT_MIN || left > INT_MAX
	|| top < INT_MIN || top > INT_MAX
	|| right < INT_MIN || right > INT_MAX
	|| bottom < INT_MIN || bottom > INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    points[0].x = (float)left;
    points[0].y = (float)top;
    points[1].x = (float)right;
    points[1].y = (float)top;
    points[2].x = (float)right;
    points[2].y = (float)bottom;
    points[3].x = (float)left;
    points[3].y = (float)bottom;
    if (!isfinite(points[0].x) || !isfinite(points[0].y)
	|| !isfinite(points[2].x) || !isfinite(points[2].y)
	|| points[0].x == points[1].x
	|| points[0].y == points[3].y) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    color = Renderer_color_from_rgba32(packed_color);
    Set_semantic_map_vertex(&vertices[0], points[0], color);
    Set_semantic_map_vertex(&vertices[1], points[1], color);
    Set_semantic_map_vertex(&vertices[2], points[2], color);
    Set_semantic_map_vertex(&vertices[3], points[0], color);
    Set_semantic_map_vertex(&vertices[4], points[2], color);
    Set_semantic_map_vertex(&vertices[5], points[3], color);
    return RENDERER_STATUS_OK;
}

static RendererStatus Reserve_semantic_appearing_vertices(
    SemanticAppearingBatch *batch, size_t additional_vertices)
{
    const size_t maximum_vertices = (size_t)INT_MAX
	- (size_t)INT_MAX % 3;
    RendererVertex2D *replacement;
    size_t needed;
    size_t capacity;

    if (batch == NULL
	|| additional_vertices > maximum_vertices - batch->vertex_count) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    needed = batch->vertex_count + additional_vertices;
    if (needed <= batch->vertex_capacity)
	return RENDERER_STATUS_OK;
    capacity = batch->vertex_capacity == 0 ? 6 : batch->vertex_capacity;
    while (capacity < needed) {
	if (capacity > maximum_vertices / 2) {
	    capacity = maximum_vertices;
	    break;
	}
	capacity *= 2;
    }
    if (capacity < needed
	|| capacity > SIZE_MAX / sizeof(*batch->vertices)) {
	return RENDERER_STATUS_OUT_OF_MEMORY;
    }
    replacement = realloc(
	batch->vertices, capacity * sizeof(*batch->vertices));
    if (replacement == NULL)
	return RENDERER_STATUS_OUT_OF_MEMORY;
    batch->vertices = replacement;
    batch->vertex_capacity = capacity;
    return RENDERER_STATUS_OK;
}

void Gui_paint_appearing_begin(void)
{
    SemanticAppearingBatch *batch = &semantic_appearing_batch;

    if (batch->active)
	Gui_paint_appearing_end();
    free(batch->vertices);
    memset(batch, 0, sizeof(*batch));
    batch->active = 1;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->sdl_renderer = Get_sdl_renderer();
    if (batch->sdl_renderer == NULL)
	return;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	(void)Track_semantic_appearing_batch(
	    batch, RENDERER_STATUS_INVALID_STATE);
    }
}

void Gui_paint_appearing(int x, int y, int id, int count)
{
    SemanticAppearingBatch *batch = &semantic_appearing_batch;
    RendererVertex2D vertices[6];
    RendererStatus status = RENDERER_STATUS_OK;
    Uint32 color;
    other_t *other;
    int standalone = !batch->active;

    if (standalone)
	Gui_paint_appearing_begin();
    other = Other_by_id(id);

    /* Make a note we are doing the base warning. */
    if (version >= 0x4F12) {
	homebase_t *base = Homebase_by_id(id);
	if (base != NULL) {
	    double candidate = (double)loops
		+ ((double)count * clientFPS) / 120.0;

	    if (!isfinite(candidate)
		|| (long double)candidate < (long double)LONG_MIN
		|| (long double)candidate > (long double)LONG_MAX) {
		status = RENDERER_STATUS_INVALID_ARGUMENT;
	    } else {
		base->appeartime = (long)candidate;
	    }
	}
    }

    if (status == RENDERER_STATUS_OK && batch->status == RENDERER_STATUS_OK) {
	color = Life_color(other);
	status = Build_semantic_appearing_vertices(
	    x, y, count, color ? color : redRGBA, vertices);
    }
    if (status == RENDERER_STATUS_OK && batch->status == RENDERER_STATUS_OK)
	status = Reserve_semantic_appearing_vertices(batch, NELEM(vertices));
    if (status == RENDERER_STATUS_OK && batch->status == RENDERER_STATUS_OK) {
	memcpy(&batch->vertices[batch->vertex_count], vertices,
	       sizeof(vertices));
	batch->vertex_count += NELEM(vertices);
    } else if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_appearing_batch(batch, status);
    }
    if (standalone)
	Gui_paint_appearing_end();
}

void Gui_paint_appearing_end(void)
{
    SemanticAppearingBatch *batch = &semantic_appearing_batch;
    RendererStatus status;

    if (!batch->active)
	return;
    if (batch->status == RENDERER_STATUS_OK && batch->vertex_count != 0) {
	status = Renderer_set_blend(
	    batch->renderer, RENDERER_BLEND_ADDITIVE);
	if (Track_semantic_appearing_batch(batch, status)
	    == RENDERER_STATUS_OK) {
	    status = Renderer_draw_triangles(
		batch->renderer, NULL, batch->vertices,
		batch->vertex_count);
	    if (Track_semantic_appearing_batch(batch, status)
		== RENDERER_STATUS_OK) {
		status = Sdl_renderer_flush(
		    batch->sdl_renderer);
		(void)Track_semantic_appearing_batch(batch, status);
	    }
	}
    }
    free(batch->vertices);
    batch->vertices = NULL;
    batch->vertex_count = 0;
    batch->vertex_capacity = 0;
    batch->active = 0;
    batch->renderer = NULL;
    batch->sdl_renderer = NULL;
}

#ifdef XPILOT_APPEARING_BATCH_TEST_HOOKS
RendererStatus Sdlgui_test_paint_appearing_batch(
    const SdlguiTestAppearing *entries, size_t entry_count)
{
    SdlRenderer *renderer;
    size_t entry_index;

    renderer = Get_sdl_renderer();
    if (renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    if (entries == NULL && entry_count != 0) {
	return Sdl_renderer_track_frame_result(
	    renderer, RENDERER_STATUS_INVALID_ARGUMENT);
    }
    Gui_paint_appearing_begin();
    for (entry_index = 0; entry_index < entry_count; entry_index++) {
	Gui_paint_appearing(
	    entries[entry_index].x, entries[entry_index].y,
	    entries[entry_index].id, entries[entry_index].count);
    }
    Gui_paint_appearing_end();
    return Sdl_renderer_frame_result(renderer);
}
#endif

void Gui_paint_ecm(int x, int y, int size)
{
}

void Gui_paint_refuel(int x_0, int y_0, int x_1, int y_1)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_segment_geometry(
	x_0, y_0, x_1, y_1, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_stippled_geometry(
	&batch, &geometry, fuelColorRGBA, 4);
}

void Gui_paint_connector(int x_0, int y_0, int x_1, int y_1, int tractor)
{
    SemanticWorldLineBatch batch;
    SemanticWorldLineGeometry geometry;
    RendererStatus status;

    if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	return;
    status = Build_semantic_world_segment_geometry(
	x_0, y_0, x_1, y_1, &geometry);
    if (status != RENDERER_STATUS_OK) {
	(void)Track_semantic_world_line_batch(&batch, status);
	return;
    }
    (void)Paint_semantic_world_stippled_geometry(
	&batch, &geometry, connColorRGBA, tractor ? 2 : 4);
}

void Gui_paint_transporter(int x_0, int y_0, int x_1, int y_1)
{
}

void Gui_paint_all_connectors_begin(void)
{
}

void Gui_paint_ships_begin(void)
{
}

void Gui_paint_ships_end(void)
{
}

/*
 * Assume MAX_TEAMS is 10
 */
int Team_color(int team)
{
    switch (team) {
    case 0:	return team0ColorRGBA;
    case 1:	return team1ColorRGBA;
    case 2:	return team2ColorRGBA;
    case 3:	return team3ColorRGBA;
    case 4:	return team4ColorRGBA;
    case 5:	return team5ColorRGBA;
    case 6:	return team6ColorRGBA;
    case 7:	return team7ColorRGBA;
    case 8:	return team8ColorRGBA;
    case 9:	return team9ColorRGBA;
    default:    break;
    }
    return 0;
}

int Life_color_by_life(int life)
{
    int color;

    if (life > 2)
	color = manyLivesColorRGBA;
    else if (life == 2)
	color = twoLivesColorRGBA;
    else if (life == 1)
	color = oneLifeColorRGBA;
    else /* we catch all */
	color = zeroLivesColorRGBA;
    return color;
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
    Uint32 ship_color = whiteRGBA;

#ifndef NO_BLUE_TEAM
    if (BIT(Setup->mode, TEAM_PLAY)
	&& eyesId != id
	&& other != NULL
	&& eyeTeam == other->team) {
	/* Paint teammates and allies ships with last life in teamLWColorRGBA */
	if (BIT(Setup->mode, LIMITED_LIVES)
	    && (other->life == 0))
	    ship_color = teamLWColorRGBA;
	else
	    ship_color = teamShipColorRGBA;
    }

    if (eyes != NULL
	&& eyesId != id
	&& other != NULL
	&& eyes->alliance != ' '
	&& eyes->alliance == other->alliance) {
	/* Paint teammates and allies ships with last life in teamLWColorRGBA */
	if (BIT(Setup->mode, LIMITED_LIVES)
	    && (other->life == 0))
	    ship_color = teamLWColorRGBA;
	else
	    ship_color = teamShipColorRGBA;
    }

    if (Gui_is_my_tank(other))
	ship_color = blueRGBA;
#endif
    if (roundDelay > 0 && ship_color == whiteRGBA)
	ship_color = redRGBA;

    /* Check for team color */
    if (other && BIT(Setup->mode, TEAM_PLAY)) {
	int team_color = Team_color(other->team);
	if (team_color)
	    return team_color;
    }

    /* Vato color hack start, edited by mara & kps */
    if (BIT(Setup->mode, LIMITED_LIVES)) {
	/* Paint your ship in selfLWColorRGBA when on last life */
	if (eyes != NULL
	    && eyes->id == id
	    && eyes->life == 0) {
	    ship_color = selfLWColorRGBA;
	}

	/* Paint enemy ships with last life in enemyLWColorRGBA */
	if (eyes != NULL
	    && eyes->id != id
	    && other != NULL
	    && eyeTeam != other->team
	    && other->life == 0) {
	    ship_color = enemyLWColorRGBA;
	}
    }
    /* Vato color hack end */

    return ship_color;
}

typedef struct SemanticShipOutlineGeometry {
    RendererPoint2D points[MAX_SHIP_PTS2];
    size_t point_count;
    float width;
} SemanticShipOutlineGeometry;

static int Semantic_ship_outline_extent_preserved(
    float first, float second, float translated_first,
    float translated_second)
{
    /* Equal source coordinates are valid; only translation collapse is bad. */
    return first == second || translated_first != translated_second;
}

static RendererStatus Build_semantic_ship_outline_geometry(
    int x, int y, int dir, shipshape_t *ship, double width,
    SemanticShipOutlineGeometry *geometry)
{
    position_t first_position;
    position_t previous_position;
    float center_x;
    float center_y;
    int has_visible_edge;
    int point_index;

    if (geometry == NULL || ship == NULL
	|| dir < 0 || dir >= RES
	|| ship->num_points < MIN_SHIP_PTS
	|| ship->num_points > MAX_SHIP_PTS2
	|| !isfinite(width) || width <= 0.0 || width > (double)FLT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    geometry->width = (float)width;
    if (!isfinite(geometry->width) || geometry->width <= 0.0f)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    for (point_index = 0; point_index < ship->num_points; point_index++) {
	if (ship->pts[point_index] == NULL)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    center_x = (float)x;
    center_y = (float)y;
    if (!isfinite(center_x) || !isfinite(center_y))
	return RENDERER_STATUS_INVALID_ARGUMENT;

    has_visible_edge = 0;
    for (point_index = 0; point_index < ship->num_points; point_index++) {
	position_t position = Ship_get_point_position(
	    ship, point_index, dir);
	RendererPoint2D *point = &geometry->points[point_index];

	if (!isfinite(position.x) || !isfinite(position.y))
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	point->x = center_x + position.x;
	point->y = center_y + position.y;
	if (!isfinite(point->x) || !isfinite(point->y))
	    return RENDERER_STATUS_INVALID_ARGUMENT;

	if (point_index == 0) {
	    first_position = position;
	} else if (!Semantic_ship_outline_extent_preserved(
		previous_position.x, position.x,
		geometry->points[point_index - 1].x, point->x)
	    || !Semantic_ship_outline_extent_preserved(
		previous_position.y, position.y,
		geometry->points[point_index - 1].y, point->y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	if (point_index > 0
	    && (geometry->points[point_index - 1].x != point->x
		|| geometry->points[point_index - 1].y != point->y)) {
	    has_visible_edge = 1;
	}
	previous_position = position;
    }

    if (!Semantic_ship_outline_extent_preserved(
	    previous_position.x, first_position.x,
	    geometry->points[ship->num_points - 1].x,
	    geometry->points[0].x)
	|| !Semantic_ship_outline_extent_preserved(
	    previous_position.y, first_position.y,
	    geometry->points[ship->num_points - 1].y,
	    geometry->points[0].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    geometry->point_count = has_visible_edge
	? (size_t)ship->num_points : 0;
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_ship_outline(
    SemanticWorldLineBatch *batch,
    const SemanticShipOutlineGeometry *geometry, Uint32 packed_color,
    int stippled)
{
    RendererStatus operation_status;

    if (batch == NULL || geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->status != RENDERER_STATUS_OK)
	return batch->status;
    if (geometry->point_count == 0)
	return batch->status;

    operation_status = Renderer_set_blend(
	batch->renderer, RENDERER_BLEND_ALPHA);
    if (Track_semantic_world_line_batch(batch, operation_status)
	!= RENDERER_STATUS_OK) {
	return batch->status;
    }

    if (stippled) {
	operation_status = Renderer_stroke_stippled_path(
	    batch->renderer, geometry->points, geometry->point_count,
	    geometry->width, Renderer_color_from_rgba32(packed_color), 1,
	    3, UINT16_C(0xAAAA));
    } else {
	operation_status = Renderer_stroke_path(
	    batch->renderer, geometry->points, geometry->point_count,
	    geometry->width, Renderer_color_from_rgba32(packed_color), 1);
    }
    (void)Accept_semantic_world_line_command(batch, operation_status);
    return Finish_semantic_world_line_batch(batch);
}

static void Gui_paint_ship_name(int x, int y, other_t *other)
{
    int color = Life_color(other);

    /* TODO : do all name painting together, so we don't need
     * all theese setupPaint<foo> calls
     */
    if (shipNameColorRGBA) {
	if (!color)
	    color = shipNameColorRGBA;

	mapnprint(&mapfont, color, CENTER, DOWN,x,y - SHIP_SZ,maxCharsInNames,"%s",other->id_string);
    } else
	color = blueRGBA;

    if (instruments.showLivesByShip
	&& BIT(Setup->mode, LIMITED_LIVES)) {
	if (other->life < 1)
	    color = whiteRGBA;

	mapprint(&mapfont, color, LEFT, CENTER,x + SHIP_SZ,y,"%d", other->life);
    }
}

void Gui_paint_ship(int x, int y, int dir, int id, int cloak, int phased,
		    int shield, int deflector, int eshield)
{
    int color, img;
    shipshape_t *ship;
    other_t *other;
    SemanticWorldLineBatch batch;
    SemanticShipOutlineGeometry geometry;
    RendererStatus status;

    if (!(other = Other_by_id(id))) return;

    if (!(color = Gui_calculate_ship_color(id,other))) return;
    
    if ((!instruments.showShipShapes) && (self != NULL) && (self->id != id))
			ship = Default_ship();
    else if ((!instruments.showMyShipShape) && (self != NULL) && (self->id == id))
			ship = Default_ship();
		else
			ship = Ship_by_id(id);

    if (!texturedShips) {
	if (Begin_semantic_world_line_batch(&batch) != RENDERER_STATUS_OK)
	    return;
	status = Build_semantic_ship_outline_geometry(
	    x, y, dir, ship, shipLineWidth, &geometry);
	if (status != RENDERER_STATUS_OK) {
	    (void)Track_semantic_world_line_batch(&batch, status);
	    return;
	}
    }

    if (shield) {
    	Image_paint(IMG_SHIELD, x - 27, y - 27, 0, (color & 0xffffff00) + ((color & 0x000000ff)/2));
	if (!texturedShips
	    && Track_semantic_world_line_batch(
		&batch, RENDERER_STATUS_OK) != RENDERER_STATUS_OK)
	    return;
    }
	if (texturedShips) {
    	    if (BIT(Setup->mode, TEAM_PLAY)
			&& other != NULL
			&& self != NULL
			&& self->team == other->team) {
			img = IMG_SHIP_FRIEND;
    	    } else if (self != NULL && self->id != id) {
			img = IMG_SHIP_ENEMY;
    	    } else {
			img = IMG_SHIP_SELF;
    	    }
    	    if (cloak || phased) Image_paint_rotated(img, x, y, dir, (color & 0xffffff00) + ((color & 0x000000ff)/2));
	    else Image_paint_rotated(img, x, y, dir, color);
	} else {
	    status = Paint_semantic_ship_outline(
		&batch, &geometry, (Uint32)color, cloak || phased);
	    if (status != RENDERER_STATUS_OK)
		return;
	}
    if (self != NULL
    	&& self->id != id
    	&& other != NULL)
		Gui_paint_ship_name(x,y,other);
}

void Paint_score_objects(void)
{
    int		i, x, y;

    if (!get_alpha(scoreObjectColorRGBA))
	return;

    for (i = 0; i < MAX_SCORE_OBJECTS; i++) {
	score_object_t*	sobj = &score_objects[i];
	if (sobj->life_time > 0) {
	    if (loopsSlow % 3) {
	    	/* approximate font width to h =( */
		x = sobj->x * BLOCK_SZ + BLOCK_SZ/2;
		y = sobj->y * BLOCK_SZ + BLOCK_SZ/2;
  		if (wrap(&x, &y)) {
		    /*mapprint(&mapfont,scoreObjectColorRGBA,CENTER,CENTER,x,y,"%s",sobj->msg);*/
		    if (Ensure_cached_text(&mapfont, sobj->msg,
					   &score_object_texs[i])) {
			disp_text(&score_object_texs[i], scoreObjectColorRGBA,
				  CENTER, CENTER, x, y, false);
		    }
		}
	    }
	    sobj->life_time -= timePerFrame;
	    if (sobj->life_time <= 0.0) {
		sobj->life_time = 0.0;
		sobj->hud_msg_len = 0;
	    }
	} else {
	    if (score_object_texs[i].cache) free_string_texture(&score_object_texs[i]);
	}
    }
}

static int Semantic_select_float(int64_t value, float *result)
{
    if (result == NULL || (double)value < -(double)FLT_MAX
	|| (double)value > (double)FLT_MAX) {
	return 0;
    }
    *result = (float)value;
    return isfinite(*result);
}

static RendererStatus Build_semantic_select_geometry(
    const irec_t *bounds, RendererPoint2D points[4])
{
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;

    if (bounds == NULL || points == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    left = (int64_t)bounds->x;
    top = (int64_t)bounds->y;
    right = left + (int64_t)bounds->w;
    bottom = top + (int64_t)bounds->h;
    if (!Semantic_select_float(left, &points[0].x)
	|| !Semantic_select_float(top, &points[0].y)
	|| !Semantic_select_float(right, &points[1].x)
	|| !Semantic_select_float(top, &points[1].y)
	|| !Semantic_select_float(right, &points[2].x)
	|| !Semantic_select_float(bottom, &points[2].y)
	|| !Semantic_select_float(left, &points[3].x)
	|| !Semantic_select_float(bottom, &points[3].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

RendererStatus Paint_select(void)
{
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererPoint2D points[4];
    RendererStatus status;

    if (select_bounds == NULL)
	return RENDERER_STATUS_OK;

    sdl_renderer = Get_sdl_renderer();
    if (sdl_renderer == NULL)
	return RENDERER_STATUS_INVALID_STATE;
    status = Sdl_renderer_track_frame_result(
	sdl_renderer, RENDERER_STATUS_OK);
    if (status != RENDERER_STATUS_OK)
	return status;
    renderer = Sdl_renderer_frontend(sdl_renderer);
    if (renderer == NULL) {
	return Sdl_renderer_track_frame_result(
	    sdl_renderer, RENDERER_STATUS_INVALID_STATE);
    }

    status = Build_semantic_select_geometry(select_bounds, points);
    if (status != RENDERER_STATUS_OK)
	return Sdl_renderer_track_frame_result(sdl_renderer, status);
    status = Renderer_set_blend(renderer, RENDERER_BLEND_ALPHA);
    status = Sdl_renderer_track_frame_result(sdl_renderer, status);
    if (status != RENDERER_STATUS_OK)
	return status;
    status = Renderer_stroke_path(
	renderer, points, 4, 1.0f,
	Renderer_color_from_rgba32(selectionColorRGBA), 1);
    status = Sdl_renderer_track_frame_result(sdl_renderer, status);
    if (status != RENDERER_STATUS_OK)
	return status;
    status = Sdl_renderer_flush(sdl_renderer);
    return Sdl_renderer_track_frame_result(sdl_renderer, status);
}

void Paint_HUD_values(void)
{
    int x, y;

    if (!hudColorRGBA)
	return;

    x = draw_width - 20;
    /* Better make sure it's below the meters */
    y = draw_height
	- 9 * (MAX((unsigned int)meterHeight, gamefont.requested_height) + 6);

    HUDprint(&gamefont,hudColorRGBA,RIGHT,DOWN,x,y,"FPS: %.3f",clientFPS);
    HUDprint(&gamefont,hudColorRGBA,RIGHT,DOWN,x,y-20,"CL.LAG : %.1f ms", clData.clientLag);
}

typedef struct SemanticHudBatch {
    SdlRenderer *sdl_renderer;
    Renderer *renderer;
    RendererStatus status;
    int has_accepted_commands;
} SemanticHudBatch;

typedef struct SemanticHudPointerGeometry {
    RendererPoint2D speed[2];
    RendererPoint2D direction[2];
    int draw_speed;
    int draw_direction;
} SemanticHudPointerGeometry;

typedef struct SemanticHudFrameGeometry {
    RendererPoint2D horizontal[2][2];
    RendererPoint2D vertical[2][2];
} SemanticHudFrameGeometry;

#define SEMANTIC_HUD_RADAR_RING_POINTS 16
#define SEMANTIC_HUD_RADAR_FILL_VERTICES \
    ((SEMANTIC_HUD_RADAR_RING_POINTS - 2) * 3)

typedef struct SemanticHudRadarDotGeometry {
    RendererVertex2D fill[SEMANTIC_HUD_RADAR_FILL_VERTICES];
    RendererPoint2D outline[SEMANTIC_HUD_RADAR_RING_POINTS];
    size_t fill_count;
    size_t outline_count;
    RendererColor color;
} SemanticHudRadarDotGeometry;

typedef struct SemanticHudRadarPass {
    double scale;
    double xlimit;
    double ylimit;
    double xfactor;
    double yfactor;
    int width;
    int height;
    int size;
} SemanticHudRadarPass;

typedef struct SemanticHudRadarScene {
    SemanticHudRadarPass passes[2];
    RendererPoint2D ball_scan[SEMANTIC_HUD_RADAR_RING_POINTS];
    RendererPoint2D cover_scan[SEMANTIC_HUD_RADAR_RING_POINTS];
    int radar_enabled;
    int draw_ball_scan;
    int draw_cover_scan;
} SemanticHudRadarScene;

typedef struct SemanticHudFuelGaugeGeometry {
    RendererPoint2D outline[4];
    float fill_x;
    float fill_y;
    float fill_width;
    float fill_height;
    int draw_fill;
} SemanticHudFuelGaugeGeometry;

typedef struct SemanticMeterGeometry {
    float fill_x;
    float fill_y;
    float fill_width;
    float fill_height;
    RendererPoint2D border[4];
    RendererPoint2D ticks[5][2];
    int draw_fill;
    int text_x;
    int text_y;
} SemanticMeterGeometry;

static RendererStatus Begin_semantic_hud_batch(SemanticHudBatch *batch)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    batch->sdl_renderer = Get_sdl_renderer();
    batch->renderer = NULL;
    batch->status = RENDERER_STATUS_INVALID_STATE;
    batch->has_accepted_commands = 0;
    if (batch->sdl_renderer == NULL)
	return batch->status;
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, RENDERER_STATUS_OK);
    if (batch->status != RENDERER_STATUS_OK)
	return batch->status;
    batch->renderer = Sdl_renderer_frontend(batch->sdl_renderer);
    if (batch->renderer == NULL) {
	batch->status = Sdl_renderer_track_frame_result(
	    batch->sdl_renderer, RENDERER_STATUS_INVALID_STATE);
    }
    return batch->status;
}

static RendererStatus Track_semantic_hud_batch(
    SemanticHudBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (batch->sdl_renderer == NULL) {
	batch->status = status;
	return batch->status;
    }
    batch->status = Sdl_renderer_track_frame_result(
	batch->sdl_renderer, status);
    return batch->status;
}

static RendererStatus Accept_semantic_hud_command(
    SemanticHudBatch *batch, RendererStatus status)
{
    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (status == RENDERER_STATUS_OK)
	batch->has_accepted_commands = 1;
    return Track_semantic_hud_batch(batch, status);
}

static RendererStatus Finish_semantic_hud_batch(
    SemanticHudBatch *batch)
{
    RendererStatus flush_status;

    if (batch == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (!batch->has_accepted_commands || batch->sdl_renderer == NULL)
	return batch->status;
    flush_status = Sdl_renderer_flush(
	batch->sdl_renderer);
    batch->has_accepted_commands = 0;
    return Track_semantic_hud_batch(batch, flush_status);
}

static int Semantic_hud_pointer_point(
    double x, double y, RendererPoint2D *point)
{
    int integer_x;
    int integer_y;

    if (point == NULL || !isfinite(x) || !isfinite(y)
	|| x < (double)INT_MIN || x > (double)INT_MAX
	|| y < (double)INT_MIN || y > (double)INT_MAX) {
	return 0;
    }
    integer_x = (int)x;
    integer_y = (int)y;
    point->x = (float)integer_x;
    point->y = (float)integer_y;
    return isfinite(point->x) && isfinite(point->y);
}

static RendererStatus Build_semantic_hud_pointer_geometry(
    int draw_speed, int draw_direction,
    SemanticHudPointerGeometry *geometry)
{
    double center_x;
    double center_y;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    geometry->draw_speed = draw_speed;
    geometry->draw_direction = draw_direction;
    center_x = (double)(draw_width / 2);
    center_y = (double)(draw_height / 2);

    if (draw_speed
	&& (!Semantic_hud_pointer_point(
		center_x, center_y, &geometry->speed[0])
	    || !Semantic_hud_pointer_point(
		center_x - ptr_move_fact * (double)selfVel.x,
		center_y + ptr_move_fact * (double)selfVel.y,
		&geometry->speed[1]))) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    if (draw_direction) {
	double cosine;
	double sine;

	if (heading < 0 || heading >= TABLE_SIZE)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	cosine = tcos(heading);
	sine = tsin(heading);
	if (!isfinite(cosine) || !isfinite(sine)
	    || !Semantic_hud_pointer_point(
		center_x + 85.0 * cosine,
		center_y - 85.0 * sine,
		&geometry->direction[0])
	    || !Semantic_hud_pointer_point(
		center_x + 100.0 * cosine,
		center_y - 100.0 * sine,
		&geometry->direction[1])) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_hud_pointers(void)
{
    SemanticHudBatch batch;
    SemanticHudPointerGeometry geometry;
    RendererStatus operation_status;
    int draw_speed;
    int draw_direction;

    draw_speed = ptr_move_fact != 0.0
	&& selfVisible
	&& (selfVel.x != 0 || selfVel.y != 0);
    draw_direction = selfVisible && dirPtrColorRGBA;
    if (!draw_speed && !draw_direction)
	return RENDERER_STATUS_OK;

    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_pointer_geometry(
	draw_speed, draw_direction, &geometry);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);
    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ADDITIVE);
    if (Track_semantic_hud_batch(&batch, operation_status)
	!= RENDERER_STATUS_OK) {
	return batch.status;
    }

    if (geometry.draw_speed) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, geometry.speed, 2, 1.0f,
	    Renderer_color_from_rgba32(hudColorRGBA), 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    if (batch.status == RENDERER_STATUS_OK && geometry.draw_direction) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, geometry.direction, 2, 1.0f,
	    Renderer_color_from_rgba32(dirPtrColorRGBA), 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    return Finish_semantic_hud_batch(&batch);
}

#ifdef XPILOT_SDLGUI_TEST_HOOKS
RendererStatus Sdlgui_test_paint_hud_pointers(void)
{
    return Paint_semantic_hud_pointers();
}
#endif

static int Semantic_hud_integer_float(int64_t value, float *result)
{
    if (result == NULL || (double)value < -(double)FLT_MAX
	|| (double)value > (double)FLT_MAX) {
	return 0;
    }
    *result = (float)value;
    return isfinite(*result);
}

static RendererStatus Build_semantic_hud_frame_geometry(
    int hud_pos_x, int hud_pos_y, SemanticHudFrameGeometry *geometry)
{
    int64_t left;
    int64_t right;
    int64_t top;
    int64_t bottom;
    int64_t horizontal_top;
    int64_t horizontal_bottom;
    int64_t vertical_left;
    int64_t vertical_right;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    left = (int64_t)hud_pos_x - (int64_t)hudSize;
    right = (int64_t)hud_pos_x + (int64_t)hudSize;
    top = (int64_t)hud_pos_y - (int64_t)hudSize;
    bottom = (int64_t)hud_pos_y + (int64_t)hudSize;
    horizontal_top = top + (int64_t)HUD_OFFSET;
    horizontal_bottom = bottom - (int64_t)HUD_OFFSET;
    vertical_left = left + (int64_t)HUD_OFFSET;
    vertical_right = right - (int64_t)HUD_OFFSET;

    if (!Semantic_hud_integer_float(
	    left, &geometry->horizontal[0][0].x)
	|| !Semantic_hud_integer_float(
	    horizontal_top, &geometry->horizontal[0][0].y)
	|| !Semantic_hud_integer_float(
	    right, &geometry->horizontal[0][1].x)
	|| !Semantic_hud_integer_float(
	    horizontal_top, &geometry->horizontal[0][1].y)
	|| !Semantic_hud_integer_float(
	    left, &geometry->horizontal[1][0].x)
	|| !Semantic_hud_integer_float(
	    horizontal_bottom, &geometry->horizontal[1][0].y)
	|| !Semantic_hud_integer_float(
	    right, &geometry->horizontal[1][1].x)
	|| !Semantic_hud_integer_float(
	    horizontal_bottom, &geometry->horizontal[1][1].y)
	|| !Semantic_hud_integer_float(
	    vertical_left, &geometry->vertical[0][0].x)
	|| !Semantic_hud_integer_float(
	    top, &geometry->vertical[0][0].y)
	|| !Semantic_hud_integer_float(
	    vertical_left, &geometry->vertical[0][1].x)
	|| !Semantic_hud_integer_float(
	    bottom, &geometry->vertical[0][1].y)
	|| !Semantic_hud_integer_float(
	    vertical_right, &geometry->vertical[1][0].x)
	|| !Semantic_hud_integer_float(
	    top, &geometry->vertical[1][0].y)
	|| !Semantic_hud_integer_float(
	    vertical_right, &geometry->vertical[1][1].x)
	|| !Semantic_hud_integer_float(
	    bottom, &geometry->vertical[1][1].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_hud_frame(
    int hud_pos_x, int hud_pos_y)
{
    SemanticHudBatch batch;
    SemanticHudFrameGeometry geometry;
    RendererColor color;
    RendererStatus operation_status;
    int path_index;

    if (!hudHLineColorRGBA && !hudVLineColorRGBA)
	return RENDERER_STATUS_OK;
    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_frame_geometry(
	hud_pos_x, hud_pos_y, &geometry);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);

    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ADDITIVE);
    Track_semantic_hud_batch(&batch, operation_status);

    color = Renderer_color_from_rgba32(hudHLineColorRGBA);
    for (path_index = 0;
	 batch.status == RENDERER_STATUS_OK
	     && hudHLineColorRGBA && path_index < 2;
	 path_index++) {
	operation_status = Renderer_stroke_stippled_path(
	    batch.renderer, geometry.horizontal[path_index], 2, 1.0f,
	    color, 0, 4, UINT16_C(0xAAAA));
	Accept_semantic_hud_command(&batch, operation_status);
    }

    color = Renderer_color_from_rgba32(hudVLineColorRGBA);
    for (path_index = 0;
	 batch.status == RENDERER_STATUS_OK
	     && hudVLineColorRGBA && path_index < 2;
	 path_index++) {
	operation_status = Renderer_stroke_stippled_path(
	    batch.renderer, geometry.vertical[path_index], 2, 1.0f,
	    color, 0, 4, UINT16_C(0xAAAA));
	Accept_semantic_hud_command(&batch, operation_status);
    }
    return Finish_semantic_hud_batch(&batch);
}

static RendererStatus Build_semantic_meter_geometry(
    int xoff, int y, int value, int maximum,
    SemanticMeterGeometry *geometry, int *x_alignment)
{
    static const int tick_top_offsets[5] = {-4, -4, -3, -1, -1};
    static const int tick_bottom_offsets[5] = {4, 4, 3, 1, 1};
    int64_t tick_x_offsets[5];
    int64_t divisor;
    int64_t product;
    int64_t width;
    int64_t fill_left;
    int64_t fill_height;
    int64_t x;
    int64_t xstr;
    int64_t text_y;
    int64_t border_right;
    int64_t border_bottom;
    int tick_index;

    if (geometry == NULL || x_alignment == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    if (xoff >= 0) {
	x = (int64_t)xoff;
	xstr = x + (int64_t)meterWidth + INT64_C(5);
	*x_alignment = LEFT;
    } else {
	x = (int64_t)draw_width
	    - ((int64_t)meterWidth - (int64_t)xoff);
	xstr = x - INT64_C(5);
	*x_alignment = RIGHT;
    }
    text_y = (int64_t)draw_height - (int64_t)y
	- (int64_t)(meterHeight / 2);
    if (xstr < INT_MIN || xstr > INT_MAX
	|| text_y < INT_MIN || text_y > INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    geometry->text_x = (int)xstr;
    geometry->text_y = (int)text_y;

    divisor = maximum != 0 ? (int64_t)maximum : INT64_C(1);
    product = (int64_t)meterWidth * (int64_t)value;
    if (product == INT64_MIN && divisor == INT64_C(-1))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    width = product / divisor;
    fill_left = x;
    if (width < 0) {
	if (width == INT64_MIN || fill_left < INT64_MIN - width)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	fill_left += width;
	width = -width;
    }
    fill_height = (int64_t)meterHeight - INT64_C(1);
    geometry->draw_fill = width != 0 && fill_height > 0;
    if (geometry->draw_fill
	&& (!Semantic_hud_integer_float(fill_left, &geometry->fill_x)
	    || !Semantic_hud_integer_float((int64_t)y, &geometry->fill_y)
	    || !Semantic_hud_integer_float(width, &geometry->fill_width)
	    || !Semantic_hud_integer_float(fill_height,
				     &geometry->fill_height))) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    border_right = x + (int64_t)meterWidth;
    border_bottom = (int64_t)y + (int64_t)meterHeight;
    if (!Semantic_hud_integer_float(x, &geometry->border[0].x)
	|| !Semantic_hud_integer_float((int64_t)y, &geometry->border[0].y)
	|| !Semantic_hud_integer_float(x, &geometry->border[1].x)
	|| !Semantic_hud_integer_float(border_bottom, &geometry->border[1].y)
	|| !Semantic_hud_integer_float(border_right, &geometry->border[2].x)
	|| !Semantic_hud_integer_float(border_bottom, &geometry->border[2].y)
	|| !Semantic_hud_integer_float(border_right, &geometry->border[3].x)
	|| !Semantic_hud_integer_float((int64_t)y,
					 &geometry->border[3].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    tick_x_offsets[0] = 0;
    tick_x_offsets[1] = (int64_t)meterWidth;
    tick_x_offsets[2] = (int64_t)meterWidth / INT64_C(2);
    tick_x_offsets[3] = (int64_t)meterWidth / INT64_C(4);
    tick_x_offsets[4] = (INT64_C(3) * (int64_t)meterWidth)
	/ INT64_C(4);
    for (tick_index = 0; tick_index < 5; tick_index++) {
	int64_t tick_x = x + tick_x_offsets[tick_index];
	int64_t tick_top = (int64_t)y + tick_top_offsets[tick_index];
	int64_t tick_bottom = border_bottom
	    + tick_bottom_offsets[tick_index];

	if (!Semantic_hud_integer_float(
		tick_x, &geometry->ticks[tick_index][0].x)
	    || !Semantic_hud_integer_float(
		tick_top, &geometry->ticks[tick_index][0].y)
	    || !Semantic_hud_integer_float(
		tick_x, &geometry->ticks[tick_index][1].x)
	    || !Semantic_hud_integer_float(
		tick_bottom, &geometry->ticks[tick_index][1].y)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    return RENDERER_STATUS_OK;
}

static void Paint_meter(int xoff, int y, string_tex_t *tex, int val, int max,
			int meter_color)
{
    SemanticHudBatch batch;
    SemanticMeterGeometry geometry;
    RendererColor border_color;
    RendererStatus operation_status;
    int x_alignment;
    int color;
    int tick_index;

    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return;
    operation_status = Build_semantic_meter_geometry(
	xoff, y, val, max, &geometry, &x_alignment);
    if (operation_status != RENDERER_STATUS_OK) {
	Track_semantic_hud_batch(&batch, operation_status);
	return;
    }
    if (Track_semantic_hud_batch(
	    &batch,
	    Renderer_set_blend(
		batch.renderer, RENDERER_BLEND_ALPHA))
	!= RENDERER_STATUS_OK) {
	return;
    }

    if (geometry.draw_fill) {
	operation_status = Renderer_fill_rect(
	    batch.renderer,
	    geometry.fill_x, geometry.fill_y,
	    geometry.fill_width, geometry.fill_height,
	    Renderer_color_from_rgba32((Uint32)meter_color));
	Accept_semantic_hud_command(&batch, operation_status);
    }

    /* meterBorderColorRGBA = 0 obviously means no meter borders are drawn */
    if (batch.status == RENDERER_STATUS_OK && meterBorderColorRGBA) {
	border_color = Renderer_color_from_rgba32(meterBorderColorRGBA);
	operation_status = Renderer_stroke_path(
	    batch.renderer, geometry.border, 4, 1.0f,
	    border_color, 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    for (tick_index = 0;
	 batch.status == RENDERER_STATUS_OK
	 && meterBorderColorRGBA && tick_index < 5;
	 tick_index++) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, geometry.ticks[tick_index], 2, 1.0f,
	    border_color, 0);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    if (Finish_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return;

    color = meterBorderColorRGBA ? meterBorderColorRGBA : meter_color;

    if (tex->cache != NULL) {
	operation_status = disp_text(
	    tex, color, x_alignment, CENTER,
	    geometry.text_x, geometry.text_y, true);
	if (operation_status != RENDERER_STATUS_OK)
	    Track_semantic_hud_batch(&batch, operation_status);
    }
}

void Paint_meters(void)
{
    int spacing = MAX((unsigned int)meterHeight, gamefont.requested_height) + 6;
    int y = spacing, color;

    (void)Ensure_cached_text(&gamefont, "Fuel", &meter_texs[0]);
    (void)Ensure_cached_text(&gamefont, "Power", &meter_texs[1]);
    (void)Ensure_cached_text(&gamefont, "Turnspeed", &meter_texs[2]);
    (void)Ensure_cached_text(&gamefont, "Packet", &meter_texs[3]);
    (void)Ensure_cached_text(&gamefont, "Loss", &meter_texs[4]);
    (void)Ensure_cached_text(&gamefont, "Drop", &meter_texs[5]);
    (void)Ensure_cached_text(&gamefont, "Lag", &meter_texs[6]);
    (void)Ensure_cached_text(&gamefont, "Thrust Left", &meter_texs[7]);
    (void)Ensure_cached_text(&gamefont, "Shields Left", &meter_texs[8]);
    (void)Ensure_cached_text(&gamefont, "Phasing left", &meter_texs[9]);
    (void)Ensure_cached_text(&gamefont, "Self destructing",
			     &meter_texs[10]);
    (void)Ensure_cached_text(&gamefont, "SHUTDOWN", &meter_texs[11]);

    if (fuelMeterColorRGBA)
	Paint_meter(-10, y += spacing, &meter_texs[0],
		    (int)fuelSum, (int)fuelMax, fuelMeterColorRGBA);

    if (powerMeterColorRGBA)
	color = powerMeterColorRGBA;
    else if (controlTime > 0.0)
	color = temporaryMeterColorRGBA;
    else
	color = 0;

    if (color)
	Paint_meter(-10, y += spacing, &meter_texs[1],
		    (int)displayedPower, (int)MAX_PLAYER_POWER, color);

    if (turnSpeedMeterColorRGBA)
	color = turnSpeedMeterColorRGBA;
    else if (controlTime > 0.0)
	color = temporaryMeterColorRGBA;
    else
	color = 0;

    if (color)
	Paint_meter(-10, y += spacing, &meter_texs[2],
		    (int)displayedTurnspeed, (int)MAX_PLAYER_TURNSPEED, color);

    if (controlTime > 0.0) {
	controlTime -= timePerFrame;
	if (controlTime <= 0.0)
	    controlTime = 0.0;
    }

    if (packetSizeMeterColorRGBA)
	Paint_meter(-10, y += spacing, &meter_texs[3],
		   (packet_size >= 4096) ? 4096 : packet_size, 4096,
		    packetSizeMeterColorRGBA);
    if (packetLossMeterColorRGBA)
	Paint_meter(-10, y += spacing, &meter_texs[4], packet_loss, FPS,
		    packetLossMeterColorRGBA);
    if (packetDropMeterColorRGBA)
	Paint_meter(-10, y += spacing, &meter_texs[5], packet_drop, FPS,
		    packetDropMeterColorRGBA);
    if (packetLagMeterColorRGBA)
	Paint_meter(-10, y += spacing, &meter_texs[6], MIN(packet_lag, 1 * FPS), 1 * FPS,
		    packetLagMeterColorRGBA);

    if (temporaryMeterColorRGBA) {
	if (thrusttime >= 0 && thrusttimemax > 0)
	    Paint_meter((ext_view_width-300)/2 -32, 2*ext_view_height/3,
			&meter_texs[7],
			(thrusttime >= thrusttimemax
			 ? thrusttimemax : thrusttime),
			thrusttimemax, temporaryMeterColorRGBA);

	if (shieldtime >= 0 && shieldtimemax > 0)
	    Paint_meter((ext_view_width-300)/2 -32, 2*ext_view_height/3 + spacing,
			&meter_texs[8],
			(shieldtime >= shieldtimemax
			 ? shieldtimemax : shieldtime),
			shieldtimemax, temporaryMeterColorRGBA);

	if (phasingtime >= 0 && phasingtimemax > 0)
	    Paint_meter((ext_view_width-300)/2 -32, 2*ext_view_height/3 + 2*spacing,
			&meter_texs[9],
			(phasingtime >= phasingtimemax
			 ? phasingtimemax : phasingtime),
			phasingtimemax, temporaryMeterColorRGBA);

	if (destruct > 0)
	    Paint_meter((ext_view_width-300)/2 -32, 2*ext_view_height/3 + 3*spacing,
			&meter_texs[10], destruct, (int)SELF_DESTRUCT_DELAY,
			temporaryMeterColorRGBA);

	if (shutdown_count >= 0)
	    Paint_meter((ext_view_width-300)/2 -32, 2*ext_view_height/3 + 4*spacing,
			&meter_texs[11], shutdown_count, shutdown_delay,
			temporaryMeterColorRGBA);
    }
}

static RendererStatus Paint_lock(int hud_pos_x, int hud_pos_y)
{
    other_t *target;
    const int BORDER = 2;

    if ((target = Other_by_id(lock_id)) == NULL)
	return RENDERER_STATUS_OK;

    if (hudColorRGBA) {
	int color = Life_color(target);

	if (!color)
	    color = hudColorRGBA;

	return HUDnprint(
	    &gamefont, color, CENTER, CENTER, hud_pos_x,
	    hud_pos_y - (-hudSize + HUD_OFFSET - BORDER),
	    strlen(target->id_string), "%s", target->id_string);
    }
    return RENDERER_STATUS_OK;
}

static int Semantic_hud_radar_dot_visible(
    Uint32 color, int shape, int size)
{
    return color != 0 && shape >= 2 && shape <= 7 && size != 0;
}

static int Semantic_hud_radar_float(double value, float *result)
{
    if (result == NULL || !isfinite(value)
	|| value < -(double)FLT_MAX || value > (double)FLT_MAX) {
	return 0;
    }
    *result = (float)value;
    return isfinite(*result);
}

static int Semantic_hud_radar_point(
    double x, double y, RendererPoint2D *point)
{
    return point != NULL
	&& Semantic_hud_radar_float(x, &point->x)
	&& Semantic_hud_radar_float(y, &point->y);
}

static void Set_semantic_hud_radar_vertex(
    RendererVertex2D *vertex, RendererPoint2D point, RendererColor color)
{
    vertex->x = point.x;
    vertex->y = point.y;
    vertex->u = 0.0f;
    vertex->v = 0.0f;
    vertex->color = color;
}

static RendererStatus Build_semantic_hud_radar_ring(
    int64_t x, int64_t y, int radius,
    RendererPoint2D points[SEMANTIC_HUD_RADAR_RING_POINTS])
{
    int point_index;

    if (points == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    for (point_index = 0;
	 point_index < SEMANTIC_HUD_RADAR_RING_POINTS;
	 point_index++) {
	int table_index = point_index * TABLE_SIZE
	    / SEMANTIC_HUD_RADAR_RING_POINTS;
	double cosine = tcos(table_index);
	double sine = tsin(table_index);

	if (!isfinite(cosine) || !isfinite(sine)
	    || !Semantic_hud_radar_point(
		(double)x + cosine * (double)radius,
		(double)y + sine * (double)radius,
		&points[point_index])) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Build_semantic_hud_radar_dot_geometry(
    int64_t x, int64_t y, Uint32 packed_color, int shape, int size,
    SemanticHudRadarDotGeometry *geometry)
{
    RendererPoint2D points[4];
    RendererStatus status;
    size_t triangle_index;

    if (geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    memset(geometry, 0, sizeof(*geometry));
    if (!Semantic_hud_radar_dot_visible(packed_color, shape, size))
	return RENDERER_STATUS_OK;
    geometry->color = Renderer_color_from_rgba32(packed_color);

    if (shape == 2 || shape == 3) {
	status = Build_semantic_hud_radar_ring(
	    x, y, size, geometry->outline);
	if (status != RENDERER_STATUS_OK)
	    return status;
	if (shape == 3) {
	    geometry->outline_count = SEMANTIC_HUD_RADAR_RING_POINTS;
	    return RENDERER_STATUS_OK;
	}
	/* GL_POLYGON's convex ring is reproduced by a fan rooted at point zero. */
	for (triangle_index = 1;
	     triangle_index + 1 < SEMANTIC_HUD_RADAR_RING_POINTS;
	     triangle_index++) {
	    size_t vertex_index = geometry->fill_count;

	    Set_semantic_hud_radar_vertex(
		&geometry->fill[vertex_index], geometry->outline[0],
		geometry->color);
	    Set_semantic_hud_radar_vertex(
		&geometry->fill[vertex_index + 1],
		geometry->outline[triangle_index], geometry->color);
	    Set_semantic_hud_radar_vertex(
		&geometry->fill[vertex_index + 2],
		geometry->outline[triangle_index + 1], geometry->color);
	    geometry->fill_count += 3;
	}
	return RENDERER_STATUS_OK;
    }

    if (shape == 4 || shape == 5) {
	if (!Semantic_hud_radar_point(
		(double)(x - (int64_t)size),
		(double)(y - (int64_t)size), &points[0])
	    || !Semantic_hud_radar_point(
		(double)(x - (int64_t)size),
		(double)(y + (int64_t)size), &points[1])
	    || !Semantic_hud_radar_point(
		(double)(x + (int64_t)size),
		(double)(y + (int64_t)size), &points[2])
	    || !Semantic_hud_radar_point(
		(double)(x + (int64_t)size),
		(double)(y - (int64_t)size), &points[3])) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	if (shape == 5) {
	    memcpy(geometry->outline, points, sizeof(points));
	    geometry->outline_count = 4;
	    return RENDERER_STATUS_OK;
	}
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[0], points[0], geometry->color);
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[1], points[1], geometry->color);
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[2], points[2], geometry->color);
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[3], points[0], geometry->color);
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[4], points[2], geometry->color);
	Set_semantic_hud_radar_vertex(
	    &geometry->fill[5], points[3], geometry->color);
	geometry->fill_count = 6;
	return RENDERER_STATUS_OK;
    }

    if (!Semantic_hud_radar_point(
	    (double)(x - (int64_t)size),
	    (double)(y + (int64_t)size), &points[0])
	|| !Semantic_hud_radar_point(
	    (double)x, (double)(y - (int64_t)size), &points[1])
	|| !Semantic_hud_radar_point(
	    (double)(x + (int64_t)size),
	    (double)(y + (int64_t)size), &points[2])) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (shape == 7) {
	memcpy(geometry->outline, points, 3 * sizeof(points[0]));
	geometry->outline_count = 3;
	return RENDERER_STATUS_OK;
    }
    Set_semantic_hud_radar_vertex(
	&geometry->fill[0], points[0], geometry->color);
    Set_semantic_hud_radar_vertex(
	&geometry->fill[1], points[1], geometry->color);
    Set_semantic_hud_radar_vertex(
	&geometry->fill[2], points[2], geometry->color);
    geometry->fill_count = 3;
    return RENDERER_STATUS_OK;
}

static RendererStatus Submit_semantic_hud_radar_dot(
    SemanticHudBatch *batch, const SemanticHudRadarDotGeometry *geometry)
{
    RendererStatus status;

    if (batch == NULL || geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    if (geometry->fill_count != 0) {
	status = Renderer_draw_triangles(
	    batch->renderer, NULL, geometry->fill, geometry->fill_count);
	return Accept_semantic_hud_command(batch, status);
    }
    if (geometry->outline_count != 0) {
	status = Renderer_stroke_path(
	    batch->renderer, geometry->outline, geometry->outline_count,
	    1.0f, geometry->color, 1);
	return Accept_semantic_hud_command(batch, status);
    }
    return batch->status;
}

static int Semantic_hud_radar_truncated_int(double value, int *result)
{
    if (result == NULL || !isfinite(value)
	|| value < (double)INT_MIN || value > (double)INT_MAX) {
	return 0;
    }
    *result = (int)value;
    return 1;
}

static RendererStatus Build_semantic_hud_radar_pass(
    double scale, double xlimit, double ylimit, int size,
    SemanticHudRadarPass *pass)
{
    double scaled_width;
    double scaled_height;

    if (pass == NULL || Setup == NULL
	|| Setup->width <= 0 || Setup->height <= 0 || RadarHeight == 0
	|| !isfinite(scale) || scale <= 0.0
	|| !isfinite(xlimit) || xlimit < 0.0
	|| !isfinite(ylimit) || ylimit < 0.0) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    scaled_width = scale * 256.0;
    scaled_height = scale * (double)RadarHeight;
    if (!Semantic_hud_radar_truncated_int(scaled_width, &pass->width)
	|| !Semantic_hud_radar_truncated_int(
	    scaled_height, &pass->height)
	|| pass->width <= 0 || pass->height <= 0) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    pass->scale = scale;
    pass->xlimit = xlimit;
    pass->ylimit = ylimit;
    pass->xfactor = (double)pass->width / (double)Setup->width;
    pass->yfactor = (double)pass->height / (double)Setup->height;
    pass->size = size;
    if (!isfinite(pass->xfactor) || !isfinite(pass->yfactor))
	return RENDERER_STATUS_INVALID_ARGUMENT;
    return RENDERER_STATUS_OK;
}

static int Semantic_hud_radar_half_size(int size)
{
    int half = size / 2;

    /* Match GCC's arithmetic right shift used by the legacy object marker. */
    if (size < 0 && size % 2 != 0)
	half--;
    return half;
}

static RendererStatus Build_semantic_hud_radar_entry(
    const SemanticHudRadarPass *pass, const radar_t *radar,
    SemanticHudRadarDotGeometry *geometry)
{
    double relative_x;
    double relative_y;
    int integer_x;
    int integer_y;
    int64_t x;
    int64_t y;
    int64_t center_x;
    int64_t center_y;
    Uint32 color;
    int shape;
    int size;

    if (pass == NULL || radar == NULL || geometry == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    memset(geometry, 0, sizeof(*geometry));
    relative_x = (double)radar->x * pass->scale
	- (double)((int64_t)world.x + ext_view_width / 2)
	    * pass->xfactor;
    relative_y = (double)radar->y * pass->scale
	- (double)((int64_t)world.y + ext_view_height / 2)
	    * pass->yfactor;
    if (!Semantic_hud_radar_truncated_int(relative_x, &integer_x)
	|| !Semantic_hud_radar_truncated_int(relative_y, &integer_y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    x = integer_x;
    y = integer_y;
    /* Preserve the legacy one-period wrap and its strict half-map bounds. */
    if (x < -(int64_t)pass->width / 2)
	x += pass->width;
    else if (x > (int64_t)pass->width / 2)
	x -= pass->width;
    if (y < -(int64_t)pass->height / 2)
	y += pass->height;
    else if (y > (int64_t)pass->height / 2)
	y -= pass->height;

    if ((double)x <= pass->xlimit && (double)x >= -pass->xlimit
	&& (double)y <= pass->ylimit && (double)y >= -pass->ylimit) {
	return RENDERER_STATUS_OK;
    }

    if (radar->type == RadarEnemy) {
	color = hudRadarEnemyColorRGBA;
	shape = hudRadarEnemyShape;
    } else {
	color = hudRadarOtherColorRGBA;
	shape = hudRadarOtherShape;
    }
    size = pass->size;
    if (radar->size == 0) {
	size = Semantic_hud_radar_half_size(size);
	if (hudRadarObjectColorRGBA)
	    color = hudRadarObjectColorRGBA;
	if (hudRadarObjectShape)
	    shape = hudRadarObjectShape;
    }

    center_x = (int64_t)(draw_width / 2);
    center_y = (int64_t)(draw_height / 2);
    return Build_semantic_hud_radar_dot_geometry(
	x + center_x, -y + center_y, color, shape, size, geometry);
}

static RendererStatus Validate_semantic_hud_radar_pass(
    const SemanticHudRadarPass *pass)
{
    SemanticHudRadarDotGeometry geometry;
    int radar_index;

    if (pass == NULL || num_radar < 0
	|| (num_radar > 0 && radar_ptr == NULL)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    /* Validate every derived dot before the batch accepts its first command. */
    for (radar_index = 0; radar_index < num_radar; radar_index++) {
	RendererStatus status = Build_semantic_hud_radar_entry(
	    pass, &radar_ptr[radar_index], &geometry);

	if (status != RENDERER_STATUS_OK)
	    return status;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Submit_semantic_hud_radar_pass(
    SemanticHudBatch *batch, const SemanticHudRadarPass *pass)
{
    SemanticHudRadarDotGeometry geometry;
    int radar_index;

    if (batch == NULL || pass == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    for (radar_index = 0;
	 radar_index < num_radar && batch->status == RENDERER_STATUS_OK;
	 radar_index++) {
	RendererStatus status = Build_semantic_hud_radar_entry(
	    pass, &radar_ptr[radar_index], &geometry);

	if (status != RENDERER_STATUS_OK) {
	    Track_semantic_hud_batch(batch, status);
	    break;
	}
	Submit_semantic_hud_radar_dot(batch, &geometry);
    }
    return batch->status;
}

static RendererStatus Build_semantic_hud_radar_scene(
    int radar_enabled, int draw_ball_scan, int draw_cover_scan,
    SemanticHudRadarScene *scene)
{
    double map_scale;
    double first_xlimit_value;
    double first_ylimit_value;
    double second_scale;
    double second_xlimit;
    double second_ylimit;
    int first_xlimit;
    int first_ylimit;
    int ball_radius = 0;
    int cover_radius = 0;
    RendererStatus status;

    if (scene == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;
    memset(scene, 0, sizeof(*scene));
    scene->radar_enabled = radar_enabled;
    scene->draw_ball_scan = draw_ball_scan;
    scene->draw_cover_scan = draw_cover_scan;

    if (radar_enabled) {
	if (Setup == NULL || Setup->width <= 0 || Setup->height <= 0
	    || ext_view_width < 0 || ext_view_height < 0
	    || active_view_width < 0 || active_view_height < 0
	    || !isfinite(hudRadarScale) || hudRadarScale <= 0.0
	    || !isfinite(hudRadarLimit) || hudRadarLimit < 0.0
	    || !isfinite(clData.scale) || clData.scale <= 0.0
	    || num_radar < 0 || (num_radar > 0 && radar_ptr == NULL)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	map_scale = (double)Setup->width / 256.0;
	if (!isfinite(map_scale) || map_scale <= 0.0
	    || map_scale > (double)FLT_MAX) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	hudRadarMapScale = (float)map_scale;
	if (!isfinite(hudRadarMapScale) || hudRadarMapScale <= 0.0f)
	    return RENDERER_STATUS_INVALID_ARGUMENT;

	first_xlimit_value = hudRadarLimit * (ext_view_width / 2)
	    * hudRadarScale / (double)hudRadarMapScale;
	first_ylimit_value = hudRadarLimit * (ext_view_height / 2)
	    * hudRadarScale / (double)hudRadarMapScale;
	if (!Semantic_hud_radar_truncated_int(
		first_xlimit_value, &first_xlimit)
	    || !Semantic_hud_radar_truncated_int(
		first_ylimit_value, &first_ylimit)) {
	    return RENDERER_STATUS_INVALID_ARGUMENT;
	}
	second_scale = (double)hudRadarMapScale * clData.scale;
	second_xlimit = (active_view_width / 2) * clData.scale;
	second_ylimit = (active_view_height / 2) * clData.scale;
	status = Build_semantic_hud_radar_pass(
	    hudRadarScale, first_xlimit, first_ylimit,
	    hudRadarDotSize, &scene->passes[0]);
	if (status == RENDERER_STATUS_OK) {
	    status = Build_semantic_hud_radar_pass(
		second_scale, second_xlimit, second_ylimit,
		SHIP_SZ, &scene->passes[1]);
	}
	if (status != RENDERER_STATUS_OK)
	    return status;
	status = Validate_semantic_hud_radar_pass(&scene->passes[0]);
	if (status == RENDERER_STATUS_OK)
	    status = Validate_semantic_hud_radar_pass(&scene->passes[1]);
	if (status != RENDERER_STATUS_OK)
	    return status;
    }

    if (draw_ball_scan || draw_cover_scan) {
	if (!isfinite(clData.scale) || clData.scale <= 0.0)
	    return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (draw_ball_scan
	&& !Semantic_hud_radar_truncated_int(
	    8.0 * clData.scale, &ball_radius)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (draw_cover_scan
	&& !Semantic_hud_radar_truncated_int(
	    6.0 * clData.scale, &cover_radius)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    if (draw_ball_scan && ball_radius != 0) {
	status = Build_semantic_hud_radar_ring(
	    (int64_t)(draw_width / 2), (int64_t)(draw_height / 2),
	    ball_radius, scene->ball_scan);
	if (status != RENDERER_STATUS_OK)
	    return status;
    } else {
	scene->draw_ball_scan = 0;
    }
    if (draw_cover_scan && cover_radius != 0) {
	status = Build_semantic_hud_radar_ring(
	    (int64_t)(draw_width / 2), (int64_t)(draw_height / 2),
	    cover_radius, scene->cover_scan);
	if (status != RENDERER_STATUS_OK)
	    return status;
    } else {
	scene->draw_cover_scan = 0;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_hud_radar_and_scans(void)
{
    SemanticHudBatch batch;
    SemanticHudRadarScene scene;
    RendererStatus operation_status;
    int radar_enabled;
    int draw_ball_scan;
    int draw_cover_scan;

    radar_enabled = hudRadarEnemyColorRGBA
	|| hudRadarOtherColorRGBA || hudRadarObjectColorRGBA;
    draw_ball_scan = Bms_test_state(BmsBall) && msgScanBallColorRGBA;
    draw_cover_scan = Bms_test_state(BmsCover) && msgScanCoverColorRGBA;
    if (!radar_enabled && !draw_ball_scan && !draw_cover_scan)
	return RENDERER_STATUS_OK;

    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_radar_scene(
	radar_enabled, draw_ball_scan, draw_cover_scan, &scene);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);

    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ALPHA);
    Track_semantic_hud_batch(&batch, operation_status);
    if (batch.status == RENDERER_STATUS_OK && scene.radar_enabled) {
	Submit_semantic_hud_radar_pass(&batch, &scene.passes[0]);
	if (batch.status == RENDERER_STATUS_OK)
	    Submit_semantic_hud_radar_pass(&batch, &scene.passes[1]);
    }
    if (batch.status == RENDERER_STATUS_OK) {
	operation_status = Renderer_set_blend(
	    batch.renderer, RENDERER_BLEND_OPAQUE);
	Track_semantic_hud_batch(&batch, operation_status);
    }
    if (batch.status == RENDERER_STATUS_OK && scene.draw_ball_scan) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, scene.ball_scan,
	    SEMANTIC_HUD_RADAR_RING_POINTS, 1.0f,
	    Renderer_color_from_rgba32(msgScanBallColorRGBA), 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    if (batch.status == RENDERER_STATUS_OK && scene.draw_cover_scan) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, scene.cover_scan,
	    SEMANTIC_HUD_RADAR_RING_POINTS, 1.0f,
	    Renderer_color_from_rgba32(msgScanCoverColorRGBA), 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    return Finish_semantic_hud_batch(&batch);
}

#ifdef XPILOT_SDLGUI_TEST_HOOKS
RendererStatus Sdlgui_test_paint_hud_radar_dot(
    int x, int y, Uint32 color, int shape, int size)
{
    SemanticHudBatch batch;
    SemanticHudRadarDotGeometry geometry;
    RendererStatus operation_status;

    if (!Semantic_hud_radar_dot_visible(color, shape, size))
	return RENDERER_STATUS_OK;
    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_radar_dot_geometry(
	x, y, color, shape, size, &geometry);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);
    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ALPHA);
    if (Track_semantic_hud_batch(&batch, operation_status)
	== RENDERER_STATUS_OK) {
	Submit_semantic_hud_radar_dot(&batch, &geometry);
    }
    return Finish_semantic_hud_batch(&batch);
}

RendererStatus Sdlgui_test_paint_hud_radar_and_scans(void)
{
    return Paint_semantic_hud_radar_and_scans();
}
#endif

static RendererStatus Build_semantic_hud_item_outline(
    int horiz_pos, int vert_pos, RendererPoint2D points[4])
{
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;

    if (points == NULL)
	return RENDERER_STATUS_INVALID_ARGUMENT;

    left = (int64_t)horiz_pos - (int64_t)ITEM_SIZE - INT64_C(2);
    top = (int64_t)vert_pos - INT64_C(2);
    right = (int64_t)horiz_pos;
    bottom = (int64_t)vert_pos + (int64_t)ITEM_SIZE;
    if (!Semantic_hud_integer_float(left, &points[0].x)
	|| !Semantic_hud_integer_float(top, &points[0].y)
	|| !Semantic_hud_integer_float(right, &points[1].x)
	|| !Semantic_hud_integer_float(top, &points[1].y)
	|| !Semantic_hud_integer_float(right, &points[2].x)
	|| !Semantic_hud_integer_float(bottom, &points[2].y)
	|| !Semantic_hud_integer_float(left, &points[3].x)
	|| !Semantic_hud_integer_float(bottom, &points[3].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_hud_item_outline(
    int horiz_pos, int vert_pos)
{
    SemanticHudBatch batch;
    RendererPoint2D points[4];
    RendererStatus operation_status;

    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_item_outline(
	horiz_pos, vert_pos, points);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);
    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ADDITIVE);
    if (Track_semantic_hud_batch(&batch, operation_status)
	== RENDERER_STATUS_OK) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, points, 4, 1.0f,
	    Renderer_color_from_rgba32(hudItemsColorRGBA), 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    return Finish_semantic_hud_batch(&batch);
}

static RendererStatus Paint_HUD_items(int hud_pos_x, int hud_pos_y)
{
    const int		BORDER = 3;
    char		str[50];
    int     	    	vert_pos, horiz_pos;
    int     	    	i, maxWidth = -1;
    RendererStatus	status;
    static int		vertSpacing = -1;
    static fontbounds	fb;


    /* Special itemtypes */
    if (vertSpacing < 0)
	vertSpacing = MAX(ITEM_SIZE, gamefont.requested_height) + 1;
    /* find the scaled location, then work in pixels */
    vert_pos = hud_pos_y - hudSize+HUD_OFFSET + BORDER;
    horiz_pos = hud_pos_x - hudSize+HUD_OFFSET - BORDER;

    for (i = 0; i < NUM_ITEMS; i++) {
	int num = numItems[i];

	if (i == ITEM_FUEL)
	    continue;

	if (instruments.showItems) {
	    lastNumItems[i] = num;
	    if (num <= 0)
		num = -1;
	} else {
	    if (num != lastNumItems[i]) {
		numItemsTime[i] = (int)(showItemsTime * (double)FPS);
		lastNumItems[i] = num;
	    }
	    if (numItemsTime[i]-- <= 0) {
		numItemsTime[i] = 0;
		num = -1;
	    }
	}

	if (num >= 0) {

	    Image_paint_hud(IMG_HUD_ITEMS,
			horiz_pos - ITEM_SIZE, 
			vert_pos, (u_byte)i, 
			hudItemsColorRGBA);

	    if (i == lose_item) {
		if (lose_item_active != 0) {
		    if (lose_item_active < 0)
			lose_item_active++;
		    status = Paint_semantic_hud_item_outline(
			horiz_pos, vert_pos);
		    if (status != RENDERER_STATUS_OK)
			return status;
		}
	    }

	    /* Paint item count */
	    sprintf(str, "%d", num);
	    status = printsize(&gamefont, &fb, "%s", str);
	    if (status != RENDERER_STATUS_OK)
		return status;

	    maxWidth = MAX(maxWidth, fb.width + BORDER + ITEM_SIZE);
	    
	    status = HUDprint(
		&gamefont, hudItemsColorRGBA, RIGHT, UP,
		horiz_pos - ITEM_SIZE - BORDER,
		draw_height - vert_pos - ITEM_SIZE, "%s", str);
	    if (status != RENDERER_STATUS_OK)
		return status;

	    vert_pos += vertSpacing;

	    if (vert_pos+vertSpacing
		> hud_pos_y+hudSize-HUD_OFFSET-BORDER) {
			horiz_pos -= maxWidth + 2*BORDER;
			vert_pos = hud_pos_y - hudSize+HUD_OFFSET + BORDER;
			maxWidth = -1;
	    }
	}
    }
    return RENDERER_STATUS_OK;
}

static int Semantic_hud_fuel_gauge_visible(void)
{
    return fuelGaugeColorRGBA
	&& (fuelTime > 0.0
	    || (fuelSum < fuelNotify
		&& ((fuelSum < fuelCritical && (loopsSlow % 4) < 2)
		    || (fuelSum < fuelWarning
			&& fuelSum > fuelCritical
			&& (loopsSlow % 8) < 4)
		    || fuelSum > fuelWarning)));
}

static RendererStatus Build_semantic_hud_fuel_gauge_geometry(
    int hud_pos_x, int hud_pos_y, SemanticHudFuelGaugeGeometry *geometry)
{
    double fill_size_value;
    int fill_size;
    int64_t outline_left;
    int64_t outline_top;
    int64_t outline_right;
    int64_t outline_bottom;
    int64_t fill_left;
    int64_t fill_top;
    int64_t fill_width;
    int64_t fill_height;
    int64_t fill_bottom;

    if (geometry == NULL || !isfinite(fuelSum) || !isfinite(fuelMax)
	|| fuelMax == 0.0) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    fill_size_value = (double)HUD_FUEL_GAUGE_SIZE * fuelSum / fuelMax;
    if (!isfinite(fill_size_value)
	|| fill_size_value < (double)INT_MIN
	|| fill_size_value > (double)INT_MAX) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    fill_size = (int)fill_size_value;

    outline_left = (int64_t)hud_pos_x + (int64_t)hudSize
	- (int64_t)HUD_OFFSET + (int64_t)FUEL_GAUGE_OFFSET - INT64_C(1);
    outline_top = (int64_t)hud_pos_y - (int64_t)hudSize
	+ (int64_t)HUD_OFFSET + (int64_t)FUEL_GAUGE_OFFSET - INT64_C(1);
    outline_right = outline_left
	+ (int64_t)(HUD_OFFSET - 2 * FUEL_GAUGE_OFFSET + 3);
    outline_bottom = outline_top + (int64_t)(HUD_FUEL_GAUGE_SIZE + 3);
    if (!Semantic_hud_integer_float(
	    outline_left, &geometry->outline[0].x)
	|| !Semantic_hud_integer_float(
	    outline_top, &geometry->outline[0].y)
	|| !Semantic_hud_integer_float(
	    outline_left, &geometry->outline[1].x)
	|| !Semantic_hud_integer_float(
	    outline_bottom, &geometry->outline[1].y)
	|| !Semantic_hud_integer_float(
	    outline_right, &geometry->outline[2].x)
	|| !Semantic_hud_integer_float(
	    outline_bottom, &geometry->outline[2].y)
	|| !Semantic_hud_integer_float(
	    outline_right, &geometry->outline[3].x)
	|| !Semantic_hud_integer_float(
	    outline_top, &geometry->outline[3].y)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }

    geometry->draw_fill = fill_size != 0;
    if (!geometry->draw_fill)
	return RENDERER_STATUS_OK;

    fill_left = (int64_t)hud_pos_x + (int64_t)hudSize
	- (int64_t)HUD_OFFSET + (int64_t)FUEL_GAUGE_OFFSET + INT64_C(1);
    fill_bottom = (int64_t)hud_pos_y - (int64_t)hudSize
	+ (int64_t)HUD_OFFSET + (int64_t)FUEL_GAUGE_OFFSET
	+ (int64_t)HUD_FUEL_GAUGE_SIZE + INT64_C(1);
    fill_top = fill_bottom - (int64_t)fill_size;
    fill_width = (int64_t)(HUD_OFFSET - 2 * FUEL_GAUGE_OFFSET);
    fill_height = (int64_t)fill_size;
    if (fill_height < 0) {
	fill_top += fill_height;
	fill_height = -fill_height;
    }
    if (!Semantic_hud_integer_float(fill_left, &geometry->fill_x)
	|| !Semantic_hud_integer_float(fill_top, &geometry->fill_y)
	|| !Semantic_hud_integer_float(fill_width, &geometry->fill_width)
	|| !Semantic_hud_integer_float(fill_height, &geometry->fill_height)) {
	return RENDERER_STATUS_INVALID_ARGUMENT;
    }
    return RENDERER_STATUS_OK;
}

static RendererStatus Paint_semantic_hud_fuel_gauge(
    int hud_pos_x, int hud_pos_y)
{
    SemanticHudBatch batch;
    SemanticHudFuelGaugeGeometry geometry;
    RendererColor color;
    RendererStatus operation_status;

    if (!Semantic_hud_fuel_gauge_visible())
	return RENDERER_STATUS_OK;
    if (Begin_semantic_hud_batch(&batch) != RENDERER_STATUS_OK)
	return batch.status;
    operation_status = Build_semantic_hud_fuel_gauge_geometry(
	hud_pos_x, hud_pos_y, &geometry);
    if (operation_status != RENDERER_STATUS_OK)
	return Track_semantic_hud_batch(&batch, operation_status);

    operation_status = Renderer_set_blend(
	batch.renderer, RENDERER_BLEND_ADDITIVE);
    Track_semantic_hud_batch(&batch, operation_status);
    color = Renderer_color_from_rgba32(fuelGaugeColorRGBA);
    if (batch.status == RENDERER_STATUS_OK) {
	operation_status = Renderer_stroke_path(
	    batch.renderer, geometry.outline, 4, 1.0f, color, 1);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    if (batch.status == RENDERER_STATUS_OK && geometry.draw_fill) {
	operation_status = Renderer_fill_rect(
	    batch.renderer, geometry.fill_x, geometry.fill_y,
	    geometry.fill_width, geometry.fill_height, color);
	Accept_semantic_hud_command(&batch, operation_status);
    }
    return Finish_semantic_hud_batch(&batch);
}

#ifdef XPILOT_SDLGUI_TEST_HOOKS
RendererStatus Sdlgui_test_paint_hud_items(int hud_pos_x, int hud_pos_y)
{
    return Paint_HUD_items(hud_pos_x, hud_pos_y);
}

RendererStatus Sdlgui_test_paint_hud_fuel_gauge(
    int hud_pos_x, int hud_pos_y)
{
    return Paint_semantic_hud_fuel_gauge(hud_pos_x, hud_pos_y);
}

RendererStatus Sdlgui_test_paint_hud_frame(
    int hud_pos_x, int hud_pos_y)
{
    return Paint_semantic_hud_frame(hud_pos_x, hud_pos_y);
}
#endif

RendererStatus Paint_HUD_checked(void)
{
    const int		BORDER = 3;
    char		str[50];
    int			hud_pos_x, hud_pos_y;
    int			did_fuel = 0;
    int			i, j, tex_index;
    static char		autopilot[] = "Autopilot";
    fontbounds dummy;
    RendererStatus status;

    status = Paint_semantic_hud_pointers();
    if (status != RENDERER_STATUS_OK)
	return status;
    status = Paint_semantic_hud_radar_and_scans();
    if (status != RENDERER_STATUS_OK)
	return status;
    tex_index = 0;

    /*
     * Display the HUD
     */
    hud_pos_x = (int)(draw_width / 2 - hud_move_fact * selfVel.x);
    hud_pos_y = (int)(draw_height / 2 + hud_move_fact * selfVel.y);

    status = Paint_semantic_hud_frame(hud_pos_x, hud_pos_y);
    if (status != RENDERER_STATUS_OK)
	goto finish_hud;

    if (hudItemsColorRGBA) {
	status = Paint_HUD_items(hud_pos_x, hud_pos_y);
	if (status != RENDERER_STATUS_OK)
	    goto finish_hud;
    }

    /* Fuel notify, HUD meter on */
    if (hudColorRGBA && (fuelTime > 0.0 || fuelSum < fuelNotify)) {
	did_fuel = 1;
	/* TODO fix this */
	sprintf(str, "%04d", (int)fuelSum);
	tex_index=0;
	if (Ensure_cached_text(&gamefont, str, &HUD_texs[tex_index])) {
	    status = disp_text(
		&HUD_texs[tex_index], hudColorRGBA, LEFT, DOWN,
		hud_pos_x + hudSize - HUD_OFFSET + BORDER,
		hud_pos_y - (hudSize - HUD_OFFSET + BORDER), true);
	    if (status != RENDERER_STATUS_OK)
		goto finish_hud;
	}

	if (numItems[ITEM_TANK]) {
	    if (fuelCurrent == 0)
		strcpy(str,"M ");
	    else
		sprintf(str, "T%d", fuelCurrent);

	    tex_index=1;
	    if (Ensure_cached_text(&gamefont, str, &HUD_texs[tex_index])) {
		status = disp_text(
		    &HUD_texs[tex_index], hudColorRGBA, LEFT, DOWN,
		    hud_pos_x + hudSize - HUD_OFFSET + BORDER,
		    hud_pos_y - hudSize - HUD_OFFSET + BORDER, true);
		if (status != RENDERER_STATUS_OK)
		    goto finish_hud;
	    }

	}
    }

    /* Update the lock display */
    status = Paint_lock(hud_pos_x, hud_pos_y);
    if (status != RENDERER_STATUS_OK)
	goto finish_hud;

    /* Draw last score on hud if it is an message attached to it */
    if (hudColorRGBA) {
	for (i = 0, j = 0; i < MAX_SCORE_OBJECTS; i++) {
	    score_object_t*	sobj = &score_objects[(i+score_object)%MAX_SCORE_OBJECTS];
	    if (sobj->hud_msg_len > 0) {
		status = printsize(&gamefont, &dummy, "%s", sobj->hud_msg);
		if (status != RENDERER_STATUS_OK)
		    goto finish_hud;
		if (sobj->hud_msg_width == -1)
		    sobj->hud_msg_width = (int)dummy.width;
		if (j == 0 &&
		    sobj->hud_msg_width > 2*hudSize-HUD_OFFSET*2 &&
		    (did_fuel || hudVLineColorRGBA))
		    ++j;

		tex_index=MAX_HUD_TEXS+i;
		if (Ensure_cached_text(&gamefont, sobj->hud_msg,
				       &HUD_texs[tex_index])) {
		    status = disp_text(
			&HUD_texs[tex_index], hudColorRGBA,
			CENTER, DOWN, hud_pos_x,
			hud_pos_y - (hudSize - HUD_OFFSET + BORDER
				+ j * HUD_texs[tex_index].height), true);
		    if (status != RENDERER_STATUS_OK)
			goto finish_hud;
		}
		j++;
	    }
	}

	if (time_left > 0) {
	    sprintf(str, "%3d:%02d", (int)(time_left / 60), (int)(time_left % 60));
	    tex_index=3;
	    if (Ensure_cached_text(&gamefont, str, &HUD_texs[tex_index])) {
		status = disp_text(
		    &HUD_texs[tex_index], hudColorRGBA, RIGHT, DOWN,
		    hud_pos_x - hudSize + HUD_OFFSET - BORDER,
		    hud_pos_y + hudSize + HUD_OFFSET + BORDER, true);
		if (status != RENDERER_STATUS_OK)
		    goto finish_hud;
	    }
	}

	/* Update the modifiers */
	tex_index=4;
	if(strlen(mods)) {
	    if (Ensure_cached_text(&gamefont, mods, &HUD_texs[tex_index])) {
		status = disp_text(
		    &HUD_texs[tex_index], hudColorRGBA, RIGHT, UP,
		    hud_pos_x - hudSize + HUD_OFFSET - BORDER,
		    hud_pos_y - hudSize + HUD_OFFSET - BORDER, true);
		if (status != RENDERER_STATUS_OK)
		    goto finish_hud;
	    }
    	}

	if (autopilotLight) {
	    tex_index=5;
	    if (Ensure_cached_text(&gamefont, autopilot,
				   &HUD_texs[tex_index])) {
		status = disp_text(
		    &HUD_texs[tex_index], hudColorRGBA, RIGHT, DOWN,
		    hud_pos_x,
		    hud_pos_y + hudSize + HUD_OFFSET + BORDER
			+ HUD_texs[tex_index].height * 2, true);
		if (status != RENDERER_STATUS_OK)
		    goto finish_hud;
	    }
	}
    }

    if (fuelTime > 0.0) {
	fuelTime -= timePerFrame;
	if (fuelTime <= 0.0)
	    fuelTime = 0.0;
    }

    status = Paint_semantic_hud_fuel_gauge(hud_pos_x, hud_pos_y);

finish_hud:
    return status;
}

void Paint_HUD(void)
{
    (void)Paint_HUD_checked();
}

typedef struct alert_timeout_struct alert_timeout;
struct alert_timeout_struct {
    GLWidget	    *msg;   /* use to build widget lists */
    double     	    timeout;
    alert_timeout   *next;
};
static alert_timeout *alert_timeout_list = NULL;

void Add_alert_message(const char *message, double timeout)
{
    GLWidget *tmp = NULL;
    alert_timeout *tol;
    
    tmp = Init_LabelWidget(message,&whiteRGBA,&nullRGBA,CENTER,CENTER);
    if (tmp) {
    	ListWidget_Prepend(((WrapperWidget *)(MainWidget->wid_info))->alert_msgs,tmp);
    } else {
    	error("Add_alert_message: Failed to create LabelWidget");
	return;
    }
    
    tol = alert_timeout_list;
    alert_timeout_list = (alert_timeout *)malloc(sizeof(alert_timeout));
    alert_timeout_list->next = tol;
    alert_timeout_list->timeout = timeout;
    alert_timeout_list->msg = tmp;
}

void Clear_alert_messages(void)
{
    GLWidget *tmp,*list;
    bool dummy;
    alert_timeout *tol;
    
    while ((tol = alert_timeout_list)) {
    	alert_timeout_list = alert_timeout_list->next;
	free(tol);
    }
    
    list = ((WrapperWidget *)(MainWidget->wid_info))->alert_msgs;
    dummy = true;
    while (dummy) {
    	tmp = ListWidget_GetItemByIndex( list, 0 );
	if (tmp == NULL) break;
    	dummy = ListWidget_Remove( list, tmp );
    }
}

void Paint_messages(void)
{
    static int old_maxMessages = 0;
    static message_t **msgs[2];
    static GLWidget *msg_list[2] = {NULL,NULL};
    static bool showMessages = true;

    int j, i = 0;
    Uint32 *msg_color;
    /*const int BORDER = 10;*/
    GLWidget *tmp = NULL,*tmp2 = NULL;
    LabelWidget *wi;
    message_t	*msg;

    alert_timeout *garbage, **tol = &alert_timeout_list;
    static int lastloops;
    
    msgs[0] = TalkMsg;
    msgs[1] = GameMsg;
    
    msg_list[0] = ((WrapperWidget *)(MainWidget->wid_info))->chat_msgs;
    msg_list[1] = ((WrapperWidget *)(MainWidget->wid_info))->game_msgs;

    if ( showMessages != instruments.showMessages ) {
    	if (!instruments.showMessages)
	    DelGLWidgetListItem( &(MainWidget->children), ((WrapperWidget *)(MainWidget->wid_info))->game_msgs );
	else
	    AppendGLWidgetList( &(MainWidget->children), ((WrapperWidget *)(MainWidget->wid_info))->game_msgs );
	        
	showMessages = instruments.showMessages;
    }
    
    if ( maxMessages < old_maxMessages ) {
    	for (i=0;i<2;++i)
	    while ((tmp = ListWidget_GetItemByIndex(msg_list[i],maxMessages)) != NULL) {
    	    	ListWidget_Remove(msg_list[i],tmp);
	    	Close_Widget(&tmp);
    	    }
    }
    
    /* Check if any alert message has timed out, if so remove it */
    while ((*tol)) {
	if ((*tol)->timeout != 0.0) {
	    (*tol)->timeout -= (loops - lastloops)/(double)FPS;
	    if ((*tol)->timeout <= 0.0) {
	    	garbage = (*tol);
		*tol = (*tol)->next;
    	    	ListWidget_Remove( ((WrapperWidget *)(MainWidget->wid_info))->alert_msgs, garbage->msg );
	    	free(garbage);
		continue;
	    }
	}
    	tol = &((*tol)->next);
    }
    lastloops = loops;
    
    /* TODO: check whether there is a more efficient way to do this!
     * i.e. add labelwidgets as messages are added/removed
     * For now this will have to do...
     */
    for ( i=0 ; i < 2 ; ++i)
    for ( j=0 ; j <= maxMessages-1 ; ++j) {
    	msg = (msgs[i])[j];
	tmp = tmp2 = NULL;
	
    	if ((msg->lifeTime -= timePerFrame) <= 0.0) {
    	    msg->txt[0] = '\0';
    	    msg->len = 0;
    	    msg->lifeTime = 0.0;
    	}
	
	if ((tmp = ListWidget_GetItemByIndex(msg_list[i],j) ) != NULL) {
	    if ( !(wi = (LabelWidget *)tmp->wid_info) ) {
	    	error("Paint_messages: ListWidget lacks a wid_info ptr!");
		continue;
	    }
	    if (strlen(msg->txt)) {
	    	if ( strcmp(msg->txt, wi->tex.text) ) {
		    tmp2 = Init_LabelWidget(msg->txt,&messagesColorRGBA,&nullRGBA,LEFT,CENTER);
		    ListWidget_Insert(msg_list[i],tmp,tmp2);
		    if (ListWidget_NELEM(msg_list[i])>maxMessages) {
		    	tmp = ListWidget_GetItemByIndex(msg_list[i],maxMessages);
			ListWidget_Remove(msg_list[i],tmp);
			Close_Widget(&tmp);
		    }
		} else tmp2 = tmp;
	    } else {
	    	ListWidget_Remove(msg_list[i],tmp);
		Close_Widget(&tmp);
	    }
	} else {
	    if (strlen(msg->txt)) {
		tmp2 = Init_LabelWidget(msg->txt,&messagesColorRGBA,&nullRGBA,LEFT,CENTER);
		ListWidget_Append(msg_list[i],tmp2);
	    }
	}
		
	if (msg->lifeTime <= MSG_FLASH_TIME)
	    msg_color = &oldmessagesColorRGBA;
	else {
	    /* If paused, don't bother to paint messages in mscScan* colors. */
	    if (self && strchr("P", self->mychar))
		msg_color = &messagesColorRGBA;
	    else {
		switch (msg->bmsinfo) {
		case BmsBall:	msg_color = &msgScanBallColorRGBA;	break;
		case BmsSafe:	msg_color = &msgScanSafeColorRGBA;	break;
		case BmsCover:	msg_color = &msgScanCoverColorRGBA;	break;
		case BmsPop:	msg_color = &msgScanPopColorRGBA;	break;
		default:	msg_color = &messagesColorRGBA;	    	break;
		}
	    }
	}

	/* kps ugly hack */
	if (newbie) {
	    const char *help = "[*Newbie help*]";
	    size_t sz = strlen(msg->txt);
	    size_t hsz = strlen(help);

	    if (sz > hsz
		&& !strcmp(help, &msg->txt[sz - hsz]))
		msg_color = &whiteRGBA;
	}

	
	if (tmp2) LabelWidget_SetColor(tmp2, msg_color, &nullRGBA);
    }
    	
    old_maxMessages = maxMessages;
}

static bool set_rgba_color_option(xp_option_t *opt, const char *val)
{
    int c = 0;
    assert(val);
    if (*val != '#') return false;
    c = strtoul(val + 1, NULL, 16) & 0xffffffff;
    *((int*)Option_get_private_data(opt)) = c;
    return true;
}

static const char *get_rgba_color_option(xp_option_t *opt)
{
    static char buf[10];
    sprintf(buf, "#%08X", *((int*)Option_get_private_data(opt)));
    return buf;
}

#define COLOR(variable, defval, description) \
    XP_STRING_OPTION(#variable, \
		     defval, \
                     NULL, \
		     0, \
		     set_rgba_color_option, \
		     &variable, \
		     get_rgba_color_option, \
		     XP_OPTFLAG_CONFIG_COLORS, \
		     "The color of " description ".\n")


static xp_option_t sdlgui_options[] = {

    COLOR(messagesColorRGBA, "#00aaaaff", "messages"),
    COLOR(oldmessagesColorRGBA, "#008888ff", "old messages"),
    COLOR(msgScanBallColorRGBA, "#ff0000ff", "ball warning"),
    COLOR(msgScanSafeColorRGBA, "#00ff00ff", "ball safe announcement"),
    COLOR(msgScanCoverColorRGBA, "#4e7cffff", "cover request"),
    COLOR(msgScanPopColorRGBA, "#ffbb11ff", "ball pop announcement"),

    COLOR(meterBorderColorRGBA, "#0000ffaa", "meter borders"),
    COLOR(fuelMeterColorRGBA, "#ff0000aa", "fuel meter"),
    COLOR(fuelGaugeColorRGBA, "#0000ff44", "fuel gauge"),
    COLOR(powerMeterColorRGBA, "#ff0000aa", "power meter"),
    COLOR(turnSpeedMeterColorRGBA, "#ff0000aa", "turn speed meter"),
    COLOR(packetSizeMeterColorRGBA, "#ff0000aa", "packet size meter"),
    COLOR(packetLossMeterColorRGBA, "#ff0000aa", "packet loss meter"),
    COLOR(packetDropMeterColorRGBA, "#ff0000aa", "drop meter"),
    COLOR(packetLagMeterColorRGBA, "#ff0000aa", "lag meter"),
    COLOR(temporaryMeterColorRGBA, "#ff0000aa", "time meter"),

    COLOR(ballColorRGBA, "#00ff00ff", "balls"),
    COLOR(connColorRGBA, "#00ff0088", "the ball connector"),
    COLOR(fuelColorRGBA, "#ffffff7f", "fuel cells"),
    COLOR(wallColorRGBA, "#0000ffff", "walls on blockmaps"),
    COLOR(decorColorRGBA, "#bb7700ff", "decorations on blockmaps"),
    COLOR(baseNameColorRGBA, "#77bbffff", "base name"),
    COLOR(shipNameColorRGBA, "#77bbffff", "ship name"),
    COLOR(scoreObjectColorRGBA, "#00ff0088", "score objects"),

    COLOR(hudColorRGBA, "#ff000088", "the HUD"),
    COLOR(hudHLineColorRGBA, "#0000ff44", "horizontal HUD line"),
    COLOR(hudVLineColorRGBA, "#0000ff44", "vertical HUD line"),
    COLOR(hudItemsColorRGBA, "#0080ffaa", "hud items"),
    COLOR(hudRadarEnemyColorRGBA, "#ff000088", "enemy on HUD radar"),
    COLOR(hudRadarOtherColorRGBA, "#0000ff88", "friend on HUD radar"),
    COLOR(hudRadarObjectColorRGBA, "#00000000", "small object on HUD radar"),

    COLOR(dirPtrColorRGBA, "#0000ff22", "direction pointer"),
    COLOR(selectionColorRGBA, "#ff0000ff", "selection"),

    COLOR(scoreInactiveSelfColorRGBA, "#88008888", "my score when inactive"),
    COLOR(scoreInactiveColorRGBA, "#8800aa88", "score when inactive"),
    COLOR(scoreSelfColorRGBA, "#ffff00ff", "my score"),
    COLOR(scoreColorRGBA, "#888800ff", "score"),
    COLOR(scoreOwnTeamColorRGBA, "#0000ffff", "my team score"),
    COLOR(scoreEnemyTeamColorRGBA, "#ff0000ff", "enemy team score"),

    COLOR(teamShipColorRGBA, "#0000ffff", "teammate color"),
    COLOR(team0ColorRGBA, "#00000000", "team 0"),
    COLOR(team1ColorRGBA, "#00000000", "team 1"),
    COLOR(team2ColorRGBA, "#00000000", "team 2"),
    COLOR(team3ColorRGBA, "#00000000", "team 3"),
    COLOR(team4ColorRGBA, "#00000000", "team 4"),
    COLOR(team5ColorRGBA, "#00000000", "team 5"),
    COLOR(team6ColorRGBA, "#00000000", "team 6"),
    COLOR(team7ColorRGBA, "#00000000", "team 7"),
    COLOR(team8ColorRGBA, "#00000000", "team 8"),
    COLOR(team9ColorRGBA, "#00000000", "team 9"),
    
    COLOR(selfLWColorRGBA, "#ff0000ff", "my ship on last life"),
    COLOR(teamLWColorRGBA, "#ff00ffff", "team ship on last life"),
    COLOR(enemyLWColorRGBA, "#ffff00ff", "enemy ship on last life"),
    COLOR(manyLivesColorRGBA, "#666666aa", "name of ship with many lives"),
    COLOR(twoLivesColorRGBA, "#008800aa", "name of ship with two lives"),
    COLOR(oneLifeColorRGBA, "#aaaa00aa", "name of ship with one life"),
    COLOR(zeroLivesColorRGBA, "#ff0000aa", "name of ship with no lives"),
    
    XP_INT_OPTION(
        "meterWidth",
	60, 0, 600,
	&meterWidth,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"Set the width of the meters.\n"),

    XP_INT_OPTION(
        "meterHeight",
	10, 0, 100,
	&meterHeight,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"Set the height of a meter.\n"),

    XP_DOUBLE_OPTION(
        "shipLineWidth",
	1.0, 1.0, 10.0,
	&shipLineWidth,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"Set the line width of ships.\n"),

    XP_BOOL_OPTION(
        "texturedBalls",
        true,
	&texturedBalls,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"Draw balls with textures.\n"),

    XP_BOOL_OPTION(
        "texturedShips",
        true,
	&texturedShips,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"Draw ships with textures.\n"),

    XP_INT_OPTION(
        "hudRadarEnemyShape",
	2, 1, 7,
	&hudRadarEnemyShape,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"The shape of enemy ships on hud radar.\n"),

    XP_INT_OPTION(
        "hudRadarOtherShape",
	2, 1, 7,
	&hudRadarOtherShape,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"The shape of friendly ships on hud radar.\n"),

    XP_INT_OPTION(
        "hudRadarObjectShape",
	0, 0, 7,
	&hudRadarObjectShape,
	NULL,
	XP_OPTFLAG_CONFIG_DEFAULT,
	"The shape of small objects on hud radar.\n")
};

void Store_sdlgui_options(void)
{
    STORE_OPTIONS(sdlgui_options);
}
