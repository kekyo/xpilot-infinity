/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2003-2004 Kristian Söderblom <kps@users.sourceforge.net>
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

#ifndef XPSERVER_H
#define XPSERVER_H

#define SERVER
#include "xpcommon.h"
#include "utf8_files.h"

#ifdef HAVE_LIBEXPAT
#  include <expat.h>
#else
#  error "Header expat.h missing. Please install expat."
#endif

#ifdef PLOCKSERVER
#  ifdef HAVE_SYS_MMAN_H
#    include <sys/mman.h>
#  elif defined HAVE_SYS_LOCK_H
#    include <sys/lock.h>
#  endif
#endif

#include "serverconst.h"
#include "object.h"
#include "player.h"
#include "asteroid.h"
#include "cannon.h"
#include "connection.h"
#include "defaults.h"
#include "map.h"
#include "modifiers.h"
#include "netserver.h"
#include "objpos.h"
#include "option.h"
#include "packet.h"
#include "rank.h"
#include "recwrap.h"
#include "robot.h"
#include "saudio.h"
#include "sched.h"
#include "setup.h"
#include "score.h"
#include "srecord.h"
#include "target.h"
#include "teamcup.h"
#include "tuner.h"
#include "walls.h"
#include "wormhole.h"
#include "server.h"

#endif /* XPSERVER_H */
