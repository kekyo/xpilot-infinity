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

#ifndef CLIENT_H
#define CLIENT_H

#ifdef _WINDOWS
#ifndef	_WINSOCKAPI_
#include <winsock.h>
#endif
#endif

#ifndef DRAW_H
/* need shipshape_t */
#include "shipshape.h"
#endif
#ifndef ITEM_H
/* need NUM_ITEMS */
#include "item.h"
#endif
#ifndef CONNECTPARAM_H
/* need Connect_param_t */
#include "connectparam.h"
#endif
#ifndef CONNECT_TARGET_H
/* need Connect_defaults_t and Connect_target_t */
#include "connect_target.h"
#endif
#ifndef OPTION_H
/* need xp_keysym_t */
#include "option.h"
#endif

typedef struct {
    bool	talking;	/* Some talk window is open? */
    bool	pointerControl;	/* Pointer (mouse) control is on? */
    bool	restorePointerControl;
				/* Pointer control should be restored later? */
    bool	quitMode;	/* Client is in quit mode? */
    double	clientLag;
    double	scaleFactor;
    double	scale;
    float	fscale;
    double	altScaleFactor;
} client_data_t;

typedef struct {
    bool clientRanker;
    bool clockAMPM;
    bool filledDecor;
    bool filledWorld;
    bool outlineDecor;
    bool outlineWorld;
    bool showDecor;
    bool showItems;
    bool showLivesByShip;
    bool showMessages;
    bool showMyShipShape;
    bool showNastyShots;
    bool showShipShapes;
    bool showShipShapesHack;
    bool slidingRadar;
    bool texturedDecor;
    bool texturedWalls;
} instruments_t;

typedef struct {
    bool help;
    bool version;
    bool text;
    bool list_servers; /* list */
    bool auto_connect; /* join */
    char shutdown_reason[MAX_CHARS]; /* shutdown reason */
} xp_args_t;

#define PACKET_LOSS		0
#define PACKET_DROP		1
#define PACKET_DRAW		2

#define MAX_SCORE_OBJECTS	10

#define MAX_SPARK_SIZE		8
#define MIN_SPARK_SIZE		1
#define MAX_MAP_POINT_SIZE	8
#define MIN_MAP_POINT_SIZE	0
#define MAX_SHOT_SIZE		20
#define MIN_SHOT_SIZE		1
#define MAX_TEAMSHOT_SIZE	20
#define MIN_TEAMSHOT_SIZE	1

#define MIN_SHOW_ITEMS_TIME	0.0
#define MAX_SHOW_ITEMS_TIME	300.0

#define MIN_SCALEFACTOR		0.1
#define MAX_SCALEFACTOR		20.0

#define FUEL_NOTIFY_TIME	3.0
#define CONTROL_TIME		8.0

#define MAX_MSGS		15	/* Max. messages displayed ever */
#define MAX_HIST_MSGS		128	/* Max. messages in history */

#define MSG_LIFE_TIME		120.0	/* Seconds */
#define MSG_FLASH_TIME		105.0	/* Old messages have life time less
					   than this */
#define MAX_POINTER_BUTTONS	5

/*
 * Macros to manipulate dynamic arrays.
 */

/*
 * Macro to add one new element of a given type to a dynamic array.
 * T is the type of the element.
 * P is the pointer to the array memory.
 * N is the current number of elements in the array.
 * M is the current size of the array.
 * V is the new element to add.
 * The goal is to keep the number of malloc/realloc calls low
 * while not wasting too much memory because of over-allocation.
 */
#define STORE(T,P,N,M,V)						\
    if (N >= M && ((M <= 0)						\
	? (P = (T *) malloc((M = 1) * sizeof(*P)))			\
	: (P = (T *) realloc(P, (M += M) * sizeof(*P)))) == NULL) {	\
	error("No memory");						\
	exit(1);							\
    } else								\
	(P[N++] = V)
