/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 1991-2004 by
 *
 *      Uoti Urpala          <uau@users.sourceforge.net>
 *      Erik Andersson
 *      Kristian Söderblom
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

#include "xpserver.h"

/* Recording uses one byte for a scheduler slot, with 127 reserved. */
#define NUM_INPUT_HANDLERS 126

struct io_handler {
    socket_handle_t	fd;
    sched_input_fn	func;
    void		*arg;
    bool		active;
};

static struct io_handler input_handlers[NUM_INPUT_HANDLERS];
static struct io_handler record_handlers[NUM_INPUT_HANDLERS];
static fd_set input_mask;
static socket_handle_t max_fd = SOCK_FD_INVALID;
static bool input_inited = false;
#ifdef _WINDOWS
static volatile LONG sched_running;
static volatile LONG sched_stop_requested;
#else
static volatile bool sched_running = false;
#endif

void stop_sched(void)
{
#ifdef _WINDOWS
    InterlockedExchange(&sched_stop_requested, 1);
    InterlockedExchange(&sched_running, 0);
#else
    sched_running = false;
#endif
}

static bool sched_start(void)
{
#ifdef _WINDOWS
    if (InterlockedCompareExchange(&sched_stop_requested, 0, 0) != 0)
	return false;
    if (InterlockedCompareExchange(&sched_running, 1, 0) != 0)
	dumpcore("sched already running");
    if (InterlockedCompareExchange(&sched_stop_requested, 0, 0) != 0) {
	InterlockedExchange(&sched_running, 0);
	return false;
    }
#else
    if (sched_running)
	dumpcore("sched already running");
    sched_running = true;
#endif
    return true;
}

static bool sched_is_running(void)
{
#ifdef _WINDOWS
    return InterlockedCompareExchange(&sched_running, 0, 0) != 0;
#else
    return sched_running;
#endif
}

static void io_dummy(socket_handle_t fd, void *arg)
{
    UNUSED_PARAM(fd);
    xpprintf("io_dummy called!  (%p)\n", arg);
}

static void clear_handler(struct io_handler *handler)
{
    handler->fd = SOCK_FD_INVALID;
    handler->func = io_dummy;
    handler->arg = NULL;
    handler->active = false;
}

static void init_input_handlers(void)
{
    int i;

    if (input_inited)
	return;
    input_inited = true;
    FD_ZERO(&input_mask);
    for (i = 0; i < NUM_INPUT_HANDLERS; i++) {
	clear_handler(&input_handlers[i]);
	clear_handler(&record_handlers[i]);
    }
}

static int find_input_slot(struct io_handler *handlers, socket_handle_t fd)
{
    int i;

    for (i = 0; i < NUM_INPUT_HANDLERS; i++) {
	if (handlers[i].active && handlers[i].fd == fd)
	    return i;
    }
    return SOCK_IS_ERROR;
}

static int find_free_input_slot(struct io_handler *handlers)
{
    int i;

    for (i = 0; i < NUM_INPUT_HANDLERS; i++) {
	if (!handlers[i].active)
	    return i;
    }
    return SOCK_IS_ERROR;
}

static void update_max_fd(void)
{
    int i;

    max_fd = SOCK_FD_INVALID;
    for (i = 0; i < NUM_INPUT_HANDLERS; i++) {
	if (!input_handlers[i].active)
	    continue;
	if (max_fd == SOCK_FD_INVALID || input_handlers[i].fd > max_fd)
	    max_fd = input_handlers[i].fd;
    }
}

int install_input(sched_input_fn func, socket_handle_t fd, void *arg)
{
    struct io_handler *handlers;
    int slot;

    init_input_handlers();
    handlers = playback ? record_handlers : input_handlers;
    if (playback) {
	slot = (int)fd;
	if (slot < 0 || slot >= NUM_INPUT_HANDLERS)
	    return SOCK_IS_ERROR;
    } else {
#ifndef _WINDOWS
	if (fd < 0 || fd >= FD_SETSIZE) {
	    error("socket descriptor is outside fd_set capacity: %d", fd);
	    return SOCK_IS_ERROR;
	}
#endif
	if (find_input_slot(input_handlers, fd) != SOCK_IS_ERROR) {
	    error("input handler already registered");
	    return SOCK_IS_ERROR;
	}
	slot = find_free_input_slot(input_handlers);
	if (slot == SOCK_IS_ERROR) {
	    error("no free input handler slots");
	    return SOCK_IS_ERROR;
	}
    }
    if (handlers[slot].active)
	return SOCK_IS_ERROR;

    handlers[slot].fd = fd;
    handlers[slot].func = func;
    handlers[slot].arg = arg;
    handlers[slot].active = true;
    if (!playback) {
	FD_SET(fd, &input_mask);
	update_max_fd();
    }
    return slot;
}

