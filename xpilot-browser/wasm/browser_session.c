#include "xpcommon.h"
#include "browser_session.h"
#include "client_session.h"
#include <emscripten.h>

static record_session_t *session;
static client_session_t *admission;
static char confirmation[CLIENT_RECV_SIZE];
static size_t confirmation_length;
static unsigned selected_version;
static int state;
static double deadline;
static char failure[MSG_LEN];

/* Operations access the current heap view after any possible memory growth.
 * A transport captures its JS socket, so replacement cannot close a new peer. */
EM_JS(int, socket_capture, (void), {
    if (!Module.xpilotTransports) Module.xpilotTransports = new Map();
    Module.xpilotNextTransport = (Module.xpilotNextTransport || 0) + 1;
    Module.xpilotTransports.set(Module.xpilotNextTransport, Module.xpilotSocket);
    return Module.xpilotNextTransport;
});
EM_JS(int, socket_read, (int id, char *data, int capacity), {
    const socket = Module.xpilotTransports.get(id);
    const record = socket.receive();
    if (record === -1) return -1;
    if (record === undefined) return 0;
    if (record.length > capacity) { socket.dispose(); return -1; }
    HEAPU8.set(record, data);
    return record.length;
});
EM_JS(int, socket_send, (int id, const char *data, int length, int transient), {
    return Module.xpilotTransports.get(id).send(HEAPU8.subarray(data, data + length), !!transient);
});
EM_JS(int, socket_flush, (int id), { return Module.xpilotTransports.get(id).flush(); });
EM_JS(void, socket_close, (int id), {
    const socket = Module.xpilotTransports.get(id);
    if (socket) socket.dispose();
});
EM_JS(void, socket_destroy, (int id), { Module.xpilotTransports.delete(id); });

static record_receive_result_t receive_record(void *context, char *data,
                                               size_t capacity, size_t *length)
{
    int count = socket_read((int)(intptr_t)context, data, (int)capacity);
    *length = count > 0 ? (size_t)count : 0;
    return count < 0 ? RECORD_RECEIVE_CLOSED : count == 0
        ? RECORD_RECEIVE_EMPTY : RECORD_RECEIVE_READY;
}
static record_send_result_t send_record(void *context, const char *data,
                                        size_t length, record_delivery_t delivery)
{
    return socket_send((int)(intptr_t)context, data, (int)length,
                       delivery == RECORD_DELIVERY_TRANSIENT);
}
static record_flush_result_t flush_record(void *context)
{
    return socket_flush((int)(intptr_t)context);
}
static void close_record(void *context) { socket_close((int)(intptr_t)context); }
static void destroy_record(void *context) { socket_destroy((int)(intptr_t)context); }
static socket_handle_t native_handle(void *context) { (void)context; return SOCK_FD_INVALID; }
record_transport_t *Browser_transport_create(void)
{
    const record_transport_operations_t operations = {
        receive_record, send_record, flush_record, close_record,
        destroy_record, native_handle
    };
    int id = socket_capture();
    record_transport_t *transport = Record_transport_create(&operations, (void *)(intptr_t)id);
    if (transport == NULL) { socket_close(id); socket_destroy(id); }
    return transport;
}
void xp_browser_session_close(void)
{
    Client_session_destroy(admission);
    admission = NULL;
    Record_session_destroy(session);
    session = NULL;
    state = 0;
}
int xp_browser_session_begin(const char *nickname)
{
    xp_browser_session_close();
    failure[0] = '\0';
    session_game_request_t request = {0};
    request.polygon_version = Game_transport_session_protocol_version(GAME_TRANSPORT_WEBSOCKET, true);
    request.legacy_version = Game_transport_session_protocol_version(GAME_TRANSPORT_WEBSOCKET, false);
    request.team = TEAM_NOT_SET;
    strlcpy(request.nick, nickname, sizeof(request.nick));
    strlcpy(request.user, "browser", sizeof(request.user));
    strlcpy(request.host, "browser", sizeof(request.host));
    strlcpy(request.display, "WebGL", sizeof(request.display));
    session = Record_session_create(Browser_transport_create());
    admission = Client_session_create(session, &request);
    if (admission == NULL) {
        strlcpy(failure, "Cannot create session", sizeof(failure));
        state = -1;
    }
    deadline = emscripten_get_now() + 15000;
    return state;
}
int xp_browser_session_step(void)
{
    if (state != 0) return state;
    if (emscripten_get_now() >= deadline) {
        strlcpy(failure, "Connection timed out", sizeof(failure));
        return state = -1;
    }
    state = Client_session_step(admission, confirmation, sizeof(confirmation),
                                &confirmation_length);
    if (state < 0) strlcpy(failure, Client_session_error(admission), sizeof(failure));
    if (state > 0) selected_version = Client_session_selected_version(admission);
    return state;
}
const char *xp_browser_session_error(void) { return failure; }
record_session_t *Browser_session_take(const char **data, size_t *length, unsigned *version)
{
    if (state != 1) return NULL;
    record_session_t *result = session;
    session = NULL;
    *data = confirmation;
    *length = confirmation_length;
    *version = selected_version;
    Client_session_destroy(admission);
    admission = NULL;
    return result;
}