/*
 * Macro to make room in a given dynamic array for new elements.
 * P is the pointer to the array memory.
 * N is the current number of elements in the array.
 * M is the current size of the array.
 * T is the type of the elements.
 * E is the number of new elements to store in the array.
 * The goal is to keep the number of malloc/realloc calls low
 * while not wasting too much memory because of over-allocation.
 */
#define EXPAND(P,N,M,T,E)						\
    if ((N) + (E) > (M)) {						\
	if ((M) <= 0) {							\
	    M = (E) + 2;						\
	    P = (T *) malloc((M) * sizeof(T));				\
	    N = 0;							\
	} else {							\
	    M = ((M) << 1) + (E);					\
	    P = (T *) realloc(P, (M) * sizeof(T));			\
	}								\
	if (P == NULL) {						\
	    error("No memory");						\
	    N = M = 0;							\
	    return;	/* ! */						\
	}								\
    }

#define UNEXPAND(P,N,M)							\
    if ((N) < ((M) >> 2)) {						\
	free(P);							\
	M = 0;								\
    }									\
    N = 0;

#ifndef PAINT_FREE
# define PAINT_FREE	1
#endif
#if PAINT_FREE
# define RELEASE(P, N, M)					\
do {								\
	if (!(N)) ; else (free(P), (M) = 0, (N) = 0);		\
} while (0)
#else
# define RELEASE(P, N, M)	((N) = 0)
#endif


typedef struct {
    double	score;
    short	id;
    uint16_t	team;
    short	check;
    short	round;
    long	timing_loops;
    short	timing;
    short	life;
    short	mychar;
    short	alliance;
    short	name_width;	/* In pixels */
    short	name_len;	/* In bytes */
    short	max_chars_in_names;	/* name_width was calculated
					   for this value of maxCharsInNames */
    short	ignorelevel;
    shipshape_t	*ship;
    char	nick_name[MAX_CHARS];
    char	user_name[MAX_CHARS];
    char	host_name[MAX_CHARS];
    char	id_string[MAX_CHARS];
} other_t;

/**
 * Return the printable marker occupying a player's alliance column.
 *
 * @param alliance Alliance marker from score metadata, or NUL before the
 * first score packet has arrived.
 * @return The character to place in the fixed-width alliance column.
 */
static inline int Player_alliance_display_char(int alliance)
{
    return alliance == '\0' ? ' ' : alliance;
}

typedef struct {
    int		pos;		/* Block index */
    double	fuel;		/* Amount of fuel available */
    irec_t	bounds;		/* Location on map */
} fuelstation_t;

typedef struct {
    int		pos;		/* Block index */
    short	id;		/* Id of owner or -1 */
    uint16_t	team;		/* Team this base belongs to */
    irec_t	bounds;		/* Location on map */
    int		type;		/* orientation */
    long	appeartime;	/* For base warning */
} homebase_t;

typedef struct {
    int		pos;		/* Block index */
    short	dead_time,	/* Frames inactive */
		dot;		/* Draw dot if inactive */
} cannontime_t;

typedef struct {
    int		pos;		/* Block index */
    short	dead_time;	/* Frames inactive */
    double	damage;		/* Damage to target */
} target_t;

typedef struct {
    int		pos;		/* Block index */
    irec_t	bounds;		/* Location on map */
} checkpoint_t;

typedef struct {
    int width;			/* Line width, -1 means no line */
    unsigned long color;	/* Line color */
    int rgb;			/* RGB values corresponding to color */
    int style;			/* 0=LineSolid, 1=LineOnOffDash, 2=LineDoubleDash */
} edge_style_t;

typedef struct {
    unsigned long color;	/* The color if drawn in filled mode */
    int rgb;			/* RGB values corresponding to color */
    int texture;		/* The texture if drawn in texture mode */
    int flags;			/* Flags about this style (see draw.h) */
    int def_edge_style;		/* The default style for edges */
} polygon_style_t;

