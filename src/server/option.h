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

#ifndef OPTION_H
#define OPTION_H

extern struct options {
    int		maxRobots;
    int		minRobots;
    char	*robotFile;
    int		robotsTalk;
    int		robotsLeave;
    int		robotLeaveLife;
    int		robotTeam;
    bool	restrictRobots;
    bool	reserveRobotTeam;
    int 	robotTicksPerSecond;
    list_t	expandList;		/* Predefined settings. */
    double	shotMass;
    double	shipMass;
    double	shotSpeed;
    double	gravity;
    double	ballMass;
    double	minItemMass;
    int		maxPlayerShots;
    double	shotLife;
    double	pulseSpeed;
    double	pulseLength;
    double	pulseLife;
    bool	shotsGravity;
    bool	shotHitFuelDrainUsesKineticEnergy;
    double      ballCollisionFuelDrain;
    double      playerCollisionFuelDrain;
    bool        treasureCollisionKills;
    double	fireRepeatRate;
    double	laserRepeatRate;
    bool	Log;
    bool	RawMode;
    bool	NoQuit;
    bool	logRobots;
    int		framesPerSecond;
    char	*mapFileName;
    bool        strictMap;      /**< Fail startup if the specified map cannot be read. */
    char	*mapData;
    int		mapWidth;
    int		mapHeight;
    char	*mapName;
    char	*mapAuthor;
    int 	contactPort;
    char	*contactTransport;
    char	*gameTransport;
    char	*serverHost;
    char	*greeting;
    bool	allowPlayerCrashes;
    bool	allowPlayerBounces;
    bool	allowPlayerKilling;
    bool	allowShields;
    bool	playerStartsShielded;
    bool	shotsWallBounce;
    bool	ballsWallBounce;
    bool        ballCollisionDetaches;
    bool	ballCollisions;
    bool	ballSparkCollisions;
    bool	minesWallBounce;
    bool	itemsWallBounce;
    bool	missilesWallBounce;
    bool	sparksWallBounce;
    bool	debrisWallBounce;
    bool	asteroidsWallBounce;
    bool	pulsesWallBounce;
    bool	cloakedExhaust;
    bool	ecmsReprogramMines;
    bool	ecmsReprogramRobots;
    double	maxObjectWallBounceSpeed;
    double	maxSparkWallBounceSpeed;  
    double	maxShieldedWallBounceSpeed;
    double	maxUnshieldedWallBounceSpeed;
    int		playerWallBounceType;
    double	playerWallBounceBrakeFactor;
    double	playerBallBounceBrakeFactor;
    double	playerWallFriction;
    double	objectWallBounceBrakeFactor;
    double	objectWallBounceLifeFactor;
    double	wallBounceDestroyItemProb;
    double	afterburnerPowerMult;

    bool	limitedVisibility;
    double	minVisibilityDistance;
    double	maxVisibilityDistance;
    bool	limitedLives;
    int		worldLives;
    bool	endOfRoundReset;
    int		resetOnHuman;
    bool	allowAlliances;
    bool	announceAlliances;
    bool	teamPlay;
    bool	teamFuel;
    bool	teamCannons;
    int		cannonSmartness;
    bool	cannonsPickupItems;
    bool	cannonFlak;
    double	cannonDeadTicks;
    double	minCannonShotLife;
    double	maxCannonShotLife;
    double      survivalScore;
    double	cannonShotSpeed;
    bool	keepShots;
    bool	teamImmunity;
    bool	tagGame;
    bool	timing;
    bool	ballrace;
    bool	ballrace_connect;
    bool	edgeWrap;
    bool	edgeBounce;
    bool	extraBorder;
    ipos_t	gravityPoint;
    double	gravityAngle;
    bool	gravityPointSource;
    bool	gravityClockwise;
    bool	gravityAnticlockwise;
    bool	gravityVisible;
    bool	wormholeVisible;
    bool	itemConcentratorVisible;
    bool	asteroidConcentratorVisible;
    double	wormholeStableTicks;
    char	*defaultsFileName;
    char	*passwordFileName;
    int		nukeMinSmarts;
    int		nukeMinMines;
    double	nukeClusterDamage;
    double	nukeDebrisLife;
    double	mineFuseTicks;
    double	mineLife;
    double	minMineSpeed;
    double	missileLife;
    double	baseMineRange;
    double	mineShotDetonateDistance;

    double	shotKillScoreMult;
    double	torpedoKillScoreMult;
    double	smartKillScoreMult;
    double	heatKillScoreMult;
    double	clusterKillScoreMult;
    double	laserKillScoreMult;
    double	tankKillScoreMult;
    double	runoverKillScoreMult;
    double	ballKillScoreMult;
    double	explosionKillScoreMult;
    double	shoveKillScoreMult;
    double	crashScoreMult;
    double	mineScoreMult;
    double	selfKillScoreMult;
    double	selfDestructScoreMult;
    double	unownedKillScoreMult;
    double	cannonKillScoreMult;
    double	tagItKillScoreMult;
    double	tagKillItScoreMult;
    bool    	zeroSumScoring;

    double	destroyItemInCollisionProb;
    bool 	allowSmartMissiles;
    bool 	allowHeatSeekers;
    bool 	allowTorpedoes;
    bool 	allowNukes;
    bool	allowClusters;
    bool	allowModifiers;
    bool	allowLaserModifiers;
    bool	allowShipShapes;

    bool	shieldedItemPickup;
    bool	shieldedMining;
    bool	laserIsStunGun;
    bool	targetKillTeam;
    bool	targetSync;
    double	targetDeadTicks;
    bool	reportToMetaServer;
    int		metaUpdateMaxSize;
    bool	searchDomainForXPilot;
    char	*denyHosts;

    bool	playersOnRadar;
    bool	missilesOnRadar;
    bool	minesOnRadar;
    bool	nukesOnRadar;
    bool	treasuresOnRadar;
    bool	asteroidsOnRadar;
    bool 	identifyMines;
    bool	distinguishMissiles;
    int		maxMissilesPerPack;
    int		maxMinesPerPack;
    bool	targetTeamCollision;
    bool	treasureKillTeam;
    bool	captureTheFlag;
    int		specialBallTeam;
    bool	treasureCollisionDestroys;
    bool	treasureCollisionMayKill;
    bool	wreckageCollisionMayKill;
    bool	asteroidCollisionMayKill;

    double	ballConnectorSpringConstant;
    double	ballConnectorDamping;
    double	maxBallConnectorRatio;
    double	ballConnectorLength;
    bool	connectorIsString;
    double	ballRadius;
    bool	multipleConnectors;

    double 	dropItemOnKillProb;
    double	detonateItemOnKillProb;
    double 	movingItemProb;
    double	randomItemProb;
    double	rogueHeatProb;
    double	rogueMineProb;
    double	itemProbMult;
    double	cannonItemProbMult;
    double	asteroidItemProb;
    int		asteroidMaxItems;
    double	maxItemDensity;
    double	maxAsteroidDensity;
    double	itemConcentratorRadius;
    double	itemConcentratorProb;
    double	asteroidConcentratorRadius;
    double	asteroidConcentratorProb;
    double	gameDuration;
    bool	baselessPausing;
    double	pauseTax;
    int		pausedFPS;
    int		waitingFPS;
    int		deadFPS;

    char	*motdFileName;
    char	*scoreTableFileName;
    char	*adminMessageFileName;
    int		adminMessageFileSizeLimit;
    char	*rankFileName;
    char	*rankWebpageFileName;
    char	*rankWebpageCSS;
    double	frictionSetting;
    double	blockFriction;
    bool	blockFrictionVisible;
    double	coriolis;
    double	checkpointRadius;
    int		raceLaps;
    bool	lockOtherTeam;
    bool	loseItemDestroys;
    int		maxOffensiveItems;
    int		maxDefensiveItems;

    int		maxVisibleObject;
    bool	pLockServer;
    bool	sound;
    int		timerResolution;

    int		maxRoundTime;
    int		roundsToPlay;

    bool	useDebris;
    bool	useWreckage;
    bool	ignore20MaxFPS;
    char	*password;

    char	*robotUserName;
    char	*robotHostName;

    char	*tankUserName;
    char	*tankHostName;
    double	tankScoreDecrement;

    bool	selfImmunity;

    char	*defaultShipShape;
    char	*tankShipShape;

    int		clientPortStart;
    int		clientPortEnd;

    int		maxPauseTime;
    int		maxIdleTime;
    int		maxClientsPerIP;

    int		playerLimit;
    int		playerLimit_orig;
    int		recordMode;
    int		recordFlushInterval;
    int		constantScoring;
    int		eliminationRace;
    char	*dataURL;
    char	*recordFileName;
    double	gameSpeed;
    bool	infinityControls;
    double  	turnPushPersistence;
    double  	turnGrip;
    double	thrustWidth;
    double	thrustMass;
    double	sparkSpeed;
    double	constantSpeed;
    bool	legacyMode;
    bool	ballStyles;
    bool	ignoreMaxFPS;
    bool	polygonMode;
    bool	fastAim;
    bool	teamcup;
    char	*teamcupName;
    char	*teamcupMailAddress;
    char	*teamcupScoreFileNamePrefix;
    int		teamcupMatchNumber;

    double	mainLoopTime;
    int		cellGetObjectsThreshold;  
} options;

/*
 * Prototypes for option.c
 */
void Options_parse(void);
void Options_free(void);
bool Convert_string_to_int(const char *value_str, int *int_ptr);
bool Convert_string_to_float(const char *value_str, double *float_ptr);
bool Convert_string_to_bool(const char *value_str, bool *bool_ptr);
void Convert_list_to_string(list_t list, char **string);
void Convert_string_to_list(const char *value, list_t *list_ptr);

#endif
