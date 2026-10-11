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
static int prepared;

extern bool __real_Contact_configure(void);

static bool option_is_default(const char *name)
{
    optOrigin origin = OPT_INIT;
    Option_get_value(name, &origin);
    return origin == OPT_INIT;
}

EM_JS(char *, browser_root, (void), {
    return Module['browserRoot'] ? stringToNewUTF8(Module['browserRoot']) : 0;
});

/** Apply host defaults only after native configuration precedence is resolved.
 * Explicit options in either files or argv remain authoritative. */
bool __wrap_Contact_configure(void)
{
    if (option_is_default("noQuit")) options.NoQuit = true;
    if (option_is_default("reportToMetaServer")) options.reportToMetaServer = false;
    if (option_is_default("listen") && option_is_default("contactPort")
        && option_is_default("contactTransport") && option_is_default("gameTransport"))
        options.listen = xp_strdup("udp:15345,tcp:15346,websocket:15347");
    bool result = __real_Contact_configure();
    if (result && option_is_default("webRoot")) {
        for (size_t i = 0; i < Contact_endpoint_count(); i++) {
            if (Contact_endpoint(i)->contact_transport == GAME_TRANSPORT_WEBSOCKET) {
                options.webRoot = browser_root();
                break;
            }
        }
    }
    return result;
}

extern int __real_sock_close(sock_t *socket);

EM_JS(void, defer_socket_close, (sock_t *socket), {
    Module['deferSocketClose'](socket);
});

/** Transfer UDP ownership until Node has dispatched numeric-address sends.
 * dgram.send resolves its address on the next tick even for an IPv4 literal;
 * closing in the same C call discards the queued shutdown/meta datagram. */
int __wrap_sock_close(sock_t *socket)
{
    if (!(socket->flags & SOCK_FLAG_UDP) || socket->fd == SOCK_FD_INVALID)
        return __real_sock_close(socket);
    sock_t *owned = malloc(sizeof(*owned));
    if (!owned) return __real_sock_close(socket);
    *owned = *socket;
    sock_init(socket);
    defer_socket_close(owned);
    return SOCK_IS_OK;
}

/** Release the transferred socket after the host's next-tick send work. */
void xp_server_close_socket(sock_t *owned)
{
    __real_sock_close(owned);
    free(owned);
}

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
    if (status == 0) status = code;
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
    status = Server_prepare(argc, argv);
    stopped = status != 0;
    prepared = status == 0;
    inside_host_call = 0;
    return status;
}

/** Borrow a name to resolve before activation: server host then metaservers. */
const char *xp_server_resolve_name(int index)
{
    if (!prepared || stopped) return NULL;
    return index == 0 ? options.serverHost : Meta_host_name((size_t)(index - 1));
}

/** Open listeners after the host has resolved configured names. */
int xp_server_activate(void)
{
    if (!prepared || stopped) return status;
    inside_host_call = 1;
    if (setjmp(host_call)) {
        inside_host_call = 0;
        return status;
    }
    status = Server_activate();
    if (status != 0) End_game();
    inside_host_call = 0;
    return status;
}

EM_JS(int, copy_host_value, (char *output, size_t size, int owner), {
    stringToUTF8(Module[owner ? 'userName' : 'hostName'], output, size);
    return 0;
});

/** Supply the OS owner without Emscripten's synthetic passwd entry. */
void __wrap_Get_login_name(char *output, size_t size)
{
    copy_host_value(output, size, 1);
}

/** Supply the host identity without synchronous DNS or process signals. */
void __wrap_sock_get_local_hostname(char *output, unsigned size, int search)
{
    (void)search;
    copy_host_value(output, size, 0);
}

EM_JS(int, copy_host_address, (const char *name, char *output, size_t size), {
    const address = Module['lookupAddress'](UTF8ToString(name));
    if (!address) return 0;
    stringToUTF8(address, output, size);
    return 1;
});

/** Return a borrowed resolved IPv4 address using the host's completed lookup. */
char *__wrap_sock_get_addr_by_name(const char *name)
{
    static char address[INET_ADDRSTRLEN];
    return copy_host_address(name, address, sizeof(address)) ? address : NULL;
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