int replace_input(socket_handle_t old_fd, socket_handle_t new_fd)
{
    int slot;

    if (playback)
	return SOCK_IS_ERROR;
    init_input_handlers();
    slot = find_input_slot(input_handlers, old_fd);
    if (slot == SOCK_IS_ERROR
	|| find_input_slot(input_handlers, new_fd) != SOCK_IS_ERROR)
	return SOCK_IS_ERROR;
#ifndef _WINDOWS
    if (new_fd < 0 || new_fd >= FD_SETSIZE)
	return SOCK_IS_ERROR;
#endif

    FD_CLR(old_fd, &input_mask);
    input_handlers[slot].fd = new_fd;
    FD_SET(new_fd, &input_mask);
    update_max_fd();
    return slot;
}

void remove_input(socket_handle_t fd)
{
    struct io_handler *handlers;
    int slot;

    init_input_handlers();
    handlers = playback ? record_handlers : input_handlers;
    slot = playback ? (int)fd : find_input_slot(input_handlers, fd);
    if (slot < 0 || slot >= NUM_INPUT_HANDLERS || !handlers[slot].active)
	return;

    if (!playback)
	FD_CLR(fd, &input_mask);
    clear_handler(&handlers[slot]);
    if (!playback)
	update_max_fd();
}

static int sched_select_wait(fd_set *readmask, struct timeval *timeout)
{
#ifdef _WINDOWS
    return select(0, readmask, NULL, NULL, timeout);
#else
    int nfds = max_fd == SOCK_FD_INVALID ? 0 : max_fd + 1;

    return select(nfds, readmask, NULL, NULL, timeout);
#endif
}

