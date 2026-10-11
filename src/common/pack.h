/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2000-2004 Uoti Urpala <uau@users.sourceforge.net>
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

#ifndef	PACK_H
#define	PACK_H

#define CAP_LETTER(c)	((c) = ((c)>='a' && (c)<='z') ? (c)-'a'+'A' : (c))

#define SERVER_PORT	15345		/* Port which server listens to. */

/*
 * Magic contact word.
 * The low 16 bits are the real magic word.
 * Bits 31-28 are the major version number.
 * Bits 27-24 are the minor version number.
 * Bits 23-20 are the patchlevel number.
 * Bits 19-16 are free to mean beta release or so.
 * These high bits only need to be changed when a new
 * client can't talk to an old server or vise versa.
 * Please don't change it more often than strictly necessary.
 * PLEASE NOTE: if you make your own private incompatible changes
 * justifying an increment of the version word then please
 * set the 4th bit.  Like 0x3108 and 0x3109 till 0x310F, etc.
 * This way we can use the lower 8 values for public releases.
 *
 * Reasons why it changed in the past:
 * 3.0.1: rewrite of contact pack protocol, because of
 * different structure layout rules on different architectures.
 * 3.0.2: rewrite of setup transmit from server to client to
 * make it possible for 64-bit machines and 32-bit machines
 * to join in the same game.  This was the last hardcoded
 * structure that was shared between client and server.
 * 3.0.3: implemented a version awareness system, so that
 * newer clients can join older servers and so that
 * newer servers can support older clients.
 * The client maintains a 'version' variable indicating
 * the version of the server it has joined and the server
 * maintains for each connection a 'connection_t->version'
 * and a 'player->version' variable.
 * 3.0.4: the so-called 'pizza-mode' introduced a new packet type.
 * The score packet now also includes pl->mychar.
 * 3.0.4.1: new laser weapon introduces another packet change.
 * Because there is an unofficial (and forbidden) 3.0.4 version floating
 * around the sub patchlevel number is used to distinguish versions.
 * A new display packet to tell the server what the view sizes are
 * and how many different debris intensities the client wants.
 * 3.0.4.2: new player-self status byte in self packet.
 * 3.0.4.3: different and incompatible laser packet.
 * New eyes packet to tell the client through wich players eyes we're
 * looking through in case the client is in game over move and it is locked
 * on someone else.
 * 3.1.0.0: new big patches implementing loads of new incompatible features.
 * Major cleanup.  Old clients (before 310) can't join new servers anymore.
 * 3.1.0.1: key-change-ids are now send as longs instead of bytes.
 * 3.1.0.2: different player status byte.
 * 3.1.0.3: different mine packet.
 * 3.2.0.0: New ship shape definition and big patches.
 * 3.2.0.1: Extended buffer for very large ship shape definitions.
 * 3.2.0.2: New mouse pointer control packet.
 * 3.2.5.0: Now client must ask for audio packets in order to get them.
 * 3.2.6.0: New map update packet.
 * 3.2.6.1: New player timing packet.
 * 3.2.8.0: New asyn packet.
 * 3.3.1.0: Different owner-only commands.
 * 3.3.2.0: Map decorations.
 * 3.4.0.0: Lose/drop item key.
 * 3.5.0.0: Player waiting queue.
 * 3.8.0.0: new items (deflector, hyperjump, phasing), keyboardsize and rounddelay.
 * 4.1.0.0: new item (mirror).
 * 4.2.0.0: new power/turnspeed behavior
 * 4.2.0.1: new item (armor).
 * 4.2.0.2: highest bit on in wreckagetype when deadly.
 * 4.2.0.3: different way of sending player item info.
 * 4.2.1.0: high bit in radar size means player is a teammate.
 * 4.3.0.0: transmit phasing separately from cloaking
 * 4.4.0.0: new object (asteroid)
 * 4.4.0.1: fast radar packet
 * 4.5.0.0: new team score packet; score packet made larger to send decimals
 * 4.5.0.1: temporary wormholes

 * Polygon branch
 * 4.F.0.9: 4.3.0.0 + xp2 map format
 * 4.F.1.0: Send_player(): Additional %c (1 when sending player's own info).
 * 4.F.1.1: support for everything in 4.5.0.1
 * 4.F.1.2: Show ships about to appear on bases, new team change packet.
 * 4.F.1.3: cumulative turning
 * 4.F.1.4: balls use polygon styles
 * 4.F.1.5: Possibility to change polygon styles.
 */
#define MAGIC_WORD		0xF4ED

#define MAGIC2VERSION(M)	(((M) >> 16) & 0xFFFF)
#define VERSION2MAGIC(V)	((((V) & 0xFFFF) << 16) | MAGIC_WORD)

/*
 * Which client versions can join this server.
 */
#ifdef SERVER
#define MIN_CLIENT_VERSION	0x4203
#endif

/*
 * Which server versions can this client join.
 */
#define MIN_SERVER_VERSION	0x4F09

/*
 * We want to keep support for servers using the old map format in the client,
 * but make incompatible changes while developing the new format. Therefore
 * there is a separate "old" range of allowed servers.
 */
#define MIN_OLD_SERVER_VERSION  0x4203

#define	MAX_STR_LEN		4096
#define	MAX_DISP_LEN		80
#define	MAX_NAME_LEN		16
#define	MAX_HOST_LEN		64

/*
 * Different contact pack types.
 */
#define	ENTER_GAME_pack		0x00
#define	ENTER_QUEUE_pack	0x01
#define	REPLY_pack		0x10
#define	REPORT_STATUS_pack	0x21
#define	OPTION_LIST_pack	0x28
/*#define	CORE_pack		0x30*/
#define	CONTACT_pack		0x31
/* The owner-only commands have a common bit high. */
#define PRIVILEGE_PACK_MASK	0x40
#define	LOCK_GAME_pack		0x62
#define	MESSAGE_pack		0x63
#define	SHUTDOWN_pack		0x64
#define	KICK_PLAYER_pack	0x65
/*#define	MAX_ROBOT_pack		0x66*/
#define	OPTION_TUNE_pack	0x67
#define	CREDENTIALS_pack	0x69

/*
 * Possible error codes returned.
 */
#define	SUCCESS		0x00		/* Operation successful */
#define	E_NOT_OWNER	0x01		/* Permission denied, not owner */
#define	E_GAME_FULL	0x02		/* Game is full, play denied */
#define	E_TEAM_FULL	0x03		/* Team is full, play denied */
#define	E_TEAM_NOT_SET	0x04		/* Need to specify a team */
#define	E_GAME_LOCKED	0x05		/* Game is locked, entry denied */
#define	E_NOT_FOUND	0x07		/* Player was not found */
#define	E_IN_USE	0x08		/* Name is already in use */
#define	E_SOCKET	0x09		/* Can't setup socket */
#define	E_INVAL		0x0A		/* Invalid input parameters */
#define	E_VERSION	0x0C		/* Incompatible version */
#define	E_NOENT		0x0D		/* No such variable */
#define	E_UNDEFINED	0x0E		/* Operation undefined */

#endif
