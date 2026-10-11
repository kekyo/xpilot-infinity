/*
 * XPilot Infinity/SDL, an SDL/OpenGL XPilot client.
 *
 * Copyright (C) 2003-2004 Juha Lindström <juhal@users.sourceforge.net>
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

#include "gameinput.h"

static void Check_pointer_move_interval(void)
{
    if (maxMouseTurnsPS > 0)
	Client_check_pointer_move_interval();
}

static GameLoopResult Refresh_network_fd(GameLoopState *state)
{
    state->network_fd = Net_fd();
    return GAME_LOOP_CONTINUE;
}

GameLoopResult Game_loop_prepare(GameLoopState *state)
{
    int result;

    if (state == NULL)
	return GAME_LOOP_STOP;
    state->network_fd = SOCK_FD_INVALID;
    state->previous_network_result = 0;

    result = Net_input();
    if (result == -1) {
	error("Bad server input");
	return GAME_LOOP_STOP;
    }
    state->previous_network_result = result;
    if (Game_input_process_batch())
	return GAME_LOOP_STOP;
    if (Net_flush() == -1)
	return GAME_LOOP_STOP;
    if (Refresh_network_fd(state) != GAME_LOOP_CONTINUE)
	return GAME_LOOP_STOP;
    Net_key_change();
    return GAME_LOOP_CONTINUE;
}

static GameLoopResult Advance_game_loop(GameLoopState *state,
					int network_ready)
{
    int result;

    if (network_ready || state->previous_network_result > 1) {
	result = Net_input();
	state->previous_network_result = result;
	if (result == -1) {
	    warn("Bad net input.  Have a nice day!");
	    return GAME_LOOP_STOP;
	}
    }
    if (Game_input_process_batch())
	return GAME_LOOP_STOP;
    if (Net_flush() == -1) {
	error("Bad net flush");
	return GAME_LOOP_STOP;
    }
    return Refresh_network_fd(state);
}

GameLoopResult Game_loop_step(GameLoopState *state, int network_ready)
{
    if (state == NULL)
	return GAME_LOOP_STOP;
    Check_pointer_move_interval();
    return Advance_game_loop(state, network_ready);
}

void Game_loop(void)
{
    GameLoopState state;
    fd_set rfds;
    int n;
    struct timeval tv;

    if (Game_loop_prepare(&state) != GAME_LOOP_CONTINUE)
	return;

    while (1) {
        if (state.network_fd == SOCK_FD_INVALID) {
            error("Native game loop requires a socket filedescriptor");
            return;
        }
	FD_ZERO(&rfds);
	FD_SET(state.network_fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 5000; /* wait max 5 ms */

	Check_pointer_move_interval();

/* Winsock retains nfds only for Berkeley compatibility and ignores it. */
#ifdef _WINDOWS
        n = select(0, &rfds, NULL, NULL, &tv);
#else
        n = select(state.network_fd + 1, &rfds, NULL, NULL, &tv);
#endif
	if (n == SOCK_IS_ERROR) {
#ifdef _WINDOWS
	    if (WSAGetLastError() == WSAEINTR)
#else
	    if (errno == EINTR)
#endif
		continue;
	    error("Select failed");
	    return;
        }
	if (Advance_game_loop(&state, n > 0) != GAME_LOOP_CONTINUE)
	    return;
    }
}