static int sched_select_interrupted(void)
{
#ifdef _WINDOWS
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

static void sched_select_error(void)
{
    error("sched select error");

    End_game();
}

#ifdef SELECT_SCHED

static long	timer_freq;
static void	(*timer_handler)(void);
static double	frametime;	/* time between 2 frames in seconds */
static double	t_nextframe;

static void setup_timer(void)
{
    struct timeval tv;
    double t;

    if (timer_freq <= 0 || timer_freq > MAX_SERVER_FPS) {
	error("illegal timer frequency: %ld", timer_freq);
	exit(1);
    }

    frametime = 1.0 / (double)timer_freq;

    gettimeofday(&tv, NULL);
    t = timeval_to_seconds(&tv);
    t_nextframe = t + frametime;
}

/*
 * Configure timer tick callback.
 */
void install_timer_tick(void (*func)(void), int freq)
{
    if (func != NULL) /* NULL to change freq, keep same handler */
	timer_handler = func;
    timer_freq = freq;
    setup_timer();
}


/*
 * If you set skip_to the server calculates frames
 * until that value of main_loops as fast as it can.
 * If you manage to record a bug, you can got to the
 * frame where it happens quickly.
 */
unsigned long skip_to = 0;

static void dispatch_frame(void)
{
    /* RECORDING STUFF */
    if (playback) {
	while (*playback_sched) {
	    if (*playback_sched == 127) {
		playback_sched++;
		Get_recording_data();
	    }
	    else {
		struct io_handler *ioh;
		ioh = &record_handlers[*playback_sched++ - 1];
		(*(ioh->func))(ioh->fd, ioh->arg);
	    }
	}
	playback_sched++;
    }
    else if (record)
	*playback_sched++ = 0;
    /* RECORDING STUFF END */

    if (timer_handler)
	(*timer_handler)();
}

static void dispatch_input(fd_set *readmask, int n)
{
    int i;
    for (i = NUM_INPUT_HANDLERS - 1; i >= 0; i--) {
	struct io_handler *ioh = &input_handlers[i];

	if (!ioh->active || !FD_ISSET(ioh->fd, readmask))
	    continue;

	/* RECORDING STUFF */
	record = playback = 0;
	if (rrecord && i > 0) {
	    *playback_sched++ = i + 1;
	    record = 1;
	}
	/* RECORDING STUFF END */

	(*(ioh->func))(ioh->fd, ioh->arg);

	/* RECORDING STUFF */
	record = rrecord;
	playback = rplayback;
	/* RECORDING STUFF END */

	if (--n == 0)
	    break;
    }
}

#ifdef __EMSCRIPTEN__
int sched_pump(int frame)
{
    playback = rplayback;
    record = rrecord;
    Handle_recording_buffers();
    fd_set readmask = input_mask;
    struct timeval timeout = {0, 0};
    int ready = playback ? 0 : sched_select_wait(&readmask, &timeout);
    if (ready == SOCK_IS_ERROR && !sched_select_interrupted())
        sched_select_error();
    if (ready > 0) dispatch_input(&readmask, ready);
    if (frame) dispatch_frame();
    return ready > 0 ? ready : 0;
}
#endif

/*
 * I/O + timer dispatcher.
 */
void sched(void)
{
    int n;
    double t_now, t_wait;
    struct timeval tv, wait_tv;

    playback = rplayback;

    if (!sched_start())
	return;

    gettimeofday(&tv, NULL);
    t_now = timeval_to_seconds(&tv);
    t_nextframe = t_now + frametime;

    while (sched_is_running()) {
	fd_set readmask = input_mask;

	gettimeofday(&tv, NULL);
	t_now = timeval_to_seconds(&tv);
	t_wait = t_nextframe - t_now;

	/* heuristics for different cases */
	if (t_wait < 0) {
	    if (t_wait < -2 * frametime) {
		/* long freeze, schedule frame now */
		t_nextframe = t_now;
		t_wait = 0;
	    } else
		t_wait = 0;
	} else {
	    if (t_wait > 2 * frametime) {
		/* nasty, someone changed the time! might aswell start over */
		t_nextframe = t_now + frametime;
		t_wait = frametime;
	    }
	}

	if (main_loops < skip_to) {
	    t_nextframe = t_now;
	    t_wait = 0;
	}
	
	/* RECORDING STUFF */
	Handle_recording_buffers();
	/* RECORDING STUFF END */

	wait_tv = seconds_to_timeval(t_wait);
	n = sched_select_wait(&readmask, &wait_tv);

	if (n <= 0) {
	    if (n == SOCK_IS_ERROR && !sched_select_interrupted())
		sched_select_error();

	    dispatch_frame();

#if 1
	    /* stable 50 fps as deity (2.6.x kernel) */
	    t_nextframe += frametime - 0.0000028571;
#else
	    t_nextframe += frametime;
#endif
	}
	else {
	    dispatch_input(&readmask, n);
	}
    }
}

#else /* SELECT_SCHED */

static volatile long	timer_ticks;	/* SIGALRMs that have occurred */
static long		timers_used;	/* SIGALRMs that have been used */
static long		timer_freq;	/* rate at which timer ticks. (in FPS) */
#ifndef _WINDOWS
static void		(*timer_handler)(void);
#else
static	TIMERPROC	timer_handler;
#endif
static time_t		current_time;
static int		ticks_till_second;

/*
 * Block or unblock a single signal.
 */
static void sig_ok(int signum, int flag)
{
#if !defined(_WINDOWS)
    sigset_t    sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, signum);
    if (sigprocmask((flag) ? SIG_UNBLOCK : SIG_BLOCK, &sigset, NULL) == -1) {
	error("sigprocmask(%d,%d)", signum, flag);
	exit(1);
    }
#endif
}

/*
 * Prevent the real-time timer from interrupting system calls.
 * Globally accessible.
 */
void block_timer(void)
{
#ifndef _WINDOWS
    sig_ok(SIGALRM, 0);
#endif
}

/*
 * Unblock the real-time timer.
 * Globally accessible.
 */
void allow_timer(void)
{
#ifndef _WINDOWS
    sig_ok(SIGALRM, 1);
#endif
}


/*
 * Catch SIGALRM.
 */
static void catch_timer(int signum)
{
    static unsigned int		timer_count = 0;

    UNUSED_PARAM(signum);
    timer_count += FPS;
    if (timer_count >= (unsigned)options.timerResolution) {
	timer_count -= options.timerResolution;
	timer_ticks++;
	if (timer_count >= (unsigned)options.timerResolution)
	    /*
	     * Don't let timer_count grow boundlessly with timer resolution 0
	     * now that timer resolution can be changed at runtime.
	     */
	    timer_count = 0;
    }
}


/*
 * Setup the handling of the SIGALRM signal
 * and setup the real-time interval timer.
 */