typedef struct {
    ipos_t *points;		/* points[0] is absolute, rest are relative */
    int num_points;		/* number of points */
    irec_t bounds;		/* bounding box for the polygon */
    int *edge_styles;		/* optional array of indexes to edge_styles */
    int style;			/* index to polygon_styles array */
} xp_polygon_t;


/*
 * Types for dynamic game data
 */

typedef struct {
    short		x0, y0, x1, y1;
} refuel_t;

typedef struct {
    short		x0, y0, x1, y1;
    u_byte		tractor;
} connector_t;

typedef struct {
    unsigned char	color, dir;
    short		x, y, len;
} laser_t;

typedef struct {
    short		x, y, dir;
    unsigned char	len;
} missile_t;

typedef struct {
    short		x, y, id;
    u_byte		style;
} ball_t;

typedef struct {
    short		x, y, id, dir;
    u_byte		shield, cloak, eshield;
    u_byte		phased, deflector;
} ship_t;

typedef struct {
    short		x, y, teammine, id;
} mine_t;

typedef struct {
    short		x, y, type;
} itemtype_t;

typedef struct {
    short		x, y, size;
} ecm_t;

typedef struct {
    short		x1, y1, x2, y2;
} trans_t;

typedef struct {
    short		x, y, count;
} paused_t;

typedef struct {
    short		x, y, id, count;
} appearing_t;

typedef enum {
    RadarEnemy,
    RadarFriend
} radar_type_t;

typedef struct {
    short		x, y, size;
    radar_type_t        type;
} radar_t;

typedef struct {
    short		x, y, type;
} vcannon_t;

typedef struct {
    short		x, y;
    double		fuel;
} vfuel_t;

typedef struct {
    short		x, y, xi, yi, type;
} vbase_t;

typedef struct {
    u_byte		x, y;
} debris_t;

typedef struct {
    short		x, y, xi, yi, type;
} vdecor_t;

typedef struct {
    short		x, y;
    u_byte		wrecktype, size, rotation;
} wreckage_t;

typedef struct {
    short		x, y;
    u_byte		type, size, rotation;
} asteroid_t;

typedef struct {
    short		x, y;
} wormhole_t;


/*#define SCORE_OBJECT_COUNT	100*/
typedef struct {
    double	score,
		life_time;
    int		x,
		y,
		hud_msg_len,
		hud_msg_width,
		msg_width,
		msg_len;
    char	msg[10],
		hud_msg[MAX_CHARS+10];
} score_object_t;


/*
 * is a selection pending (in progress), done, drawn emphasized?
 */
/** Lifecycle state of a text selection in the talk or draw window. */
typedef enum {
    SEL_NONE = (1 << 0),       /**< No text is selected. */
    SEL_PENDING = (1 << 1),    /**< The user is still selecting text. */
    SEL_SELECTED = (1 << 2),   /**< Text is selected without a highlight. */
    SEL_EMPHASIZED = (1 << 3)  /**< Selected text is highlighted. */
} selection_state_t;

/*
 * a selection (text, string indices, state,...)
 */
typedef struct {
    /* a selection in the talk window */
    struct {
        selection_state_t state; /* current state of the selection */
        size_t  x1;	/* string indices */
        size_t  x2;
        bool    incl_nl;/* include a '\n'? */
    } talk ;
    /* a selection in the draw window */
    struct {
        selection_state_t state; /**< Current draw-window selection state. */
        int     x1;	/* string indices (for TalkMsg[].txt) */
        int     x2;	/* they are modified when the emphasized area */
        int     y1;	/* is scrolled down by new messages coming in */
        int     y2;
    } draw;
    char	*txt;   /* allocated when needed */
    size_t	txt_size;	/* size of txt buffer */
    size_t	len;
    /* when a message 'jumps' from talk window to the player messages: */
    bool	keep_emphasizing;
} selection_t;

/* typedefs begin */
typedef enum {
    BmsNone = 0,
    BmsBall,
    BmsSafe,
    BmsCover,
    BmsPop
} msg_bms_t;

