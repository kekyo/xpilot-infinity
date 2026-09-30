/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2000-2004 by
 *
 *      Uoti Urpala          <uau@users.sourceforge.net>
 *      Kristian Söderblom   <kps@users.sourceforge.net>
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

/* Options parsing code contributed by Ted Lemon <mellon@ncd.com> */

#include "xpserver.h"

double		friction;
double		coriolisCosine, coriolisSine;	/* cos and sin of cor. angle */
int		roundsPlayed;		/* # of rounds played sofar. */
extern char	conf_logfile_string[];	/* Default name of log file */

double		timeStep;		/* Game time step per frame */
double		timePerFrame;		/* Real time elapsed per frame */
double		ecmSizeFactor;		/* Factor for ecm size update */
struct options	options;

/*
 * Two functions which can be used if an option does not have its own
 * function which should be called after the option value has been
 * changed during runtime.  The tuner_none function should be
 * specified when an option cannot be changed at all during runtime.
 * The tuner_dummy can be specified if it is OK to modify the option
 * during runtime and no follow up action is needed.
 */
void tuner_none(void)  { ; }
void tuner_dummy(void) { ; }


static void Tune_robot_user_name(void)
{
    Fix_user_name(options.robotUserName);
}
static void Tune_robot_host_name(void)
{
    UNUSED_PARAM(world);
    Fix_host_name(options.robotHostName);
}
static void Tune_tank_user_name(void)
{
    UNUSED_PARAM(world);
    Fix_user_name(options.tankUserName);
}
static void Tune_tank_host_name(void)
{
    UNUSED_PARAM(world);
    Fix_host_name(options.tankHostName);
}
static void Tune_tagGame(void)
{
    UNUSED_PARAM(world);
    if (!options.tagGame)
	tagItPlayerId = NO_ID;
}


static void Check_baseless(void);

