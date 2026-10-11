/* One game is owned by one WASM instance. Legacy exit paths must unwind only
 * to the active host call, never terminate the embedding Node.js process. */
#include "xpserver.h"
#include <emscripten.h>
#include <setjmp.h>

static jmp_buf host_call;
static int started;
static int stopped;
static int status;
static int inside_host_call;

EM_JS(void, notify_ready, (void), {
    if (Module['onSocketReady']) Module['onSocketReady']();
});

static void socket_ready(int fd, void *context)
{
    (void)fd;
    (void)context;
    notify_ready();
}

static void socket_error(int fd, int error, const char *message, void *context)
{
    (void)error;
    (void)message;
    socket_ready(fd, context);
}

/** Linker wrapper: preserve non-returning legacy exits within the host call. */
__attribute__((noreturn)) void __wrap_exit(int code)
{
    status = code;
    stopped = 1;
    if (!inside_host_call) abort();
    longjmp(host_call, 1);
}

/** Start a game, borrowing argv until this call returns. */
int xp_server_start(int argc, char **argv)
{
    if (started) return 1;
    started = 1;
    inside_host_call = 1;
    if (setjmp(host_call)) {
        inside_host_call = 0;
        return status;
    }
    emscripten_set_socket_open_callback(NULL, socket_ready);
    emscripten_set_socket_listen_callback(NULL, socket_ready);
    emscripten_set_socket_connection_callback(NULL, socket_ready);
    emscripten_set_socket_message_callback(NULL, socket_ready);
    emscripten_set_socket_close_callback(NULL, socket_ready);
    emscripten_set_socket_error_callback(NULL, socket_error);
    status = Server_initialize(argc, argv);
    stopped = status != 0;
    inside_host_call = 0;
    return status;
}

/** Process ready input and optionally one frame; minus one means stopped. */
int xp_server_pump(int frame)
{
    if (!started || stopped) return -1;
    inside_host_call = 1;
    if (setjmp(host_call)) {
        inside_host_call = 0;
        return -1;
    }
    int ready = sched_pump(frame);
    inside_host_call = 0;
    return ready;
}

/** Stop once, saving rankings and closing network endpoints. */
void xp_server_stop(void)
{
    if (!started || stopped) return;
    inside_host_call = 1;
    if (!setjmp(host_call)) End_game();
    inside_host_call = 0;
}

/** Return completed game frames. */
int xp_server_frames(void) { return main_loops; }
/** Return current robot count. */
int xp_server_robots(void) { return NumRobots; }
/** Return the configured frame rate. */
int xp_server_fps(void) { return FPS; }
/** Return the most recent initialization or exit status. */
int xp_server_status(void) { return status; }