typedef struct {
    char		txt[MSG_LEN];
    size_t		len;
    /*short		pixelLen;*/
    double		lifeTime;
    msg_bms_t		bmsinfo;
} message_t;
/* typedefs end */

extern client_data_t	clData;

extern bool		newbie;
extern char		*geometry;
extern xp_args_t	xpArgs;
extern Connect_param_t	connectParam;
/** Default endpoint settings for direct client connections. */
extern Connect_defaults_t connectDefaults;
extern message_t	*TalkMsg[];
extern message_t	*GameMsg[];
extern message_t	*TalkMsg_pending[];	/* store incoming messages */
extern message_t	*GameMsg_pending[];	/* while a cut is pending */
extern char		*HistoryMsg[];		/* talk window history */

extern int		maxLinesInHistory;	/* lines to save in history */
extern selection_t	selection;		/* in talk/draw window */
extern int		maxMessages;
extern int		messagesToStdout;

extern char		*talk_fast_msgs[];	/* talk macros */

extern score_object_t	score_objects[MAX_SCORE_OBJECTS];
extern int		score_object;

extern int      oldServer; /* Compatibility mode for old block-based servers */
extern ipos_t	selfPos;
extern ipos_t	selfVel;
extern short	heading;
extern short	nextCheckPoint;
extern u_byte	numItems[NUM_ITEMS];
extern u_byte	lastNumItems[NUM_ITEMS];
extern int	numItemsTime[NUM_ITEMS];
extern double	showItemsTime;
extern short	autopilotLight;
extern int	showScoreDecimals;
extern double   scoreObjectTime;        /* How long to show score objects */


extern short	lock_id;		/* Id of player locked onto */
extern short	lock_dir;		/* Direction of lock */
extern short	lock_dist;		/* Distance to player locked onto */

extern int	eyesId;		        /* Player we get frame updates for */
extern other_t	*eyes;        		/* Player we get frame updates for */
extern bool	snooping;	        /* are we snooping on someone else? */
extern int	eyeTeam;	        /* Team of player we get updates for */

extern other_t*	self;			/* Player info */
extern short	selfVisible;		/* Are we alive and playing? */
extern short	damaged;		/* Damaged by ECM */
extern short	destruct;		/* If self destructing */
extern short	shutdown_delay;
extern short	shutdown_count;
extern short	thrusttime;
extern short	thrusttimemax;
extern short	shieldtime;
extern short	shieldtimemax;
extern short	phasingtime;
extern short	phasingtimemax;

extern int	roundDelay;
extern int	roundDelayMax;

extern bool	UpdateRadar;
extern unsigned	RadarWidth;
extern unsigned	RadarHeight;
extern int	backgroundPointDist;	/* spacing of navigation points */
extern int	backgroundPointSize;	/* size of navigation points */
extern int	sparkSize;		/* size of sparks and debris */
extern int	shotSize;		/* size of shot */
extern int	teamShotSize;		/* size of team shot */

extern double	controlTime;		/* Display control for how long? */
extern u_byte	spark_rand;		/* Sparkling effect */
extern u_byte	old_spark_rand;		/* previous value of spark_rand */

extern double	fuelSum;		/* Sum of fuel in all tanks */
extern double	fuelMax;		/* How much fuel can you take? */
extern short	fuelCurrent;		/* Number of currently used tank */
extern short	numTanks;		/* Number of tanks */
extern double	fuelTime;		/* Display fuel for how long? */
extern double	fuelCritical;		/* Fuel critical level */
extern double	fuelWarning;		/* Fuel warning level */
extern double	fuelNotify;		/* Fuel notify level */