static void setup_timer(void)
{
#ifndef _WINDOWS

    struct itimerval itv;
    struct sigaction act;

    /*
     * Prevent SIGALRMs from disturbing the initialization.
     */
    block_timer();

    /*
     * Install a signal handler for the alarm signal.
     */
    act.sa_handler = catch_timer;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGALRM);
    if (sigaction(SIGALRM, &act, (struct sigaction *)NULL) == -1) {
	error("sigaction SIGALRM");
	exit(1);
    }

    /*
     * Install a real-time timer.
     */
    if (timer_freq <= 0 || timer_freq > MAX_SERVER_FPS) {
	error("illegal timer frequency: %ld", timer_freq);
	exit(1);
    }

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 1000000 / timer_freq;
    itv.it_value = itv.it_interval;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
	error("setitimer");
	exit(1);
    }

    timers_used = timer_ticks;
    time(&current_time);
    ticks_till_second = timer_freq;
#else
/*
    UINT cr = SetTimer(NULL, 0, 1000/timer_freq, timer_handler);
    UINT cr = SetTimer(NULL, 0, 20, (TIMERPROC)ServerThreadTimerProc);
    if (!cr)
	error("Can't create timer");
*/
#endif
    /*
     * Allow the real-time timer to generate SIGALRM signals.
     */
    allow_timer();
}

/*
 * Configure timer tick callback.
 */
#ifndef _WINDOWS
void install_timer_tick(void (*func)(void), int freq)
{
    if (func != NULL) /* NULL to change freq, keep same handler */
	timer_handler = func;
    timer_freq = freq;
    setup_timer();
}
#else

typedef void (__stdcall *windows_timer_t)(void *, unsigned int, unsigned int, unsigned long);

void install_timer_tick(windows_timer_t func, int freq)
{
    if (func != NULL)
	timer_handler = (TIMERPROC)func;
    timer_freq = freq;
    setup_timer();
}
#endif


/*
 * Linked list of timeout callbacks.
 */
struct to_handler {
    struct to_handler	*next;
    time_t		when;
    void		(*func)(void *);
    void		*arg;
};
static struct to_handler *to_busy_list = NULL;
static struct to_handler *to_free_list = NULL;
static int		to_min_free = 3;
static int		to_max_free = 5;
static int		to_cur_free = 0;

static void to_fill(void)
{
    if (to_cur_free < to_min_free) {
	do {
	    struct to_handler *top =
		(struct to_handler *)malloc(sizeof(struct to_handler));
	    if (!top) {
		break;
	    }
	    top->next = to_free_list;
	    to_free_list = top;
	    to_cur_free++;
	} while (to_cur_free < to_max_free);
    }
}

static struct to_handler *to_alloc(void)
{
    struct to_handler *top;

    to_fill();
    if (!to_free_list) {
	error("Not enough memory for timeouts");
	exit(1);
    }

    top = to_free_list;
    to_free_list = top->next;
    to_cur_free--;
    top->next = 0;

    return top;
}

static void to_free(struct to_handler *top)
{
    if (to_cur_free < to_max_free) {
	top->next = to_free_list;
	to_free_list = top;
	to_cur_free++;
    }
    else {
	free(top);
    }
}

/*
 * Configure timout callback.
 */
void install_timeout(void (*func)(void *), int offset, void *arg)
{
    struct to_handler *top = to_alloc();
    top->func = func;
    top->when = current_time + offset;
    top->arg = arg;
    if (!to_busy_list || to_busy_list->when >= top->when) {
	top->next = NULL;
	to_busy_list = top;
    }
    else {
	struct to_handler *prev = to_busy_list;
	struct to_handler *lp = prev->next;
	while (lp && lp->when < top->when) {
	    prev = lp;
	    lp = lp->next;
	}
	top->next = lp;
	prev->next = top;
    }
}

void remove_timeout(void (*func)(void *), void *arg)
{
    struct to_handler *prev = 0;
    struct to_handler *lp = to_busy_list;
    while (lp) {
	if (lp->func == func && lp->arg == arg) {
	    struct to_handler *top = lp;
	    lp = lp->next;
	    if (prev) {
		prev->next = lp;
	    } else {
		to_busy_list = lp;
	    }
	    to_free(top);
	}
	else {
	    prev = lp;
	    lp = lp->next;
	}
    }
}

