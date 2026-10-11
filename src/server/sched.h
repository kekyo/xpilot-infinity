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

#ifndef	SCHED_H
#define	SCHED_H

void block_timer(void);
void allow_timer(void);

/** Callback invoked when a registered socket becomes readable. */
typedef void (*sched_input_fn)(socket_handle_t fd, void *arg);

/**
 * Register a socket with the server scheduler.
 *
 * @param func Callback invoked for readable input.
 * @param fd Native socket handle to monitor, or a recorded slot in playback.
 * @param arg Opaque callback argument.
 * @return Stable scheduler slot on success, or SOCK_IS_ERROR on failure.
 */
int install_input(sched_input_fn func, socket_handle_t fd, void *arg);
/**
 * Replace the native socket associated with an existing scheduler slot.
 *
 * @param old_fd Currently registered socket handle.
 * @param new_fd Replacement socket handle.
 * @return Stable scheduler slot on success, or SOCK_IS_ERROR on failure.
 */
int replace_input(socket_handle_t old_fd, socket_handle_t new_fd);
/** Remove a socket from the server scheduler. */
void remove_input(socket_handle_t fd);
void sched(void);
/** Request an orderly stop of the server scheduler. */
void stop_sched(void);

#ifdef __EMSCRIPTEN__
/**
 * Process ready input without waiting and optionally advance one game frame.
 * @param frame Nonzero when the host's next frame deadline has been reached.
 * @return Number of ready descriptors dispatched; no readiness means zero.
 * @remarks Call only after Server_initialize, from the owning Node event loop.
 */
int sched_pump(int frame);
#endif

#ifdef SELECT_SCHED

void install_timer_tick(void (*func)(void), int freq);

#else /* SELECT_SCHED */

#ifndef _WINDOWS
void install_timer_tick(void (*func)(void), int freq);
#else
extern	void install_timer_tick(void (__stdcall *func)(void *,unsigned int ,unsigned int ,unsigned long ), int freq);
#endif
void install_timeout(void (*func)(void *), int offset, void *arg);
void remove_timeout(void (*func)(void *), void *arg);

#endif /* SELECT_SCHED */

#endif