extern char	*shipShape;		/* Shape of player's ship */
extern double	power;			/* Force of thrust */
extern double	power_s;		/* Saved power fiks */
extern double	turnspeed;		/* How fast player acc-turns */
extern double	turnspeed_s;		/* Saved turnspeed */
extern double	turnresistance;		/* How much is lost in % */
extern double	turnresistance_s;	/* Saved (see above) */
extern double	displayedPower;		/* What the server is sending us */
extern double	displayedTurnspeed;	/* What the server is sending us */
extern double	displayedTurnresistance;/* What the server is sending us */
extern double	sparkProb;		/* Sparkling effect configurable */
extern int	charsPerSecond;		/* Message output speed (config) */

extern double	hud_move_fact;		/* scale the hud-movement (speed) */
extern double	ptr_move_fact;		/* scale the speed pointer length */
extern char	mods[MAX_CHARS];	/* Current modifiers in effect */
extern instruments_t	instruments;	/* Instruments on screen */
extern int	packet_size;		/* Current frame update packet size */
extern int	packet_loss;		/* lost packets per second */
extern int	packet_drop;		/* dropped packets per second */
extern int	packet_lag;		/* approximate lag in frames */
extern char	*packet_measure;	/* packet measurement in a second */
extern long	packet_loop;		/* start of measurement */

extern bool	showUserName;		/* Show nick=user@host details */
extern char	servername[MAX_CHARS];	/* Name of server connecting to */
extern unsigned	version;		/* Version of the server */
extern bool	scoresChanged;
extern bool	toggle_shield;		/* Are shields toggled by a press? */
extern bool	shields;		/* When shields are considered up */
extern bool	auto_shield;            /* drops shield for fire */

extern int	maxFPS;			/* Max FPS player wants from server */
extern int 	oldMaxFPS;
extern double	clientFPS;		/* FPS client is drawing at */
extern int	recordFPS;		/* What FPS to record at */
extern time_t	currentTime;	        /* Current value of time() */
extern bool	newSecond;              /* Did time() increment this frame? */
extern int	maxMouseTurnsPS;
extern int	mouseMovementInterval;
extern int	cumulativeMouseMovement;

extern char	modBankStr[][MAX_CHARS];/* modifier banks strings */

extern int	clientPortStart;	/* First network port for clients */
extern int	clientPortEnd;		/* Last one (these are for firewalls) */
extern int	baseWarningType;	/* Which type of base warning you prefer */
extern int	maxCharsInNames;
extern u_byte	lose_item;		/* flag and index to drop item */
extern int	lose_item_active;	/* one of the lose keys is pressed */

/* mapdata accessible to outside world */

extern int	        num_playing_teams;

extern fuelstation_t	*fuels;
extern int		num_fuels;
extern homebase_t	*bases;
extern int		num_bases;
extern checkpoint_t	*checks;
extern int		num_checks;
extern xp_polygon_t	*polygons;
extern int		num_polygons, max_polygons;
extern edge_style_t	*edge_styles;
extern int		num_edge_styles, max_edge_styles;
extern polygon_style_t	*polygon_styles;
extern int		num_polygon_styles, max_polygon_styles;

/* dynamic global game data */

extern other_t          *Others;
extern int	        num_others, max_others;
extern refuel_t		*refuel_ptr;
extern int		 num_refuel, max_refuel;
extern connector_t	*connector_ptr;
extern int		 num_connector, max_connector;
extern laser_t		*laser_ptr;
extern int		 num_laser, max_laser;
extern missile_t	*missile_ptr;
extern int		 num_missile, max_missile;
extern ball_t		*ball_ptr;
extern int		 num_ball, max_ball;
extern ship_t		*ship_ptr;
extern int		 num_ship, max_ship;
extern mine_t		*mine_ptr;
extern int		 num_mine, max_mine;
extern itemtype_t	*itemtype_ptr;
extern int		 num_itemtype, max_itemtype;
extern ecm_t		*ecm_ptr;
extern int		 num_ecm, max_ecm;
extern trans_t		*trans_ptr;
extern int		 num_trans, max_trans;
extern paused_t		*paused_ptr;
extern int		 num_paused, max_paused;
extern appearing_t	*appearing_ptr;
extern int		 num_appearing, max_appearing;
extern radar_t		*radar_ptr;
extern int		 num_radar, max_radar;
extern vcannon_t	*vcannon_ptr;
extern int		 num_vcannon, max_vcannon;
extern vfuel_t		*vfuel_ptr;
extern int		 num_vfuel, max_vfuel;
extern vbase_t		*vbase_ptr;
extern int		 num_vbase, max_vbase;
extern debris_t		*debris_ptr[DEBRIS_TYPES];
extern int		 num_debris[DEBRIS_TYPES],
			 max_debris[DEBRIS_TYPES];
