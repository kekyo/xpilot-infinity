/* XPilot Infinity -- GPL-2.0-or-later. */
#ifndef WEB_FRONT_H
#define WEB_FRONT_H

#include "socklib.h"
#include "server_endpoint.h"

/** Bounded HTTP entrance shared by WebSocket listeners; owns pending sockets. */
typedef struct web_front web_front_t;
/**
 * Receives an accepted Upgrade without consuming its HTTP header.
 * @param socket Owned socket, transferred to the callback.
 * @param address Validated numeric client IP, borrowed during the callback.
 * @param port Observed TCP source port.
 * @param endpoint Listener description, borrowed during the callback.
 */
typedef void (*web_upgrade_fn)(sock_t *socket, const char *address, int port,
                               const server_endpoint_t *endpoint);
/**
 * Create an HTTP entrance. Strings are copied; NULL disables each option.
 * @param root Static directory, or NULL for WebSocket-only operation.
 * @param origin Exact public http(s) origin, or NULL to require http://Host.
 * @param proxy Single numeric trusted proxy IP, or NULL for direct clients.
 * @param upgrade Callback taking ownership of accepted game sockets.
 * @return Owned entrance, or NULL for invalid configuration or allocation failure.
 */
web_front_t *Web_front_create(const char *root, const char *origin,
                             const char *proxy, web_upgrade_fn upgrade);
/** Release every pending socket, file and the entrance itself. */
void Web_front_destroy(web_front_t *front);
/**
 * Take ownership of a nonblocking socket, including on overload rejection.
 * @param front Entrance owning pending requests.
 * @param socket Socket moved out of the caller's object.
 * @param address Physical numeric peer IP.
 * @param port Observed source port.
 * @param endpoint Listener description, copied for eventual Upgrade.
 */
void Web_front_accept(web_front_t *front, sock_t *socket, const char *address,
                      int port, const server_endpoint_t *endpoint);
/** Advance bounded nonblocking work once per server frame; never waits for I/O. */
void Web_front_step(web_front_t *front);
#endif