static void timeout_chime(void)
{
    while (to_busy_list && to_busy_list->when <= current_time) {
	struct to_handler *top = to_busy_list;
	void (*func)(void *) = top->func;
	void *arg = top->arg;
	to_busy_list = top->next;
	to_free(top);
	(*func)(arg);
    }
}

/*
 * I/O + timer dispatcher.
 * Windows pumps this one time
 */

long skip_to = 0;

#ifndef _WINDOWS
void sched(void)
{
    int			i, n, io_todo = 3;
    struct timeval	tv, *tvp = &tv;

    playback = rplayback;

    if (!sched_start())
	return;

    while (sched_is_running()) {

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (main_loops < skip_to && timers_used >= timer_ticks)
	    timer_ticks++;
	if (io_todo == 0 && timers_used < timer_ticks) {
	    io_todo = 1 + (timer_ticks - timers_used);
	    tvp = &tv;
	    if (playback) {
		while (*playback_sched) {
		    if (*playback_sched == 127) {
			playback_sched++;
			Get_recording_data();
		    }
		    else {
			struct io_handler *ioh;
			ioh = &record_handlers[*playback_sched++ - 1];
			(*(ioh->func))(ioh->fd, ioh->arg);
		    }
		}
		playback_sched++;
	    }
	    else if (record)
		*playback_sched++ = 0;

	    if (timer_handler)
		(*timer_handler)();

	    do {
		++timers_used;
		if (--ticks_till_second <= 0) {
		    ticks_till_second += timer_freq;
		    current_time++;
		    timeout_chime();
		}
	    } while (timers_used + 1 < timer_ticks);
	}
	else {
	    fd_set readmask;
	    readmask = input_mask;
	    Handle_recording_buffers();
	    n = sched_select_wait(&readmask, tvp);
	    if (n <= 0) {
		if (n == SOCK_IS_ERROR && !sched_select_interrupted())
		    sched_select_error();
		io_todo = 0;
	    }
	    else {
		for (i = NUM_INPUT_HANDLERS - 1; i >= 0; i--) {
		    struct io_handler *ioh = &input_handlers[i];

		    if (!ioh->active || !FD_ISSET(ioh->fd, &readmask))
			continue;
		    record = playback = 0;
		    if (rrecord && i > 0) {
			*playback_sched++ = i + 1;
			record = 1;
		    }
		    (*(ioh->func))(ioh->fd, ioh->arg);
		    record = rrecord;
		    playback = rplayback;
		    if (--n == 0)
			break;
		}
		if (io_todo > 0)
		    io_todo--;
	    }
	    if (io_todo == 0)
		tvp = NULL;
	}
    }
}

#else /* _WINDOWS */
void sched(void)
{
    int			i, n, io_todo = 3;
    struct timeval	tv, *tvp = &tv;

    if (NumPlayers > NumRobots + NumPseudoPlayers
	|| login_in_progress != 0
	|| NumQueuedPlayers > 0) {

	/* need fast I/O checks now! (2 or 3 times per frames) */
	tv.tv_sec = 0;
	/* KOERBER */
	/*	tv.tv_usec = 1000000 / (3 * timer_freq + 1); */
	tv.tv_usec = 1000000 / (10 * timer_freq + 1);
    }
    else {
	/* slow I/O checks are possible here... (2 times per second) */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
    }


    if (io_todo == 0 && timers_used < timer_ticks) {
	io_todo = 1 + (timer_ticks - timers_used);
	tvp = &tv;

	do {
	    ++timers_used;
	    if (--ticks_till_second <= 0) {
		ticks_till_second += timer_freq;
		current_time++;
		timeout_chime();
	    }
	} while (timers_used + 1 < timer_ticks);
    }
    else {
	fd_set readmask;
	readmask = input_mask;
	n = sched_select_wait(&readmask, tvp);
	if (n <= 0) {
	    if (n == SOCK_IS_ERROR && !sched_select_interrupted()) {
		sched_select_error();
	    }
	    io_todo = 0;
	}
	else {
	    for (i = NUM_INPUT_HANDLERS - 1; i >= 0; i--) {
		struct io_handler *ioh = &input_handlers[i];

		if (!ioh->active || !FD_ISSET(ioh->fd, &readmask))
		    continue;
		(*(ioh->func))(ioh->fd, ioh->arg);
		if (--n == 0)
		    break;
	    }
	    if (io_todo > 0) {
		io_todo--;
	    }
	}
	if (io_todo == 0) {
	    tvp = NULL;
	}
    }
}

#endif /* _WINDOWS */

#endif /* SELECT_SCHED */