extern debris_t		*fastshot_ptr[DEBRIS_TYPES * 2];
/** Cumulative received shot particles, including those already rendered. */
extern unsigned long received_fastshots;
extern int		 num_fastshot[DEBRIS_TYPES * 2],
			 max_fastshot[DEBRIS_TYPES * 2];
extern vdecor_t		*vdecor_ptr;
extern int		 num_vdecor, max_vdecor;
extern wreckage_t	*wreckage_ptr;
extern int		 num_wreckage, max_wreckage;
extern asteroid_t	*asteroid_ptr;
extern int		 num_asteroids, max_asteroids;
extern wormhole_t	*wormhole_ptr;
extern int		 num_wormholes, max_wormholes;

extern long		start_loops, end_loops;
extern long		time_left;

extern bool roundend;
extern bool played_this_round;

/*
 * somewhere
 */
const char *Program_name(void);
int Bitmap_add(const char *filename, int count, bool scalable);
void Pointer_control_newbie_message(void);

/*
 * Platform specific code needs to implement these.
 */
void Platform_specific_pointer_control_set_state(bool on);
void Platform_specific_talk_set_state(bool on);
void Record_toggle(void);
void Toggle_fullscreen(void);
void Toggle_radar_and_scorelist(void);

/*
 * event.c
 */
void Pointer_control_set_state(bool on);
void Talk_set_state(bool on);

void Pointer_button_pressed(int button);
void Pointer_button_released(int button);
void Keyboard_button_pressed(xp_keysym_t ks);
void Keyboard_button_released(xp_keysym_t ks);

int Key_init(void);
int Key_update(void);
void Key_clear_counts(void);
/** @return Number of logically held game actions, excluding default shields. */
int Key_held_count(void);
bool Key_press(keys_t key);
bool Key_release(keys_t key);
void Set_auto_shield(bool on);
void Set_toggle_shield(bool on);

/*
 * messages.c
 */
bool Bms_test_state(msg_bms_t bms);
void Bms_set_state(msg_bms_t bms);
int Alloc_msgs(void);
void Free_msgs(void);
int Alloc_history(void);
void Free_selectionAndHistory(void);
void Add_message(const char *message);
void Add_newbie_message(const char *message);
extern void Add_alert_message(const char *message, double timeout);
extern void Clear_alert_messages(void);
void Add_pending_messages(void);
void Add_roundend_messages(other_t **order);
void Print_messages_to_stdout(void);

/*
 * client.c
 */
double Fuel_by_pos(int x, int y);
int Target_alive(int x, int y, double *damage);
int Target_by_index(int ind, int *xp, int *yp, int *dead_time, double *damage);
int Handle_fuel(int ind, double fuel);
int Cannon_dead_time_by_pos(int x, int y, int *dot);
int Handle_cannon(int ind, int dead_time);
int Handle_target(int num, int dead_time, double damage);
int Base_info_by_pos(int x, int y, int *id, int *team);
int Handle_base(int id, int ind);
int Check_pos_by_index(int ind, int *xp, int *yp);
int Check_index_by_pos(int x, int y);
homebase_t *Homebase_by_id(int id);
other_t *Other_by_id(int id);
other_t *Other_by_name(const char *name, bool show_error_msg);
shipshape_t *Ship_by_id(int id);
int Handle_leave(int id);
int Handle_player(int id, int team, int mychar,
		  char *nick_name, char *user_name, char *host_name,
		  char *shape, int myself);