static option_desc opts[] = {
    {
	"help",
	"help",
	"0",
	NULL,
	valVoid,
	tuner_none,
	"Print out this help message.\n",
	OPT_NONE
    },
    {
	"version",
	"version",
	"0",
	NULL,
	valVoid,
	tuner_none,
	"Print version information.\n",
	OPT_NONE
    },
    {
	"dump",
	"dump",
	"0",
	NULL,
	valVoid,
	tuner_none,
	"Print all options and their default values in defaultsfile format.\n",
	OPT_NONE
    },
    {
	"expand",
	"expand",
	"",
	&options.expandList,
	valList,
	tuner_none,
	"Expand a comma separated list of predefined settings.\n"
	"These settings can be defined in either the defaults file\n"
	"or in a map file using the \"define:\" operator.\n",
	OPT_COMMAND
    },
    {
	"gravity",
	"gravity",
	"0.0",
	&options.gravity,
	valReal,
	Compute_gravity,
	"Gravity strength.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shipMass",
	"shipMass",
	"20.0",
	&options.shipMass,
	valReal,
	tuner_shipmass,
	"Mass of fighters.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballMass",
	"ballMass",
	"50.0",
	&options.ballMass,
	valReal,
	tuner_ballmass,
	"Mass of balls.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minItemMass",
	"minItemMass",
	"0.0",		/* 4.5.5beta has default of 1.0 */
	&options.minItemMass,
	valReal,
	tuner_dummy,
	"The minimum mass per item when carried by a ship.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotMass",
	"shotMass",
	"0.1",
	&options.shotMass,
	valReal,
	tuner_dummy,
	"Mass of bullets.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotSpeed",
	"shotSpeed",
	"21.0",
	&options.shotSpeed,
	valReal,
	tuner_dummy,
	"Maximum speed of bullets.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotLife",
	"shotLife",
	"60.0",
	&options.shotLife,
	valReal,
	tuner_dummy,
	"Life of bullets in ticks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"treasureCollisionKills",
	"treasureKills",
	"true",
	&options.treasureCollisionKills,
	valBool,
	tuner_dummy,
	"Does a player die when hitting a ballarea?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballCollisionFuelDrain",
	"ballFuelDrain",
	"50",
	&options.ballCollisionFuelDrain,
	valReal,
	tuner_dummy,
	"How much fuel does a ball collision cost?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerCollisionFuelDrain",
	"playerFuelDrain",
	"100",
	&options.playerCollisionFuelDrain,
	valReal,
	tuner_dummy,
	"How much fuel does a player collision cost?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotHitFuelDrainUsesKineticEnergy",
	"kineticEnergyFuelDrain",
	"true",
	&options.shotHitFuelDrainUsesKineticEnergy,
	valBool,
	tuner_dummy,
	"Does the fuel drain from shot hits depend on their mass and speed?\n"
	"This should be set to false on Blood's Music maps.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"fireRepeatRate",
	"fireRepeat",
	"2.0",
	&options.fireRepeatRate,
	valReal,
	tuner_dummy,
	"Number of ticks per automatic fire (0=off).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pulseSpeed",
	"pulseSpeed",
	"90.0",
	&options.pulseSpeed,
	valReal,
	tuner_dummy,
	"Speed of laser pulses.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pulseLife",
	"pulseLife",
	"6.0",
	&options.pulseLife,
	valReal,
	tuner_dummy,
	"Life of laser pulses shot by ships, in ticks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pulseLength",
	"pulseLength",
	"85.0",
	&options.pulseLength,
	valReal,
	tuner_dummy,
	"Max length of laser pulse.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"laserRepeatRate",
	"laserRepeat",
	"2.0",
	&options.laserRepeatRate,
	valReal,
	tuner_dummy,
	"Number of ticks per automatic laser pulse fire (0=off).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    /* kps - this is stupid */
    /* 1. default should be 0 */
    /* 2. robots = 4 should mean 4 robots, not 4 - numplayers robots */
    {
	"maxRobots",
	"robots",
	"4",
	&options.maxRobots,
	valInt,
	tuner_maxrobots,
	"The maximum number of robots wanted.\n"
	"Adds robots if there are less than maxRobots players.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minRobots",
	"minRobots",
	"-1",
	&options.minRobots,
	valInt,
	tuner_minrobots,
	"The minimum number of robots wanted.\n"
	"At least minRobots robots will be in the game, if there is room.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotFile",
	"robotFile",
	NULL,
	&options.robotFile,
	valString,
	tuner_none,
	"The file containing parameters for robot details.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"robotsTalk",
	"robotsTalk",
	"false",
	&options.robotsTalk,
	valBool,
	tuner_dummy,
	"Do robots talk when they kill, die etc.?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotsLeave",
	"robotsLeave",
	"true",
	&options.robotsLeave,
	valBool,
	tuner_dummy,
	"Do robots leave the game?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotLeaveLife",
	"robotLeaveLife",
	"50",
	&options.robotLeaveLife,
	valInt,
	tuner_dummy,
	"After how many deaths does a robot want to leave? (0=off).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotTeam",
	"robotTeam",
	"0",
	&options.robotTeam,
	valInt,
	tuner_dummy,
	"Team to use for robots.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"restrictRobots",
	"restrictRobots",
	"true",
	&options.restrictRobots,
	valBool,
	tuner_dummy,
	"Are robots restricted to their own team?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"reserveRobotTeam",
	"reserveRobotTeam",
	"true",
	&options.reserveRobotTeam,
	valBool,
	tuner_dummy,
	"Is the robot team only for robots?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotUserName",
	"robotRealName",
	"robot",
	&options.robotUserName,
	valString,
	Tune_robot_user_name,
	"What is the robots' apparent user name?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"robotHostName",
	"robotHostName",
	"xpilot.sourceforge.net",
	&options.robotHostName,
	valString,
	Tune_robot_host_name,
	"What is the robots' apparent host name?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tankUserName",
	"tankRealName",
	"tank",
	&options.tankUserName,
	valString,
	Tune_tank_user_name,
	"What is the tanks' apparent user name?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tankHostName",
	"tankHostName",
	"xpilot.sourceforge.net",
	&options.tankHostName,
	valString,
	Tune_tank_host_name,
	"What is the tanks' apparent host name?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tankScoreDecrement",
	"tankDecrement",
	"500.0",
	&options.tankScoreDecrement,
	valReal,
	tuner_dummy,
	"How much lower is the tank's score than the player's?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"selfImmunity",
	"selfImmunity",
	"false",
	&options.selfImmunity,
	valBool,
	tuner_dummy,
	"Are players immune to their own weapons?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"defaultShipShape",
	"defaultShipShape",
	"(NM:Default)(AU:Unknown)(SH: 14,0 -8,8 -8,-8)(MG: 14,0)(LG: 14,0)"
	"(RG: 14,0)(EN: -8,0)(LR: -8,8)(RR: -8,-8)(LL: -8,8)(RL: -8,-8)"
	"(MR: 14,0)",
	&options.defaultShipShape,
	valString,
	tuner_none,
	"What is the default ship shape for people who do not have a ship\n"
	"shape defined?",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tankShipShape",
	"tankShipShape",
	"(NM:fueltank)(AU:John E. Norlin)"
	"(SH: 15,0 14,-5 9,-8 -5,-8 -3,-8 -3,0 "
	"2,0 2,2 -3,2 -3,6 5,6 5,8 -5,8 -5,-8 "
	"-9,-8 -14,-5 -15,0 -14,5 -9,8 9,8 14,5)"
	"(EN: -15,0)(MG: 15,0)"
	/*"(NM:fueltank)"
	  "(SH: 15,0 14,5 9,8 -9,8 -14,5 -15,0 -14,-5 -9,-8 9,-8 14,-5)"
	  "(EN: -15,0)(MG: 15,0)"*/,
	&options.tankShipShape,
	valString,
	tuner_none,
	"What is the ship shape used for tanks?",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxPlayerShots",
	"shots",
	"256",
	&options.maxPlayerShots,
	valInt,
	tuner_dummy,
	"Maximum number of shots visible at the same time per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotsGravity",
	"shotsGravity",
	"true",
	&options.shotsGravity,
	valBool,
	tuner_dummy,
	"Are shots affected by gravity.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"Log",
	"Log",
	"false",
	&options.Log,
	valBool,
	tuner_dummy,
	"Log major server events to log file?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"idleRun",
	"rawMode",
	"false",
	&options.RawMode,
	valBool,
	tuner_dummy,
	"Does server calculate frames and do robots keep on playing even\n"
	"if all human players quit?\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"noQuit",
	"noQuit",
	"false",
	&options.NoQuit,
	valBool,
	tuner_dummy,
	"Does the server wait for new human players to show up\n"
	"after all players have left.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"logRobots",
	"logRobots",
	"true",
	&options.logRobots,
	valBool,
	tuner_dummy,
	"Log the comings and goings of robots.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"mapWidth",
	"mapWidth",
	"3500",
	&options.mapWidth,
	valInt,
	tuner_none,
	"Width of the world in pixels.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mapHeight",
	"mapHeight",
	"3500",
	&options.mapHeight,
	valInt,
	tuner_none,
	"Height of the world in pixels.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mapFileName",
	"map",
	NULL,
	&options.mapFileName,
	valString,
	tuner_none,
	"The filename of the map to use.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"strictMap",
	"strictMap",
	"false",
	&options.strictMap,
	valBool,
	tuner_none,
	"Require the specified map; fail startup instead of using the default map.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"mapName",
	"mapName",
	"unknown",
	&options.mapName,
	valString,
	tuner_none,
	"The title of the map.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mapAuthor",
	"mapAuthor",
	"anonymous",
	&options.mapAuthor,
	valString,
	tuner_none,
	"The name of the map author.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mapData",
	"mapData",
	NULL,
	&options.mapData,
	valString,
	tuner_none,
	"Block map topology.\n",
	OPT_MAP
    },
    {
	"contactPort",
	"port",
	"15345",
	&options.contactPort,
	valInt,
	tuner_none,
	"The server contact port number.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"contactTransport",
	"contactTransport",
	"udp",
	&options.contactTransport,
	valString,
	tuner_dummy,
	"Contact and lobby transport for direct connections: udp, tcp, or "
	"websocket.\n"
	"This value must match the client; there is no automatic fallback.\n"
	"WebSocket must also be selected for gameplay.\n"
	"LAN broadcast discovery is available only in udp mode.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"gameTransport",
	"gameTransport",
	"udp",
	&options.gameTransport,
	valString,
	tuner_dummy,
	"Gameplay transport for player connections: udp, tcp, or websocket.\n"
	"This value must match every client; there is no automatic fallback.\n"
	"WebSocket must also be selected for contact and lobby traffic.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"serverHost",
	"serverHost",
	NULL,
	&options.serverHost,
	valString,
	tuner_none,
	"The server's fully qualified domain name (for multihomed hosts).\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"greeting",
	"xpilotGreeting",
	NULL,
	&options.greeting,
	valString,
	tuner_dummy,
	"Short greeting string for players when they login to server.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowPlayerCrashes",
	"allowPlayerCrashes",
	"true",
	&options.allowPlayerCrashes,
	valBool,
	Set_world_rules,
	"Can players overrun other players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowPlayerBounces",
	"allowPlayerBounces",
	"true",
	&options.allowPlayerBounces,
	valBool,
	Set_world_rules,
	"Can players bounce with other players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowPlayerKilling",
	"killings",
	"true",
	&options.allowPlayerKilling,
	valBool,
	Set_world_rules,
	"Should players be allowed to kill one other?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowShields",
	"shields",
	"true",
	&options.allowShields,
	valBool,
	tuner_allowshields,
	"Are shields allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerStartsShielded",
	"playerStartShielded",
	"true",
	&options.playerStartsShielded,
	valBool,
	tuner_playerstartsshielded,
	"Do players start with shields up?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotsWallBounce",
	"shotsWallBounce",
	"false",
	&options.shotsWallBounce,
	valBool,
	Move_init,
	"Do shots bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballsWallBounce",
	"ballsWallBounce",
	"true",
	&options.ballsWallBounce,
	valBool,
	Move_init,
	"Do balls bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballCollisionDetaches",
	"ballHitDetaches",
	"false",
	&options.ballCollisionDetaches,
	valBool,
	tuner_dummy,
	"Does a ball get freed by a collision with a player?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballCollisions",
	"ballCollisions",
	"false",
	&options.ballCollisions,
	valBool,
	tuner_dummy,
	"Can balls collide with other objects?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballSparkCollisions",
	"ballSparkCollisions",
	"true",
	&options.ballSparkCollisions,
	valBool,
	tuner_dummy,
	"Can balls be blown around by exhaust? (Needs ballCollisions too)\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minesWallBounce",
	"minesWallBounce",
	"false",
	&options.minesWallBounce,
	valBool,
	Move_init,
	"Do mines bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemsWallBounce",
	"itemsWallBounce",
	"true",
	&options.itemsWallBounce,
	valBool,
	Move_init,
	"Do items bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"missilesWallBounce",
	"missilesWallBounce",
	"false",
	&options.missilesWallBounce,
	valBool,
	Move_init,
	"Do missiles bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"sparksWallBounce",
	"sparksWallBounce",
	"true",
	&options.sparksWallBounce,
	valBool,
	Move_init,
	"Do thrust spark particles bounce off walls to give reactive \n"
	"thrust?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"debrisWallBounce",
	"debrisWallBounce",
	"false",
	&options.debrisWallBounce,
	valBool,
	Move_init,
	"Do explosion debris particles bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidsWallBounce",
	"asteroidsWallBounce",
	"true",
	&options.asteroidsWallBounce,
	valBool,
	Move_init,
	"Do asteroids bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pulsesWallBounce",
	"pulsesWallBounce",
	"false",
	&options.pulsesWallBounce,
	valBool,
	Move_init,
	"Do laser pulses bounce off walls?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cloakedExhaust",
	"cloakedExhaust",
	"true",
	&options.cloakedExhaust,
	valBool,
	tuner_dummy,
	"Do engines of cloaked ships generate exhaust?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxObjectWallBounceSpeed",
	"maxObjectBounceSpeed",
	"40.0",
	&options.maxObjectWallBounceSpeed,
	valReal,
	Move_init,
	"The maximum allowed speed for objects to bounce off walls.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxSparkWallBounceSpeed",
	"maxSparkBounceSpeed",
	"80.0", /* was "40.0" */
	&options.maxSparkWallBounceSpeed,
	valReal,
	Move_init,
	"The maximum allowed speed for sparks to bounce off walls.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxShieldedWallBounceSpeed",
	"maxShieldedBounceSpeed",
	"100.0",
	&options.maxShieldedWallBounceSpeed,
	valReal,
	Move_init,
	"The max allowed speed for a shielded player to bounce off walls.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxUnshieldedWallBounceSpeed",
	"maxUnshieldedBounceSpeed",
	"90.0",
	&options.maxUnshieldedWallBounceSpeed,
	valReal,
	Move_init,
	"Max allowed speed for an unshielded player to bounce off walls.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerWallBounceType",
	"wallBounceType",
	"3",
	&options.playerWallBounceType,
	valInt,
	tuner_playerwallbouncetype,
	"What kind of ship wall bounces to use.\n"
	"\n"
	"A value of 0 gives the old XPilot wall bounces, where the ship\n"
	"velocity in the direction perpendicular to the wall is reversed\n"
	"after which the velocity is multiplied by the value of the\n"
	"playerWallBounceBrakeFactor option.\n"
	"\n"
	"A value of 1 gives the \"separate multipliers\" implementation.\n"
	"\n"
	"A value of 2 causes Mara's suggestion for the speed change in the\n"
	"direction parallel to the wall to be used.\n"
	"Vtangent2 = (1-Vnormal1/Vtotal1*wallfriction)*Vtangent1\n"
	"\n"
	"A value of 3 causes Uau's suggestion to be used:\n"
	"change the parallel one by\n"
	"MIN(C1*perpendicular_change, C2*parallel_speed)\n"
	"if you assume the wall has a coefficient of friction C1.\n.",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerWallBounceBrakeFactor",
	"playerWallBrake",
	"0.5",
	&options.playerWallBounceBrakeFactor,
	valReal,
	Move_init,
	"Factor to slow down ship in direction perpendicular to the wall\n"
	"when a wall is hit (0 to 1).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerBallBounceBrakeFactor",
	"playerBallBrake",
	"0.7",
	&options.playerBallBounceBrakeFactor,
	valReal,
	Move_init,
	"Elastic or inelastic properties of the player-ball collision\n"
	"1 means fully elastic, 0 fully inelastic.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playerWallFriction",
	"wallFriction",
	"0.5",
	&options.playerWallFriction,
	valReal,
	Move_init,
	"Player-wall friction (0 to 1).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"objectWallBounceBrakeFactor",
	"objectWallBrake",
	"0.95",
	&options.objectWallBounceBrakeFactor,
	valReal,
	Move_init,
	"Factor to slow down objects when they hit the wall (0 to 1).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"objectWallBounceLifeFactor",
	"objectWallBounceLifeFactor",
	"0.80",
	&options.objectWallBounceLifeFactor,
	valReal,
	Move_init,
	"Factor to reduce the life of objects after bouncing (0 to 1).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"afterburnerPowerMult",
	"afterburnerPower",
	"1.0",
	&options.afterburnerPowerMult,
	valReal,
	tuner_dummy,
	"Multiplication factor for afterburner power.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"wallBounceDestroyItemProb",
	"wallBounceDestroyItemProb",
	"0.0",
	&options.wallBounceDestroyItemProb,
	valReal,
	Move_init,
	"The probability for each item a player owns to get destroyed\n"
	"when the player bounces against a wall.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"reportToMetaServer",
	"reportMeta",
	"true",
	&options.reportToMetaServer,
	valBool,
	tuner_none,
	"Keep the meta server informed about our game?\n"
	"Advertisements include the selected contact and gameplay transports.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"metaUpdateMaxSize",
	"metaUpdateMaxSize",
	"4096",
	&options.metaUpdateMaxSize,
	valInt,
	Meta_update_max_size_tuner,
	"Maximum size of meta update messages.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"searchDomainForXPilot",
	"searchDomainForXPilot",
	"false",
	&options.searchDomainForXPilot,
	valBool,
	tuner_none,
	"Search the local domain for the existence of xpilot.domain?\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"denyHosts",
	"denyHosts",
	"",
	&options.denyHosts,
	valString,
	Set_deny_hosts,
	"List of network addresses of computers which are denied service.\n"
	"Addresses may be followed by a slash and a network mask.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"limitedVisibility",
	"limitedVisibility",
	"false",
	&options.limitedVisibility,
	valBool,
	Set_world_rules,
	"Should the players have a limited visibility?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minVisibilityDistance",
	"minVisibility",
	"0.0",
	&options.minVisibilityDistance,
	valReal,
	tuner_dummy,
	"Minimum block distance for limited visibility, 0 for default.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxVisibilityDistance",
	"maxVisibility",
	"0.0",
	&options.maxVisibilityDistance,
	valReal,
	tuner_dummy,
	"Maximum block distance for limited visibility, 0 for default.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"limitedLives",
	"limitedLives",
	"false",
	&options.limitedLives,
	valBool,
	tuner_none,
	"Should players have limited lives?\n"
	"See also worldLives.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"worldLives",
	"lives",
	"0",
	&options.worldLives,
	valInt,
	tuner_worldlives,
	"Number of lives each player has (no sense without limitedLives).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"reset",
	"reset",
	"true",
	&options.endOfRoundReset,
	valBool,
	tuner_dummy,
	"Does the world reset when the end of round is reached?\n"
	"When true all mines, missiles, shots and explosions will be\n"
	"removed from the world and all players including the winner(s)\n"
	"will be transported back to their homebases.\n"
	"This option is only effective when limitedLives is turned on.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"resetOnHuman",
	"humanReset",
	"0",
	&options.resetOnHuman,
	valInt,
	tuner_dummy,
	"Normally, new players have to wait until a round is finished\n"
	"before they can start playing. With this option, the first N\n"
	"human players to enter will cause the round to be restarted.\n"
	"In other words, if this option is set to 0, nothing special\n"
	"happens and you have to wait as usual until the round ends (if\n"
	"there are rounds at all, otherwise this option doesn't do\n"
	"anything). If it is set to 1, the round is ended when the first\n"
	"human player enters. This is useful if the robots have already\n"
	"started a round and you don't want to wait for them to finish.\n"
	"If it is set to 2, this also happens for the second human player.\n"
	"This is useful when you got bored waiting for another player to\n"
	"show up and have started playing against the robots. When someone\n"
	"finally joins you, they won't have to wait for you to finish the\n"
	"round before they can play too. For higher numbers it works the\n"
	"same. So this option gives the last human player for whom the\n"
	"round is restarted. Anyone who enters after that does have to\n"
	"wait until the round is over.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowAlliances",
	"alliances",
	"true",
	&options.allowAlliances,
	valBool,
	tuner_allowalliances,
	"Are alliances between players allowed?\n"
	"Alliances are like teams, except they can be formed and dissolved\n"
	"at any time. Notably, teamImmunity works for alliances too.\n"
	"To manage alliances, use the '/ally' talk command:\n"
	"'/ally invite <player name>' to invite another player to join you.\n"
	"'/ally cancel' to cancel such an invitation.\n"
	"'/ally refuse <player name>' to decline an invitation from a player.\n"
	"'/ally refuse' to decline all the invitations you received.\n"
	"'/ally accept <player name>' to join the other player.\n"
	"'/ally accept' to accept all the invitations you received.\n"
	"'/ally leave' to leave the alliance you are currently in.\n"
	"'/ally list' lists the members of your current alliance.\n"
	"If members from different alliances join, all their allies do so.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"announceAlliances",
	"announceAlliances",
	"false",
	&options.announceAlliances,
	valBool,
	tuner_announcealliances,
	"Are changes in alliances made public?\n"
	"If this option is on, changes in alliances are sent to all players\n"
	"and all alliances are shown in the score list. Invitations for\n"
	"alliances are never sent to anyone but the invited players.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"teamPlay",
	"teams",
	"false",
	&options.teamPlay,
	valBool,
	tuner_none,
	"Is the map a team play map?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"lockOtherTeam",
	"lockOtherTeam",
	"true",
	&options.lockOtherTeam,
	valBool,
	tuner_dummy,
	"Can you watch opposing players when rest of your team is \n"
	"still alive?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"teamFuel",
	"teamFuel",
	"false",
	&options.teamFuel,
	valBool,
	tuner_dummy,
	"Do fuelstations belong to teams?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"teamCannons",
	"teamCannons",
	"false",
	&options.teamCannons,
	valBool,
	tuner_teamcannons,
	"Do cannons belong to teams?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonSmartness",
	"cannonSmartness",
	"1",
	&options.cannonSmartness,
	valInt,
	tuner_cannonsmartness,
	"0: dumb (straight ahead),\n"
	"1: default (random direction),\n"
	"2: good (small error),\n"
	"3: accurate (aims at predicted player position).\n"
	"Also influences use of weapons.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonsPickupItems",
	"cannonsPickupItems",
	"false",
	&options.cannonsPickupItems,
	valBool,
	Move_init,
	"Do cannons pick up items?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonFlak",
	"cannonAAA",
	"true",
	&options.cannonFlak,
	valBool,
	tuner_dummy,
	"Do cannons fire flak or normal bullets?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonDeadTicks",
	"cannonDeadTicks",
	"864.0",		/* 72 seconds at gamespeed 12 */
	&options.cannonDeadTicks,
	valReal,
	tuner_dummy,
	"How many ticks do cannons stay dead?\n"
	"Replaces option cannonDeadTime.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonShotSpeed",
	"cannonShotSpeed",
	"21.0",
	&options.cannonShotSpeed,
	valReal,
	tuner_dummy,
	"Speed of cannon shots.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minCannonShotLife",
	"minCannonShotLife",
	"8.0",
	&options.minCannonShotLife,
	valReal,
	tuner_mincannonshotlife,
	"Minimum life of cannon shots, measured in ticks.\n"
	"If this is set to a value greater than maxCannonShotLife, then\n"
	"maxCannonShotLife will be set to that same value.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxCannonShotLife",
	"maxCannonShotLife",
	"32.0",
	&options.maxCannonShotLife,
	valReal,
	tuner_maxcannonshotlife,
	"Maximum life of cannon shots, measured in ticks.\n"
	"If this is set to a value less than minCannonShotLife, then\n"
	"minCannonShotLife will be set to that same value.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialFuel",
	"cannonInitialFuel",
	"0",
	&World.items[ITEM_FUEL].cannon_initial,
	valInt,
	Set_initial_resources,
	"How much fuel cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialTanks",
	"cannonInitialTanks",
	"0",
	&World.items[ITEM_TANK].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many tanks cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialECMs",
	"cannonInitialECMs",
	"0",
	&World.items[ITEM_ECM].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many ECMs cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialArmor",
	"cannonInitialArmors",
	"0",
	&World.items[ITEM_ARMOR].cannon_initial,
	valInt,
	Set_initial_resources,
	"How much armor cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialMines",
	"cannonInitialMines",
	"0",
	&World.items[ITEM_MINE].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many mines cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialMissiles",
	"cannonInitialMissiles",
	"0",
	&World.items[ITEM_MISSILE].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many missiles cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialCloaks",
	"cannonInitialCloaks",
	"0",
	&World.items[ITEM_CLOAK].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many cloaks cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialSensors",
	"cannonInitialSensors",
	"0",
	&World.items[ITEM_SENSOR].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many sensors cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialWideangles",
	"cannonInitialWideangles",
	"0",
	&World.items[ITEM_WIDEANGLE].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many wideangles cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialRearshots",
	"cannonInitialRearshots",
	"0",
	&World.items[ITEM_REARSHOT].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many rearshots cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialAfterburners",
	"cannonInitialAfterburners",
	"0",
	&World.items[ITEM_AFTERBURNER].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many afterburners cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialTransporters",
	"cannonInitialTransporters",
	"0",
	&World.items[ITEM_TRANSPORTER].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many transporters cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialMirrors",
	"cannonInitialMirrors",
	"0",
	&World.items[ITEM_MIRROR].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many mirrors cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialDeflectors",
	"cannonInitialDeflectors",
	"0",
	&World.items[ITEM_DEFLECTOR].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many deflectors cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialHyperJumps",
	"cannonInitialHyperJumps",
	"0",
	&World.items[ITEM_HYPERJUMP].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many hyperjumps cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialPhasings",
	"cannonInitialPhasings",
	"0",
	&World.items[ITEM_PHASING].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many phasing devices cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialLasers",
	"cannonInitialLasers",
	"0",
	&World.items[ITEM_LASER].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many lasers cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialEmergencyThrusts",
	"cannonInitialEmergencyThrusts",
	"0",
	&World.items[ITEM_EMERGENCY_THRUST].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many emergency thrusts cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialTractorBeams",
	"cannonInitialTractorBeams",
	"0",
	&World.items[ITEM_TRACTOR_BEAM].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many tractor/pressor beams cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialAutopilots",
	"cannonInitialAutopilots",
	"0",
	&World.items[ITEM_AUTOPILOT].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many autopilots cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonInitialEmergencyShields",
	"cannonInitialEmergencyShields",
	"0",
	&World.items[ITEM_EMERGENCY_SHIELD].cannon_initial,
	valInt,
	Set_initial_resources,
	"How many emergency shields cannons start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"keepShots",
	"keepShots",
	"false",
	&options.keepShots,
	valBool,
	tuner_dummy,
	"Do shots, mines and missiles remain after their owner leaves?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"teamImmunity",
	"teamImmunity",
	"true",
	&options.teamImmunity,
	valBool,
	Team_immunity_init,
	"Should other team members be immune to various shots thrust etc.?\n"
	"This works for alliances too.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ecmsReprogramMines",
	"ecmsReprogramMines",
	"true",
	&options.ecmsReprogramMines,
	valBool,
	tuner_dummy,
	"Is it possible to reprogram mines with ECMs?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ecmsReprogramRobots",
	"ecmsReprogramRobots",
	"true",
	&options.ecmsReprogramRobots,
	valBool,
	tuner_dummy,
	"Are robots reprogrammed by ECMs instead of blinded?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"targetKillTeam",
	"targetKillTeam",
	"false",
	&options.targetKillTeam,
	valBool,
	tuner_dummy,
	"Do team members die when their last target explodes?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"targetTeamCollision",
	"targetCollision",
	"true",
	&options.targetTeamCollision,
	valBool,
	Target_init,
	"Do team members collide with their own target or not.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"targetSync",
	"targetSync",
	"false",
	&options.targetSync,
	valBool,
	tuner_dummy,
	"Do all the targets of a team reappear/repair at the same time?",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"targetDeadTicks",
	"targetDeadTicks",
	"720.0",		/* 60 seconds at gamespeed 12 */
	&options.targetDeadTicks,
	valReal,
	tuner_dummy,
	"How many ticks do targets stay destroyed?\n"
	"Replaces option targetDeadTime.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"treasureKillTeam",
	"treasureKillTeam",
	"false",
	&options.treasureKillTeam,
	valBool,
	tuner_dummy,
	"Do team members die when their treasure is destroyed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"captureTheFlag",
	"ctf",
	"false",
	&options.captureTheFlag,
	valBool,
	tuner_dummy,
	"Does a team's treasure have to be safe before enemy balls can be\n"
	"cashed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"specialBallTeam",
	"specialBall",
	"-1",
	&options.specialBallTeam,
	valInt,
	tuner_dummy,
	"Balls that belong to this team are 'special' balls that score\n"
	"against all other teams.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"treasureCollisionDestroys",
	"treasureCollisionDestroy",
	"true",
	&options.treasureCollisionDestroys,
	valBool,
	tuner_dummy,
	"Are balls destroyed when a player touches it?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballConnectorSpringConstant",
	"ballConnectorSpringConstant",
	"1650.0", /* legacy value was 1500 */
	&options.ballConnectorSpringConstant,
	valReal,
	tuner_dummy,
	"What is the spring constant for connectors?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballConnectorDamping",
	"ballConnectorDamping",
	"2.0",
	&options.ballConnectorDamping,
	valReal,
	tuner_dummy,
	"What is the damping force on connectors?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxBallConnectorRatio",
	"maxBallConnectorRatio",
	"0.30",
	&options.maxBallConnectorRatio,
	valReal,
	tuner_dummy,
	"How much longer or shorter can a connecter get before it breaks?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballConnectorLength",
	"ballConnectorLength",
	"120.0",
	&options.ballConnectorLength,
	valReal,
	tuner_dummy,
	"How long is a normal connector string?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"connectorIsString",
	"connectorIsString",
	"false",
	&options.connectorIsString,
	valBool,
	tuner_dummy,
	"Can the connector get shorter?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballRadius",
	"ballRadius",
	"10.0",
	&options.ballRadius,
	valReal,
	Ball_line_init,
	"What radius, measured in pixels, the treasure balls have on\n"
	"the server. In traditional XPilot, the ball was treated as a\n"
	"point (radius = 0), but visually appeared on the client with\n"
	"a radius of 10 pixels.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {   "multipleConnectors",
	"multipleBallConnectors",
	"true",
	&options.multipleConnectors,
	valBool,
	tuner_dummy,
	"Can a player connect to multiple balls or just to one?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"treasureCollisionMayKill",
	"treasureUnshieldedCollisionKills",
	"false",
	&options.treasureCollisionMayKill,
	valBool,
	tuner_dummy,
	"Does a ball kill a player when the player touches it unshielded?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"wreckageCollisionMayKill",
	"wreckageUnshieldedCollisionKills",
	"false",
	&options.wreckageCollisionMayKill,
	valBool,
	tuner_dummy,
	"Can ships be destroyed when hit by wreckage?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidCollisionMayKill",
	"asteroidUnshieldedCollisionKills",
	"true",
	&options.asteroidCollisionMayKill,
	valBool,
	tuner_dummy,
	"Can ships be destroyed when hit by an asteroid?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ignore20MaxFPS",
	"ignore20MaxFPS",
	"true",
	&options.ignore20MaxFPS,
	valBool,
	tuner_dummy,
	"Ignore client maxFPS request if it is 20 (old default, too low).\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"tagGame",
 	"tag",
 	"false",
 	&options.tagGame,
 	valBool,
 	Tune_tagGame,
 	"Are we going to play a game of tag?\n"
 	"One player is 'it' (is worth more points when killed than the\n"
 	"others). After this player is killed, the one who killed them\n"
 	"becomes 'it', and so on.\n",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tagKillItScoreMult",
	"tagKillItMult",
 	"10.0",
 	&options.tagKillItScoreMult,
 	valReal,
 	tuner_dummy,
 	"Score multiplier for killing 'it' (tagGame must be on).\n",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tagItKillScoreMult",
	"tagItKillMult",
 	"2.0",
 	&options.tagItKillScoreMult,
 	valReal,
 	tuner_dummy,
 	"Score multiplier when 'it' kills an enemy player\n"
	"(tagGame must be on).\n",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"timing",
	"race",
	"false",
	&options.timing,
	valBool,
	tuner_none,
	"Enable race mode?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballrace",
	"ballrace",
	"false",
	&options.ballrace,
	valBool,
	tuner_dummy,
	"Is timing done for balls (on) or players (off)?\n"
	"Only used if timing is on.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballraceConnected",
	"ballraceConnected",
	"false",
	&options.ballrace_connect,
	valBool,
	tuner_dummy,
	"Should a player be connected to a ball to pass a checkpoint?\n"
	"Only used if timing and ballrace are both on.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"edgeWrap",
	"edgeWrap",
	"false",
	&options.edgeWrap,
	valBool,
	tuner_none,
	"Do objects wrap when they cross the edge of the Universe?.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"edgeBounce",
	"edgeBounce",
	"true",
	&options.edgeBounce,
	valBool,
	tuner_dummy,
	"Do objects bounce when they hit the edge of the Universe?.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"extraBorder",
	"extraBorder",
	"false",
	&options.extraBorder,
	valBool,
	tuner_none,
	"Give map an extra border of wall blocks?.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityPoint",
	"gravityPoint",
	"0,0",
	&options.gravityPoint,
	valIPos,
	Compute_gravity,
	"If the gravity is a point source where does that gravity originate?\n"
	"Specify the point in the form: x,y.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityAngle",
	"gravityAngle",
	"90.0",
	&options.gravityAngle,
	valReal,
	Compute_gravity,
	"If gravity is along a uniform line, at what angle is that line?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityPointSource",
	"gravityPointSource",
	"false",
	&options.gravityPointSource,
	valBool,
	Compute_gravity,
	"Is gravity originating from a single point?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityClockwise",
	"gravityClockwise",
	"false",
	&options.gravityClockwise,
	valBool,
	Compute_gravity,
	"If the gravity is a point source, is it clockwise?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityAnticlockwise",
	"gravityAnticlockwise",
	"false",
	&options.gravityAnticlockwise,
	valBool,
	Compute_gravity,
	"If the gravity is a point source, is it anticlockwise?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gravityVisible",
	"gravityVisible",
	"true",
	&options.gravityVisible,
	valBool,
	tuner_none,
	"Are gravity mapsymbols visible to players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"wormholeVisible",
	"wormholeVisible",
	"true",
	&options.wormholeVisible,
	valBool,
	tuner_none,
	"Are wormhole mapsymbols visible to players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemConcentratorVisible",
	"itemConcentratorVisible",
	"true",
	&options.itemConcentratorVisible,
	valBool,
	tuner_none,
	"Are itemconcentrator mapsymbols visible to players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidConcentratorVisible",
	"asteroidConcentratorVisible",
	"true",
	&options.asteroidConcentratorVisible,
	valBool,
	tuner_none,
	"Are asteroidconcentrator mapsymbols visible to players?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"wormholeStableTicks",
	"wormholeStableTicks",
	"64.0",
	&options.wormholeStableTicks,
	valReal,
	tuner_wormhole_stable_ticks,
	"Number of ticks wormholes will keep the same destination.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"defaultsFileName",
	"defaults",
	NULL,
	&options.defaultsFileName,
	valString,
	tuner_none,
	"The filename of the defaults file to read on startup.\n",
	OPT_COMMAND,
    },
    {
	"passwordFileName",
	"passwordFileName",
	NULL,
	&options.passwordFileName,
	valString,
	tuner_none,
	"The filename of the password file to read on startup.\n",
	OPT_COMMAND | OPT_DEFAULTS,
    },
    {
	"motdFileName",
	"motd",
	NULL,
	&options.motdFileName,
	valString,
	tuner_none,
	"The filename of the MOTD file to show to clients when they join.\n",
	OPT_COMMAND | OPT_DEFAULTS,
    },
    {
	"scoreTableFileName",
	"scoretable",
	NULL,
	&options.scoreTableFileName,
	valString,
	tuner_none,
	"The filename for the score table to be dumped to.\n"
	"This is a placeholder option which doesn't do anything.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"adminMessageFileName",
	"adminMessage",
	conf_logfile_string,
	&options.adminMessageFileName,
	valString,
	tuner_none,
	"The name of the file where player messages for the\n"
	"server administrator will be saved.  For the messages\n"
	"to be saved the file must already exist.  Players can\n"
	"send these messages by writing to god.",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"adminMessageFileSizeLimit",
	"adminMessageLimit",
	"20202", /* kps - ??? */
	&options.adminMessageFileSizeLimit,
	valInt,
	tuner_none,
	"The maximum size in bytes of the admin message file.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"rankFileName",
	"rankFileName",
	NULL,
	&options.rankFileName,
	valString,
	tuner_none,
	"The filename for the XML file to hold server ranking data.\n"
	"To reset the ranking, delete this file.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"rankWebpageFileName",
	"rankWebpage",
	NULL,
	&options.rankWebpageFileName,
	valString,
	tuner_none,
	"The filename for the webpage with the server ranking list.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"rankWebpageCSS",
	"rankCSS",
	NULL,
	&options.rankWebpageCSS,
	valString,
	tuner_none,
	"The URL of an optional style sheet for the ranking webpage.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"framesPerSecond",
	"FPS",
	"60",
	&options.framesPerSecond,
	valInt,
	Timing_setup,
	"The number of frames per second the server should strive for.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gameSpeed",
	"gameSpeed",
	"12.5",
	&options.gameSpeed,
	valReal,
	Timing_setup,
	"Rate at which game events happen. The gameSpeed specifies how\n"
	"many ticks of game time elapse each second. A value of 0 means\n"
	"that the value of gameSpeed should be equal to the value of FPS.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowSmartMissiles",
	"allowSmarts",
	"true",
	&options.allowSmartMissiles,
	valBool,
	tuner_dummy,
	"Should smart missiles be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowHeatSeekers",
	"allowHeats",
	"true",
	&options.allowHeatSeekers,
	valBool,
	tuner_dummy,
	"Should heatseekers be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowTorpedoes",
	"allowTorps",
	"true",
	&options.allowTorpedoes,
	valBool,
	tuner_dummy,
	"Should torpedoes be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowNukes",
	"nukes",
	"false",
	&options.allowNukes,
	valBool,
	tuner_modifiers,
	"Should nuclear weapons be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowClusters",
	"clusters",
	"false",
	&options.allowClusters,
	valBool,
	tuner_modifiers,
	"Should cluster weapons be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowModifiers",
	"modifiers",
	"false",
	&options.allowModifiers,
	valBool,
	tuner_modifiers,
	"Should the weapon modifiers be allowed?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowLaserModifiers",
	"lasermodifiers",
	"false",
	&options.allowLaserModifiers,
	valBool,
	tuner_modifiers,
	"Can lasers be modified to be a different weapon?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"allowShipShapes",
	"ShipShapes",
	"true",
	&options.allowShipShapes,
	valBool,
	tuner_dummy,
	"Are players allowed to define their own ship shape?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"playersOnRadar",
	"playersRadar",
	"true",
	&options.playersOnRadar,
	valBool,
	tuner_dummy,
	"Are players visible on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"missilesOnRadar",
	"missilesRadar",
	"true",
	&options.missilesOnRadar,
	valBool,
	tuner_dummy,
	"Are missiles visible on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minesOnRadar",
	"minesRadar",
	"false",
	&options.minesOnRadar,
	valBool,
	tuner_dummy,
	"Are mines visible on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"nukesOnRadar",
	"nukesRadar",
	"true",
	&options.nukesOnRadar,
	valBool,
	tuner_dummy,
	"Are nukes visible or highlighted on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"treasuresOnRadar",
	"treasuresRadar",
	"false",
	&options.treasuresOnRadar,
	valBool,
	tuner_dummy,
	"Are treasure balls visible or highlighted on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidsOnRadar",
	"asteroidsRadar",
	"false",
	&options.asteroidsOnRadar,
	valBool,
	tuner_dummy,
	"Are asteroids visible on the radar.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"distinguishMissiles",
	"distinguishMissiles",
	"true",
	&options.distinguishMissiles,
	valBool,
	tuner_dummy,
	"Are different types of missiles distinguished (by length).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxMissilesPerPack",
	"maxMissilesPerPack",
	"4",
	&options.maxMissilesPerPack,
	valInt,
	Tune_item_packs,
	"The number of missiles gotten by picking up one missile item.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxMinesPerPack",
	"maxMinesPerPack",
	"2",
	&options.maxMinesPerPack,
	valInt,
	Tune_item_packs,
	"The number of mines gotten by picking up one mine item.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"identifyMines",
	"identifyMines",
	"true",
	&options.identifyMines,
	valBool,
	tuner_dummy,
	"Are mine owner's names displayed.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shieldedItemPickup",
	"shieldItem",
	"false",
	&options.shieldedItemPickup,
	valBool,
	tuner_dummy,
	"Can items be picked up while shields are up?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shieldedMining",
	"shieldMine",
	"false",
	&options.shieldedMining,
	valBool,
	tuner_dummy,
	"Can mines be thrown and placed while shields are up?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"laserIsStunGun",
	"stunGun",
	"false",
	&options.laserIsStunGun,
	valBool,
	tuner_dummy,
	"Is the laser weapon a stun gun weapon?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"nukeMinSmarts",
	"nukeMinSmarts",
	"7",
	&options.nukeMinSmarts,
	valInt,
	tuner_dummy,
	"The minimum number of missiles needed to fire one nuclear missile.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"nukeMinMines",
	"nukeMinMines",
	"4",
	&options.nukeMinMines,
	valInt,
	tuner_dummy,
	"The minimum number of mines needed to make a nuclear mine.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"nukeClusterDamage",
	"nukeClusterDamage",
	"1.0",
	&options.nukeClusterDamage,
	valReal,
	tuner_dummy,
	"How much each cluster debris does damage wise from a nuke mine.\n"
	"This helps to reduce the number of particles caused by nuclear mine\n"
	"explosions, which improves server response time for such explosions.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"nukeDebrisLife",
	"nukeDebrisLife",
	"120.0",
	&options.nukeDebrisLife,
	valReal,
	tuner_dummy,
	"Life of nuke debris, in ticks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mineFuseTicks",
	"mineFuseTicks",
	"0.0",
	&options.mineFuseTicks,
	valReal,
	tuner_dummy,
	"Number of ticks after which owned mines become deadly (0=never).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mineLife",
	"mineLife",
	"7200.0",
	&options.mineLife,
	valReal,
	tuner_dummy,
	"Life of mines in ticks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"minMineSpeed",
	"minMineSpeed",
	"0.0",
	&options.minMineSpeed,
	valReal,
	tuner_dummy,
	"Minimum speed of mines.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"missileLife",
	"missileLife",
	"2400.0",
	&options.missileLife,
	valReal,
	tuner_dummy,
	"Life of missiles in ticks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"baseMineRange",
	"baseMineRange",
	"0.0",
	&options.baseMineRange,
	valReal,
	tuner_dummy,
	"Minimum distance from base mines may be used (unit is blocks).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mineShotDetonateDistance",
	"mineShotDetonateDistance",
	"0.0",
	&options.mineShotDetonateDistance,
	valReal,
	tuner_dummy,
	"How close must a shot be to detonate a mine?\n"
	"Set this to 0 to turn it off.",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shotKillScoreMult",
	"shotKillScoreMult",
	"1.0",
	&options.shotKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for shot kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"torpedoKillScoreMult",
	"torpedoKillScoreMult",
	"1.0",
	&options.torpedoKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for torpedo kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"smartKillScoreMult",
	"smartKillScoreMult",
	"1.0",
	&options.smartKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for smart missile kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"heatKillScoreMult",
	"heatKillScoreMult",
	"1.0",
	&options.heatKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for heatseeker kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"clusterKillScoreMult",
	"clusterKillScoreMult",
	"1.0",
	&options.clusterKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for cluster debris kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"laserKillScoreMult",
	"laserKillScoreMult",
	"1.0",
	&options.laserKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for laser kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"tankKillScoreMult",
	"tankKillScoreMult",
	"0.44",
	&options.tankKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for tank kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"runoverKillScoreMult",
	"runoverKillScoreMult",
	"0.33",
	&options.runoverKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for player runovers.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballKillScoreMult",
	"ballKillScoreMult",
	"1.0",
	&options.ballKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for ball kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"explosionKillScoreMult",
	"explosionKillScoreMult",
	"0.33",
	&options.explosionKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for explosion kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"shoveKillScoreMult",
	"shoveKillScoreMult",
	"0.5",
	&options.shoveKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for shove kills.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"crashScoreMult",
	"crashScoreMult",
	"0.33",
	&options.crashScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for player crashes.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"mineScoreMult",
	"mineScoreMult",
	"0.17",
	&options.mineScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for mine hits.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"selfKillScoreMult",
	"selfKillScoreMult",
	"0.5",
	&options.selfKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for killing yourself.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"selfDestructScoreMult",
	"selfDestructScoreMult",
	"0.0",
	&options.selfDestructScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for self-destructing.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"unownedKillScoreMult",
	"unownedKillScoreMult",
	"0.5",
	&options.unownedKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for being killed by asteroids\n"
	"or other objects without an owner.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonKillScoreMult",
	"cannonKillScoreMult",
	"0.25",
	&options.cannonKillScoreMult,
	valReal,
	tuner_dummy,
	"Multiplication factor to scale score for being killed by cannons.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"movingItemProb",
	"movingItemProb",
	"0.2",
	&options.movingItemProb,
	valReal,
	Set_misc_item_limits,
	"Probability for an item to appear as moving.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"randomItemProb",
	"randomItemProb",
	"0.0",
	&options.randomItemProb,
	valReal,
	Set_misc_item_limits,
	"Probability for an item to appear random.\n"
	"Random items change their appearance every frame, so players\n"
	"cannot tell what item they have until they get it.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"dropItemOnKillProb",
	"dropItemOnKillProb",
	"0.5",
	&options.dropItemOnKillProb,
	valReal,
	Set_misc_item_limits,
	"Probability for dropping an item (each item) when you are killed.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"detonateItemOnKillProb",
	"detonateItemOnKillProb",
	"0.5",
	&options.detonateItemOnKillProb,
	valReal,
	Set_misc_item_limits,
	"Probability for undropped items to detonate when you are killed.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"destroyItemInCollisionProb",
	"destroyItemInCollisionProb",
	"0.0",
	&options.destroyItemInCollisionProb,
	valReal,
	Set_misc_item_limits,
	"Probability for items (some items) to be destroyed in a collision.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidItemProb",
	"asteroidItemProb",
	"0.0",
	&options.asteroidItemProb,
	valReal,
	Set_misc_item_limits,
	"Probability for an asteroid to release items when it breaks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidMaxItems",
	"asteroidMaxItems",
	"0",
	&options.asteroidMaxItems,
	valInt,
	Set_misc_item_limits,
	"The maximum number of items a broken asteroid can release.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemProbMult",
	"itemProbFact",
	"1.0",
	&options.itemProbMult,
	valReal,
	Tune_item_probs,
	"Item Probability Factor scales all item probabilities.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"cannonItemProbMult",
	"cannonItemProbMult",
	"1.0",
	&options.cannonItemProbMult,
	valReal,
	tuner_dummy,
	"Average number of items a cannon gets per minute.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxItemDensity",
	"maxItemDensity",
	"0.00012",
	&options.maxItemDensity,
	valReal,
	Tune_item_probs,
	"Maximum density for items (max items per block).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidProb",
	"asteroidProb",
	"5e-7",
	&World.asteroids.prob,
	valReal,
	Tune_asteroid_prob,
	"Probability for an asteroid to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxAsteroidDensity",
	"maxAsteroidDensity",
	"0.0",
	&options.maxAsteroidDensity,
	valReal,
	Tune_asteroid_prob,
	"Maximum density [0.0-1.0] for asteroids (max asteroids per block.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemConcentratorRadius",
	"itemConcentratorRange",
	"10.0",
	&options.itemConcentratorRadius,
	valReal,
	Set_misc_item_limits,
	"Range within which an item concentrator can create an item.\n"
	"Sensible values are in the range 1.0 to 20.0 (unit is 35 pixels).\n"
	"If there are no item concentrators, items might popup anywhere.\n"
	"Some clients draw item concentrators as three rotating triangles.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemConcentratorProb",
	"itemConcentratorProb",
	"1.0",
	&options.itemConcentratorProb,
	valReal,
	Set_misc_item_limits,
	"The chance for an item to appear near an item concentrator.\n"
	"If this is less than 1.0 or there are no item concentrators,\n"
	"items may also popup where there is no concentrator nearby.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidConcentratorRadius",
	"asteroidConcentratorRange",
	"10.0",
	&options.asteroidConcentratorRadius,
	valReal,
	Tune_asteroid_prob,
	"Range within which an asteroid concentrator can create an asteroid.\n"
	"Sensible values are in the range 1.0 to 20.0 (unit is 35 pixels).\n"
	"If there are no such concentrators, asteroids can popup anywhere.\n"
	"Some clients draw these concentrators as three rotating squares.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"asteroidConcentratorProb",
	"asteroidConcentratorProb",
	"1.0",
	&options.asteroidConcentratorProb,
	valReal,
	Tune_asteroid_prob,
	"The chance for an asteroid to appear near an asteroid concentrator.\n"
	"If this is less than 1.0 or there are no asteroid concentrators,\n"
	"asteroids may also appear where there is no concentrator nearby.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"rogueHeatProb",
	"rogueHeatProb",
	"1.0",
	&options.rogueHeatProb,
	valReal,
	tuner_dummy,
	"Probability that unclaimed missile packs will go rogue.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"rogueMineProb",
	"rogueMineProb",
	"1.0",
	&options.rogueMineProb,
	valReal,
	tuner_dummy,
	"Probability that unclaimed mine items will activate.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemEnergyPackProb",
	"itemEnergyPackProb",
	"1e-9",
	&World.items[ITEM_FUEL].prob,
	valReal,
	Tune_item_probs,
	"Probability for an energy pack to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemTankProb",
	"itemTankProb",
	"1e-9",
	&World.items[ITEM_TANK].prob,
	valReal,
	Tune_item_probs,
	"Probability for an extra tank to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemECMProb",
	"itemECMProb",
	"1e-9",
	&World.items[ITEM_ECM].prob,
	valReal,
	Tune_item_probs,
	"Probability for an ECM item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemArmorProb",
	"itemArmorProb",
	"1e-9",
	&World.items[ITEM_ARMOR].prob,
	valReal,
	Tune_item_probs,
	"Probability for an armor item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemMineProb",
	"itemMineProb",
	"1e-9",
	&World.items[ITEM_MINE].prob,
	valReal,
	Tune_item_probs,
	"Probability for a mine item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemMissileProb",
	"itemMissileProb",
	"1e-9",
	&World.items[ITEM_MISSILE].prob,
	valReal,
	Tune_item_probs,
	"Probability for a missile item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemCloakProb",
	"itemCloakProb",
	"1e-9",
	&World.items[ITEM_CLOAK].prob,
	valReal,
	Tune_item_probs,
	"Probability for a cloak item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemSensorProb",
	"itemSensorProb",
	"1e-9",
	&World.items[ITEM_SENSOR].prob,
	valReal,
	Tune_item_probs,
	"Probability for a sensor item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemWideangleProb",
	"itemWideangleProb",
	"1e-9",
	&World.items[ITEM_WIDEANGLE].prob,
	valReal,
	Tune_item_probs,
	"Probability for a wideangle item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemRearshotProb",
	"itemRearshotProb",
	"1e-9",
	&World.items[ITEM_REARSHOT].prob,
	valReal,
	Tune_item_probs,
	"Probability for a rearshot item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemAfterburnerProb",
	"itemAfterburnerProb",
	"1e-9",
	&World.items[ITEM_AFTERBURNER].prob,
	valReal,
	Tune_item_probs,
	"Probability for an afterburner item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemTransporterProb",
	"itemTransporterProb",
	"1e-9",
	&World.items[ITEM_TRANSPORTER].prob,
	valReal,
	Tune_item_probs,
	"Probability for a transporter item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemMirrorProb",
	"itemMirrorProb",
	"1e-9",
	&World.items[ITEM_MIRROR].prob,
	valReal,
	Tune_item_probs,
	"Probability for a mirror item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemDeflectorProb",
	"itemDeflectorProb",
	"1e-9",
	&World.items[ITEM_DEFLECTOR].prob,
	valReal,
	Tune_item_probs,
	"Probability for a deflector item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemHyperJumpProb",
	"itemHyperJumpProb",
	"1e-9",
	&World.items[ITEM_HYPERJUMP].prob,
	valReal,
	Tune_item_probs,
	"Probability for a hyperjump item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemPhasingProb",
	"itemPhasingProb",
	"1e-9",
	&World.items[ITEM_PHASING].prob,
	valReal,
	Tune_item_probs,
	"Probability for a phasing item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemLaserProb",
	"itemLaserProb",
	"1e-9",
	&World.items[ITEM_LASER].prob,
	valReal,
	Tune_item_probs,
	"Probability for a Laser item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemEmergencyThrustProb",
	"itemEmergencyThrustProb",
	"1e-9",
	&World.items[ITEM_EMERGENCY_THRUST].prob,
	valReal,
	Tune_item_probs,
	"Probability for an Emergency Thrust item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemTractorBeamProb",
	"itemTractorBeamProb",
	"1e-9",
	&World.items[ITEM_TRACTOR_BEAM].prob,
	valReal,
	Tune_item_probs,
	"Probability for a Tractor Beam item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemAutopilotProb",
	"itemAutopilotProb",
	"1e-9",
	&World.items[ITEM_AUTOPILOT].prob,
	valReal,
	Tune_item_probs,
	"Probability for an Autopilot item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"itemEmergencyShieldProb",
	"itemEmergencyShieldProb",
	"1e-9",
	&World.items[ITEM_EMERGENCY_SHIELD].prob,
	valReal,
	Tune_item_probs,
	"Probability for an Emergency Shield item to appear.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialFuel",
	"initialFuel",
	"300", /* was 1000 */
	&World.items[ITEM_FUEL].initial,
	valInt,
	Set_initial_resources,
	"How much fuel players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialTanks",
	"initialTanks",
	"0",
	&World.items[ITEM_TANK].initial,
	valInt,
	Set_initial_resources,
	"How many tanks players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialECMs",
	"initialECMs",
	"0",
	&World.items[ITEM_ECM].initial,
	valInt,
	Set_initial_resources,
	"How many ECMs players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialArmor",
	"initialArmors",
	"0",
	&World.items[ITEM_ARMOR].initial,
	valInt,
	Set_initial_resources,
	"How much armor players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialMines",
	"initialMines",
	"0",
	&World.items[ITEM_MINE].initial,
	valInt,
	Set_initial_resources,
	"How many mines players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialMissiles",
	"initialMissiles",
	"0",
	&World.items[ITEM_MISSILE].initial,
	valInt,
	Set_initial_resources,
	"How many missiles players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialCloaks",
	"initialCloaks",
	"0",
	&World.items[ITEM_CLOAK].initial,
	valInt,
	Set_initial_resources,
	"How many cloaks players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialSensors",
	"initialSensors",
	"0",
	&World.items[ITEM_SENSOR].initial,
	valInt,
	Set_initial_resources,
	"How many sensors players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialWideangles",
	"initialWideangles",
	"0",
	&World.items[ITEM_WIDEANGLE].initial,
	valInt,
	Set_initial_resources,
	"How many wideangles players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialRearshots",
	"initialRearshots",
	"0",
	&World.items[ITEM_REARSHOT].initial,
	valInt,
	Set_initial_resources,
	"How many rearshots players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialAfterburners",
	"initialAfterburners",
	"0",
	&World.items[ITEM_AFTERBURNER].initial,
	valInt,
	Set_initial_resources,
	"How many afterburners players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialTransporters",
	"initialTransporters",
	"0",
	&World.items[ITEM_TRANSPORTER].initial,
	valInt,
	Set_initial_resources,
	"How many transporters players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialMirrors",
	"initialMirrors",
	"0",
	&World.items[ITEM_MIRROR].initial,
	valInt,
	Set_initial_resources,
	"How many mirrors players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialDeflectors",
	"initialDeflectors",
	"0",
	&World.items[ITEM_DEFLECTOR].initial,
	valInt,
	Set_initial_resources,
	"How many deflectors players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialHyperJumps",
	"initialHyperJumps",
	"0",
	&World.items[ITEM_HYPERJUMP].initial,
	valInt,
	Set_initial_resources,
	"How many hyperjumps players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialPhasings",
	"initialPhasings",
	"0",
	&World.items[ITEM_PHASING].initial,
	valInt,
	Set_initial_resources,
	"How many phasing devices players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialLasers",
	"initialLasers",
	"0",
	&World.items[ITEM_LASER].initial,
	valInt,
	Set_initial_resources,
	"How many lasers players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialEmergencyThrusts",
	"initialEmergencyThrusts",
	"0",
	&World.items[ITEM_EMERGENCY_THRUST].initial,
	valInt,
	Set_initial_resources,
	"How many emergency thrusts players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialTractorBeams",
	"initialTractorBeams",
	"0",
	&World.items[ITEM_TRACTOR_BEAM].initial,
	valInt,
	Set_initial_resources,
	"How many tractor/pressor beams players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialAutopilots",
	"initialAutopilots",
	"0",
	&World.items[ITEM_AUTOPILOT].initial,
	valInt,
	Set_initial_resources,
	"How many autopilots players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"initialEmergencyShields",
	"initialEmergencyShields",
	"0",
	&World.items[ITEM_EMERGENCY_SHIELD].initial,
	valInt,
	Set_initial_resources,
	"How many emergency shields players start with.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxFuel",
	"maxFuel",
	"10000",
	&World.items[ITEM_FUEL].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the amount of fuel per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxTanks",
	"maxTanks",
	"8",
	&World.items[ITEM_TANK].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of tanks per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxECMs",
	"maxECMs",
	"10",
	&World.items[ITEM_ECM].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of ECMs per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxMines",
	"maxMines",
	"10",
	&World.items[ITEM_MINE].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of mines per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxMissiles",
	"maxMissiles",
	"10",
	&World.items[ITEM_MISSILE].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of missiles per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxCloaks",
	"maxCloaks",
	"10",
	&World.items[ITEM_CLOAK].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of cloaks per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxSensors",
	"maxSensors",
	"10",
	&World.items[ITEM_SENSOR].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of sensors per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxWideangles",
	"maxWideangles",
	"10",
	&World.items[ITEM_WIDEANGLE].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of wides per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxRearshots",
	"maxRearshots",
	"10",
	&World.items[ITEM_REARSHOT].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of rearshots per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxAfterburners",
	"maxAfterburners",
	"10",
	&World.items[ITEM_AFTERBURNER].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of afterburners per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxTransporters",
	"maxTransporters",
	"10",
	&World.items[ITEM_TRANSPORTER].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of transporters per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxArmor",
	"maxArmors",
	"10",
	&World.items[ITEM_ARMOR].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the amount of armor per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxMirrors",
	"maxMirrors",
	"10",
	&World.items[ITEM_MIRROR].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of mirrors per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxDeflectors",
	"maxDeflectors",
	"10",
	&World.items[ITEM_DEFLECTOR].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of deflectors per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxPhasings",
	"maxPhasings",
	"10",
	&World.items[ITEM_PHASING].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of phasing devices per players.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxHyperJumps",
	"maxHyperJumps",
	"10",
	&World.items[ITEM_HYPERJUMP].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of hyperjumps per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxEmergencyThrusts",
	"maxEmergencyThrusts",
	"10",
	&World.items[ITEM_EMERGENCY_THRUST].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of emergency thrusts per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxLasers",
	"maxLasers",
	"5",
	&World.items[ITEM_LASER].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of lasers per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxTractorBeams",
	"maxTractorBeams",
	"4",
	&World.items[ITEM_TRACTOR_BEAM].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of tractorbeams per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxAutopilots",
	"maxAutopilots",
	"10",
	&World.items[ITEM_AUTOPILOT].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of autopilots per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxEmergencyShields",
	"maxEmergencyShields",
	"10",
	&World.items[ITEM_EMERGENCY_SHIELD].limit,
	valInt,
	Set_initial_resources,
	"Upper limit on the number of emergency shields per player.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"gameDuration",
	"time",
	"0.0",
	&options.gameDuration,
	valReal,
	tuner_gameduration,
	"The duration of the game in minutes (aka. pizza mode).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"baselessPausing",
	"baselessPausing",
	"false",
	&options.baselessPausing,
	valBool,
	Check_baseless,
	"Should paused players keep their bases?\n"
	"Can only be used on teamplay maps for now.\n",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"survivalScore",
	"survivalScore",
	"0.0",
	&options.survivalScore,
	valReal,
	tuner_dummy,
	"Multiplicator for quadratic score increase over time \n"
	"survived with lowered shield",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pauseTax",
	"pauseTax",
	"0.0",
	&options.pauseTax,
	valReal,
	tuner_dummy,
	"How many points to subtract from pausing players each second.\n",
 	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"friction",
	"friction",
	"0.0",
	&options.frictionSetting,
	valReal,
	Timing_setup,
	"Fraction of velocity ship loses each frame.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"blockFriction",
	"blockFriction",
	"0.0",
	&options.blockFriction,
	valReal,
	Timing_setup,
	"Fraction of velocity ship loses each frame when it is in friction \n"
	"blocks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"blockFrictionVisible",
	"blockFrictionVisible",
	"true",
	&options.blockFrictionVisible,
	valBool,
	tuner_none,
	"Are friction blocks visible?\n"
	"If true, friction blocks show up as decor; if false, they don't \n"
	"show up at all.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"coriolis",
	"coriolis",
	"0.0",
	&options.coriolis,
	valReal,
	Timing_setup,
	"The clockwise angle (in degrees) a ship's velocity turns each \n"
	"time unit.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"checkpointRadius",
	"checkpointRadius",
	"6.0",
	&options.checkpointRadius,
	valReal,
	tuner_dummy,
	"How close you have to be to a checkpoint to register - in blocks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"raceLaps",
	"raceLaps",
	"3",
	&options.raceLaps,
	valInt,
	tuner_racelaps,
	"How many laps per race.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"loseItemDestroys",
	"loseItemDestroys",
	"false",
	&options.loseItemDestroys,
	valBool,
	tuner_dummy,
	"Destroy item that player drops. Otherwise drop it.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"useDebris",
	"useDebris",
	"true",
	&options.useDebris,
	valBool,
	tuner_dummy,
	"Are debris particles created where appropriate?\n"
	"Value affect ship exhaust sparks and cluster debris.\n"
	"To disallow cluster weapons but not sparks, set allowClusters off.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"useWreckage",
	"useWreckage",
	"true",
	&options.useWreckage,
	valBool,
	tuner_dummy,
	"Do destroyed ships leave wreckage?\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxOffensiveItems",
	"maxOffensiveItems",
	"100",
	&options.maxOffensiveItems,
	valInt,
	tuner_dummy,
	"How many offensive items a player can carry.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxDefensiveItems",
	"maxDefensiveItems",
	"100",
	&options.maxDefensiveItems,
	valInt,
	tuner_dummy,
	"How many defensive items a player can carry.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxRoundTime",
	"maxRoundTime",
	"0",
	&options.maxRoundTime,
	valInt,
	tuner_dummy,
	"The maximum duration of each round, in seconds.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"roundsToPlay",
	"numRounds",
	"0",
	&options.roundsToPlay,
	valInt,
	tuner_dummy,
	"The number of rounds to play.  Unlimited if 0.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxVisibleObject",
	"maxVisibleObjects",
	"1000",
	&options.maxVisibleObject,
	valInt,
	tuner_dummy,
	"What is the maximum number of objects a player can see.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pLockServer",
	"pLockServer",
#ifdef PLOCKSERVER
	"true",
#else
	"false",
#endif
	&options.pLockServer,
	valBool,
	tuner_plock,
	"Whether the server is prevented from being swapped out of memory.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"sound",
	"sound",
	"true",
	&options.sound,
	valBool,
	tuner_dummy,
	"Does the server send sound events to players that request sound.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
#ifndef SELECT_SCHED
    {
	"timerResolution",
	"timerResolution",
	"0",
	&options.timerResolution,
	valInt,
	Timing_setup,
	"If set to nonzero xpilots will requests signals from the OS at\n"
	"1/timerResolution second intervals.  The server will then compute\n"
	"a new frame FPS times out of every timerResolution signals.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
#endif
    {
	"password",
	"password",
	NULL,
	&options.password,
	valString,
	tuner_dummy,
	"The password needed to obtain operator privileges.\n"
	"If specified on the command line, on many systems other\n"
	"users will be able to see the password.  Therefore, using\n"
	"the password file instead is recommended.",
	OPT_COMMAND | OPT_DEFAULTS | OPT_PASSWORD
    },
    {
	"clientPortStart",
	"clientPortStart",
	"0",
	&options.clientPortStart,
	valInt,
	tuner_dummy,
	"Restrict selected gameplay transport ports to clientPortStart - "
	"clientPortEnd (for firewalls)\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"clientPortEnd",
	"clientPortEnd",
	"0",
	&options.clientPortEnd,
	valInt,
	tuner_dummy,
	"Restrict selected gameplay transport ports to clientPortStart - "
	"clientPortEnd (for firewalls)\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"maxPauseTime",
	"maxPauseTime",
	"14400",	/* can pause 4 hours by default */
	&options.maxPauseTime,
	valInt,
	tuner_dummy,
	"The maximum time a player can stay paused for, in seconds.\n"
	"After being paused this long, the player will be kicked off.\n"
	"Setting this option to 0 disables the feature.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxIdleTime",
	"maxIdleTime",
	"60",		/* can idle 1 minute by default */
	&options.maxIdleTime,
	valInt,
	tuner_dummy,
	"The maximum time a player can stay idle, in seconds.\n"
	"After having idled this long, the player will be paused.\n"
	"Setting this option to 0 disables the feature.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"maxClientsPerIP",
	"maxPerIP",
	"4",		/* if more try to join they get "game locked" */
	&options.maxClientsPerIP,
	valInt,
	tuner_dummy,
	"Maximum number of clients per IP address allowed to connect.\n"
	"This prevents unfriendly players from occupying all the bases, \n"
	"effectively \"kicking\" paused players and denying other players\n"
	"to join.\n"
	"Setting this to 0 means any number of clients from the same IP\n"
	"address can join.\n",
	OPT_COMMAND | OPT_DEFAULTS | OPT_VISIBLE
    },
    {
	"playerLimit",
	"playerLimit",
	"0",
	&options.playerLimit,
	valInt,
	Check_playerlimit,
	"Allow playerLimit players to enter at once.\n"
	"If set to 0, allow 10 more players than there are bases.\n"
	"(If baselessPausing is off, more than bases cannot enter.)\n"
	"During game, cannot be set higher than the starting value.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"recordMode",
	"recordMode",
	"0",
	&options.recordMode,
	valInt,
	Init_recording,
	"If this is set to 1 when the server starts, the game is saved\n"
	"in the file specified by recordFileName. If set to 2 at startup,\n"
	"the server replays the recorded game. Joining players are\n"
	"spectators who can watch the recorded game from anyone's\n"
	"viewpoint. Playback must use the same gameTransport value as the\n"
	"recording. Can be set to 0 in the middle of a game to stop "
	"recording.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"recordFileName",
	"recordFile",
	NULL,
	&options.recordFileName,
	valString,
	tuner_none,
	"Name of the file where server recordings are saved.\n",
	OPT_COMMAND | OPT_DEFAULTS
    },
    {
	"recordFlushInterval",
	"recordWait",
	"0",
	&options.recordFlushInterval,
	valInt,
	tuner_dummy,
	"If set to a nonzero value x, the server will flush all recording\n"
	"data in memory to the record file at least once every x seconds.\n"
	"This is useful if you want to replay the game on another server\n"
	"while it is still being played. There is a small overhead\n"
	"(some dozens of bytes extra recording file size) for each flush.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"constantScoring",
	"constantScoring",
	"false",
	&options.constantScoring,
	valBool,
	tuner_dummy,
	"Whether the scores given from various things are fixed.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"zeroSumScoring",
	"zeroSum",
	"false",
	&options.zeroSumScoring,
	valBool,
	tuner_dummy,
	"Use Zero sum scoring?\n",
	OPT_COMMAND | OPT_VISIBLE
    },
    {
	"elimination",
	"elimination",
	"false",
	&options.eliminationRace,
	valBool,
	tuner_dummy,
	"Race mode where the last player drops out each lap.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"dataURL",
	"dataURL",
	"",
	&options.dataURL,
	valString,
	tuner_dummy,
	"URL where the client can get extra data for this map.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"polygonMode",
	"polygonMode",
	"false",
	&options.polygonMode,
	valBool,
	tuner_dummy,
	"Force use of polygon protocol when communicating with clients?\n"
	"(useful for debugging if you want to see the polygons created\n"
	"in the blocks to polygons conversion function).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"fastAim",
	"fastAim",
	"false",
	&options.fastAim,
	valBool,
	tuner_dummy,
	"When calculating a frame, turn the ship before firing.\n"
	"This means you can change aim one frame faster.\n"
	"Added this option to see how much difference changing the order\n"
	"would make.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"infinityControls",
	"infinityControls",
	"false",
	&options.infinityControls,
	valBool,
	tuner_dummy,
	"Enable improved precision steering and aiming of main gun.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"legacyMode",
	"legacyMode",
	"false",
	&options.legacyMode,
	valBool,
	tuner_dummy,
	"Try to emulate classic xpilot behavior.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"constantSpeed",
	"oldThrust",
	"0.88",
	&options.constantSpeed,
	valReal,
	tuner_dummy,
	"Make ship move forward at a constant speed when thrust key is held\n"
	"down, in addition to the normal acceleration. The constant speed\n"
	"is proportional to the product of the acceleration of the ship\n"
	"(varying with ship mass and afterburners) and the value of this\n"
	"option. Note that this option is quite unphysical and can using it\n"
	"can cause weird effects (with bounces for example).\n"
	"Low values close to 0.5 (maybe in the range 0.3 to 1) for this\n"
	"option can be used if you want to increase ship agility without\n"
	"increasing speeds otherwise. This can improve gameplay for example\n"
	"on the Blood's Music map. Higher values make the ship behaviour\n"
	"visibly weird.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"turnPushPersistence",
	"pushPersist",
	"0.0",
	&options.turnPushPersistence,
	valReal,
	tuner_dummy,
	"How much of the turnpush to remain as player velocity. (0.0-1.0)\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"turnGrip",
	"turnPushGrip",
	"0.0",
	&options.turnGrip,
	valReal,
	tuner_dummy,
	"How much of of the turnPush should pull the ship sideways by\n"
	"gripping to the friction of the wall?. (0.0-1.0)\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"thrustWidth",
	"sprayWidth",
	"0.25", /* was "1.0" */
	&options.thrustWidth,
	valReal,
	tuner_dummy,
	"Width of thrust spark spray 0.0-1.0 where 1.0 means 180 degrees.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"thrustMass",
	"sparkWeight",
	"0.0435", /* was "0.7" */
	&options.thrustMass,
	valReal,
	tuner_dummy,
	"Weight of thrust sparks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"sparkSpeed",
	"sparkVel",
	"30.0", /* was "1.0" */
	&options.sparkSpeed,
	valReal,
	tuner_dummy,
	"Multiplier affecting avg. speed (relative ship) of thrust sparks.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ballStyles",
	"ballStyles",
	"true",
	&options.ballStyles,
	valBool,
	tuner_dummy,
	"Send ball styles to clients.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"ignoreMaxFPS",
	"ignoreMaxFPS",
	"false",
	&options.ignoreMaxFPS,
	valBool,
	tuner_dummy,
	"Ignore client maxFPS requests and always send all frames.\n"
	"This is a hack for demonstration purposes to allow changing\n"
	"the server FPS when there are old clients with broken maxFPS\n"
	"handling. Those clients could be better dealt with separately.\n"
	"This option will be removed in the future (hopefully).\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"pausedFramesPerSecond",
	"pausedFPS",
	"0",
	&options.pausedFPS,
	valInt,
	tuner_dummy,
	"Maximum FPS shown to paused players. 0 means full framerate.\n"
	"Can be used to reduce server bandwidth consumption.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"waitingFramesPerSecond",
	"waitingFPS",
	"0",
	&options.waitingFPS,
	valInt,
	tuner_dummy,
	"Maximum FPS shown to players in waiting state.\n"
	"0 means full framerate. Can be used to limit bandwidth used.\n"
	"Waiting players are those that have just joined a game and have to\n"
	"wait until next round starts. Note that in clients, a W is shown\n"
	"next to waiting players' names in the score list.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    {
	"deadFramesPerSecond",
	"deadFPS",
	"0",
	&options.deadFPS,
	valInt,
	tuner_dummy,
	"Maximum FPS shown to players in dead state.\n"
	"0 means full framerate. Can be used to limit bandwidth used.\n"
	"This option should only be used if pausedFPS and waitingFPS\n"
	"options don't limit bandwidth usage enough.\n"
	"Dead players are those that have played this round but have run\n"
	"out of lives. Note that in clients, a D is shown next to dead\n"
	"players' names in the score list.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
    /* teamcup related options */
    {
	"teamcup",
	"teamcup",
	"false",
	&options.teamcup,
	valBool,
	tuner_none,
	"Is this a teamcup match?.\n",
	OPT_ORIGIN_ANY 
    },
    {
	"teamcupName",
	"teamcupName",
	"",
	&options.teamcupName,
 	valString,
	tuner_none,
	"The name of the teamcup (used only if teamcup is true).\n",
	OPT_ORIGIN_ANY
    },
    {
	"teamcupMailAddress",
	"teamcupMailAddress",
	"",
	&options.teamcupMailAddress,
	valString,
	tuner_none,
	"The mail address where players should send match results.\n",
	OPT_ORIGIN_ANY
    },
    {
	"teamcupScoreFileNamePrefix",
	"teamcupScoreFileNamePrefix",
	"",
	&options.teamcupScoreFileNamePrefix,
	valString,
	tuner_none,
	"First part of file name for teamcup score files.\n"
	"The whole filename will be this followed by the match number.\n",
	OPT_ORIGIN_ANY
     },
     {
 	"teamcupMatchNumber",
	"match",
	"0",
	&options.teamcupMatchNumber,
	valInt,
	tuner_dummy,
	"The number of the teamcup match.\n",
	OPT_COMMAND | OPT_VISIBLE
    },
    {
	"mainLoopTime",
	"mainLoopTime",
	"0",
	&options.mainLoopTime,
	valReal,
	tuner_none,
	"Duration of last Main_loop() function call (in milliseconds).\n"
	"This option is read only.\n",
	OPT_COMMAND | OPT_VISIBLE
    },
    {
	"cellGetObjectsThreshold",
	"cellThreshold",
	"500",
	&options.cellGetObjectsThreshold,
	valInt,
	tuner_dummy,
	"Use Cell_get_objects if there is this many objects or more.\n",
	OPT_ORIGIN_ANY | OPT_VISIBLE
    },
};


static bool options_inited = false;


option_desc* Get_option_descs(int *count_ptr)
{
    if (!options_inited)
	dumpcore("options not initialized");

    *count_ptr = NELEM(opts);
    return &opts[0];
}


static void Init_default_options(void)
{
    option_desc *desc;

    if ((desc = Find_option_by_name("adminMessageFileName")) == NULL)
	dumpcore("Could not find admin message file option");
    desc->defaultValue = Conf_logfile();

    if ((desc = Find_option_by_name("mapFileName")) == NULL)
	dumpcore("Could not find map file option");
    desc->defaultValue = Conf_default_map();

    if ((desc = Find_option_by_name("motdFileName")) == NULL)
	dumpcore("Could not find motd file option");
    desc->defaultValue = Conf_servermotdfile();

    if ((desc = Find_option_by_name("robotFile")) == NULL)
	dumpcore("Could not find robot file option");
    desc->defaultValue = Conf_robotfile();

    if ((desc = Find_option_by_name("defaultsFileName")) == NULL)
	dumpcore("Could not find defaults file option");
    desc->defaultValue = Conf_defaults_file_name();

    if ((desc = Find_option_by_name("passwordFileName")) == NULL)
	dumpcore("Could not find password file option");
    desc->defaultValue = Conf_password_file_name();
}


bool Init_options(void)
{
    int i, option_count = NELEM(opts);

    if (options_inited)
	dumpcore("Can't init options twice.");

    Init_default_options();

    for (i = 0; i < option_count; i++) {
	if (Option_add_desc(&opts[i]) == false)
	    return false;
    }

    options_inited = true;

    return true;
}


void Free_options(void)
{
    int i, option_count = NELEM(opts);

    if (options_inited) {
	options_inited = false;
	for (i = 0; i < option_count; i++) {
	    if (opts[i].type == valString) {
		char **str_ptr = (char **)opts[i].variable;
		char *str = *str_ptr;

		if (str != NULL && str != opts[i].defaultValue) {
		    free(str);
		    *str_ptr = NULL;
		}
	    }
	}
    }
}


option_desc *Find_option_by_name(const char* name)
{
    int j, option_count = NELEM(opts);

    for (j = 0; j < option_count; j++) {
	if (!strcasecmp(opts[j].commandLineOption, name)
	    || !strcasecmp(opts[j].name, name))
	    return(&opts[j]);
    }
    return NULL;
}


void Check_playerlimit(void)
{
    if (options.playerLimit == 0)
	options.playerLimit = Num_bases() + 10;

    if (options.playerLimit_orig == 0)
	options.playerLimit_orig = MAX(options.playerLimit,
				       Num_bases() + 10);

    if (options.playerLimit > options.playerLimit_orig)
	options.playerLimit = options.playerLimit_orig;
}

static void Check_baseless(void)
{
    if (!BIT(world->rules->mode, TEAM_PLAY))
	options.baselessPausing = false;
}

void Timing_setup(void)
{
    LIMIT(FPS, 1, MAX_SERVER_FPS);
    LIMIT(options.gameSpeed, 0.0, FPS);
#ifndef SELECT_SCHED
    LIMIT(options.timerResolution, 0, 100);
#endif
    if (options.gameSpeed == 0.0)
	options.gameSpeed = FPS;
    if (options.gameSpeed < FPS / 50.)
	options.gameSpeed = FPS / 50.;

    /*
     * Calculate amount of game time that elapses per frame.
     */
    timeStep = options.gameSpeed / FPS;

    /*
     * Calculate amount of real time that elapses per frame.
     */
    timePerFrame = 1.0 / FPS;

    /*
     * EXPERIMENTAL FEATURE:
     * Negative values for friction give an "acceleration area".
     * kps - maybe we should remove FrictionArea and add AccelerationArea.
     */
    LIMIT(options.frictionSetting, -1.0, 1.0);
    friction = options.frictionSetting;
    if (friction < 1.0)
	friction = 1.0 - pow(1.0 - friction, timeStep);

    /* Adjust friction area friction suitable for gameSpeed */
    {
	int i;

	LIMIT(options.blockFriction, -1.0, 1.0);

	for (i = 0; i < Num_frictionAreas(); i++) {
	    friction_area_t *fa = FrictionArea_by_index(i);
	    double fric;

	    /*
	     * On xp maps, use blockFriction for all the friction areas,
	     * on xp2 maps each friction area has an own friction value.
	     */
	    if (is_polygon_map)
		fric = fa->friction_setting;
	    else
		fric = options.blockFriction;

	    LIMIT(fric, -1.0, 1.0);
	    if (fric < 1.0)
		fric = 1.0 - pow(1.0 - fric, timeStep);
	    fa->friction = fric;
	}
    }

    /* ecm size used to be halved every update on old servers */
    ecmSizeFactor = pow(0.5, timeStep);

    {
	double cor_angle;

	cor_angle = options.coriolis * PI / 180.0;

	coriolisCosine = cos(cor_angle / timeStep);
	coriolisSine = sin(cor_angle / timeStep);
    }

#ifdef SELECT_SCHED
    install_timer_tick(NULL, FPS);
#else
    install_timer_tick(NULL, options.timerResolution ? options.timerResolution
		       : FPS);
#endif
}