int Handle_team(int id, int pl_team);
int Handle_score(int id, double score, int life, int mychar, int alliance);
int Handle_score_object(double score, int x, int y, char *msg);
int Handle_team_score(int team, double score);
int Handle_timing(int id, int check, int round, long loops);
int Handle_seek(int programmer_id, int robot_id, int sought_id);
int Handle_start(long server_loops);
int Handle_end(long server_loops);
int Handle_self(int x, int y, int vx, int vy, int newHeading,
		double newPower, double newTurnspeed, double newTurnresistance,
		int newLockId, int newLockDist, int newLockBearing,
		int newNextCheckPoint, int newAutopilotLight,
		u_byte *newNumItems, int newCurrentTank,
		double newFuelSum, double newFuelMax, int newPacketSize, 
		int status);
int Handle_self_items(u_byte *newNumItems);
int Handle_modifiers(char *m);
int Handle_damaged(int dam);
int Handle_destruct(int count);
int Handle_shutdown(int count, int delay);
int Handle_thrusttime(int count, int max);
int Handle_shieldtime(int count, int max);
int Handle_phasingtime(int count, int max);
int Handle_rounddelay(int count, int max);
int Handle_refuel(int x_0, int y_0, int x_1, int y_1);
int Handle_connector(int x_0, int y_0, int x_1, int y_1, int tractor);
int Handle_laser(int color, int x, int y, int len, int dir);
int Handle_missile(int x, int y, int dir, int len);
int Handle_ball(int x, int y, int id, int style);
int Handle_ship(int x, int y, int id, int dir, int shield, int cloak,
		int eshield, int phased, int deflector);
int Handle_mine(int x, int y, int teammine, int id);
int Handle_item(int x, int y, int type);
int Handle_fastshot(int type, u_byte *p, int n);
int Handle_debris(int type, u_byte *p, int n);
int Handle_wreckage(int x, int y, int wrecktype, int size, int rotation);
int Handle_asteroid(int x, int y, int type, int size, int rotation);
int Handle_wormhole(int x, int y);
int Handle_polystyle(int polyind, int newstyle);
int Handle_ecm(int x, int y, int size);
int Handle_trans(int x_1, int y_1, int x_2, int y_2);
int Handle_paused(int x, int y, int count);
int Handle_appearing(int x, int y, int id, int count);
int Handle_radar(int x, int y, int size);
int Handle_fastradar(int x, int y, int size);
int Handle_vcannon(int x, int y, int type);
int Handle_vfuel(int x, int y, double fuel);
int Handle_vbase(int x, int y, int xi, int yi, int type);
int Handle_vdecor(int x, int y, int xi, int yi, int type);
int Handle_message(char *msg);
int Handle_eyes(int id);
int Handle_time_left(long sec);
void Map_dots(void);
void Map_restore(int startx, int starty, int width, int height);
void Map_blue(int startx, int starty, int width, int height);
bool Using_score_decimals(void);
int Client_init(char *server, unsigned server_version);
int Client_setup(void);
void Client_cleanup(void);
int Client_start(void);
int Client_fps_request(void);
int Client_power(void);
int Client_pointer_move(int movement);
int Client_check_pointer_move_interval(void);
void Client_exit(int status);

int Init_playing_windows(void);
void Raise_window(void);
void Reset_shields(void);
void Platform_specific_cleanup(void);

#ifdef _WINDOWS
void MarkPlayersForRedraw(void);
#endif

int Check_client_fps(void);

/*
 * about.c
 */
extern int Handle_motd(long off, char *buf, int len, long filesize);
extern void aboutCleanup(void);

#ifdef _WINDOWS
extern	void Motd_destroy(void);
extern	void Keys_destroy(void);
#endif

extern int motd_viewer;		/* so Windows can clean him up */
extern int keys_viewer;


extern void Colors_init_style_colors(void);

/*
 * default.c
 */
extern void Store_default_options(void);
extern void defaultCleanup(void);			/* memory cleanup */

extern bool Set_scaleFactor(xp_option_t *opt, double val);
extern bool Set_altScaleFactor(xp_option_t *opt, double val);

#ifdef _WINDOWS
extern char *Get_xpilotini_file(int level);
#endif

/*
 * event.c
 */
extern void Store_key_options(void);

/*
 * join.c
 */
extern int Join(Connect_param_t *conpar);
extern void xpilotShutdown(void);

/*
 * mapdata.c
 */
/**
 * Makes the texture package referenced by a map available to the client.
 *
 * An already extracted package is reused first.  A bundled package whose
 * filename matches the URL is copied and extracted before a network download
 * is attempted.
 *
 * @param urlstr HTTP URL supplied by the map.
 * @return Nonzero when the map texture directory is available, zero on error.
 */
extern int Mapdata_setup(const char *);


/*
 * metaclient.c
 */
extern int metaclient(int, char **);


/*
 * paintdata.c
 */
extern void paintdataCleanup(void);		/* memory cleanup */


/*
 * paintobjects.c
 */
extern int Init_wreckage(void);
extern int Init_asteroids(void);


/*
 * query.c
 */
extern int Query_all(sock_t *sockfd, int port, char *msg, size_t msglen);



/*
 * textinterface.c
 */
extern int Connect_to_server(int auto_connect, int list_servers,
			     int auto_shutdown, char *shutdown_reason,
			     Connect_param_t *conpar);
/**
 * Contact explicit endpoints in order until one joins successfully.
 *
 * A failed attempt is isolated from subsequent targets. `conpar` is updated
 * with endpoint and negotiated connection data only after a successful join.
 *
 * @param count Number of entries in `targets`.
 * @param targets Connection targets to try in command-line order.
 * @param auto_connect Whether to request joining without an interactive prompt.
 * @param list_servers Whether to request and print server status.
 * @param auto_shutdown Whether to request server shutdown.
 * @param shutdown_message Shutdown reason used when `auto_shutdown` is set.
 * @param conpar Client identity input and successful connection output.
 * @return Nonzero after a successful join, otherwise zero.
 */
extern int Contact_servers(int count, const Connect_target_t *targets,
			   int auto_connect, int list_servers,
			   int auto_shutdown, char *shutdown_message,
			   Connect_param_t *conpar);
/**
 * Discover and optionally contact servers using UDP LAN broadcast.
 *
 * @param defaults Default contact port and expected gameplay transport.
 * @param auto_connect Whether to request joining without an interactive prompt.
 * @param list_servers Set to 2 to collect discovery results, or 1 to print
 * server status; zero performs the normal contact interaction.
 * @param auto_shutdown Whether to request server shutdown.
 * @param shutdown_message Shutdown reason used when `auto_shutdown` is set.
 * @param find_max Capacity of each optional discovery output array.
 * @param num_found Receives the number of stored discovery results, or `NULL`.
 * @param server_addresses Optional array of `MAX_HOST_LEN`-byte buffers.
 * @param server_names Optional array of `MAX_HOST_LEN`-byte buffers.
 * @param server_versions Optional protocol-version output array.
 * @param conpar Client identity input and successful connection output.
 * @return Nonzero after a successful join, otherwise zero. Discovery-only
 * operation reports its result count through `num_found` and returns zero.
 */
extern int Contact_local_servers(const Connect_defaults_t *defaults,
				 int auto_connect, int list_servers,
				 int auto_shutdown, char *shutdown_message,
				 int find_max, int *num_found,
				 char **server_addresses, char **server_names,
				 unsigned *server_versions,
				 Connect_param_t *conpar);

/*
 * welcome.c
 */
extern int Welcome_screen(Connect_param_t *conpar);

/*
 * widget.c
 */
extern void Widget_cleanup(void);

/*
 * xinit.c
 */
#ifdef _WINDOWS
extern	void WinXCreateItemBitmaps();
#endif

#endif
