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

#ifndef SOCKLIB_H
#define SOCKLIB_H

#define SOCK_HOSTNAME_LENGTH	256
#ifdef _WINDOWS
/** Native socket handle used by Winsock. */
typedef SOCKET socket_handle_t;
#define SOCK_FD_INVALID		INVALID_SOCKET
#else
/** Native socket descriptor used by POSIX socket APIs. */
typedef int socket_handle_t;
#define SOCK_FD_INVALID		(-1)
#endif
#define SOCK_IS_ERROR		(-1)
#define SOCK_IS_OK		(0)
#define SOCK_TIMEOUT_SECONDS	3

enum sock_flags_e {
    SOCK_FLAG_INIT	= 1,
    SOCK_FLAG_UDP	= 2,
    SOCK_FLAG_TCP	= 4,
    SOCK_FLAG_CONNECT	= 8
};

typedef enum sock_call_e {
    SOCK_CALL_ANY,
    SOCK_CALL_CLOSE,
    SOCK_CALL_SOCKET,
    SOCK_CALL_FCNTL,
    SOCK_CALL_IO,
    SOCK_CALL_GETHOSTBYNAME,
    SOCK_CALL_CONNECT,
    SOCK_CALL_BIND,
    SOCK_CALL_GETSOCKNAME,
    SOCK_CALL_GETSOCKOPT,
    SOCK_CALL_SETSOCKOPT,
    SOCK_CALL_SELECT,
    SOCK_CALL_LISTEN,
    SOCK_CALL_ACCEPT
} sock_call_t;

typedef struct sock_timeout_s {
    long		seconds;
    unsigned long	useconds;
} sock_timeout_t;

typedef struct sock_error_s {
    int			error;
    int			call;
    int			line;
} sock_error_t;

typedef struct sock_s {
    socket_handle_t	fd;
    sock_timeout_t	timeout;
    unsigned		flags;
    sock_error_t	error;
    void		*lastaddr;
    char		*hostname;
} sock_t;

#if !defined(select) && defined(__linux__)
#define select(N, R, W, E, T)	select((N),		\
	(fd_set*)(R), (fd_set*)(W), (fd_set*)(E), (T))
#endif

int sock_startup(void);
void sock_cleanup(void);
int sock_init(sock_t *sock);
int sock_close(sock_t *sock);
int sock_set_non_blocking(sock_t *sock, int flag);
int sock_open_tcp(sock_t * sock);
/**
 * Open and bind a TCP stream socket.
 *
 * @param sock Socket object to initialize.
 * @param dotaddr Local dotted-decimal address, or NULL for all interfaces.
 * @param port Local port, or zero to select an ephemeral port.
 * @return SOCK_IS_OK on success or SOCK_IS_ERROR on failure.
 */
int sock_open_tcp_bound(sock_t *sock, char *dotaddr, int port);
/**
 * Open a bound TCP stream listener.
 *
 * @param sock Socket object to initialize.
 * @param dotaddr Local dotted-decimal address, or NULL for all interfaces.
 * @param port Local port, or zero to select an ephemeral port.
 * @param backlog Maximum number of pending connections accepted by the OS.
 * @return SOCK_IS_OK on success or SOCK_IS_ERROR on failure.
 */
int sock_open_tcp_listener(sock_t *sock, char *dotaddr, int port, int backlog);
/**
 * Accept one TCP stream connection.
 *
 * @param listener Listening socket.
 * @param accepted Socket object initialized with the accepted connection.
 * @return SOCK_IS_OK on success or SOCK_IS_ERROR on failure.
 */
int sock_accept(sock_t *listener, sock_t *accepted);
int sock_open_tcp_connected_non_blocking(sock_t *sock, char *host, int port);
/**
 * Connect an open TCP socket without exceeding a fixed deadline.
 *
 * The socket remains non-blocking after a successful connection.
 *
 * @param sock Open, optionally bound TCP socket.
 * @param host Numeric IPv4 address or hostname.
 * @param port Remote TCP port.
 * @param timeout_seconds Positive connection timeout in seconds.
 * @return SOCK_IS_OK on success or SOCK_IS_ERROR on failure.
 */
int sock_connect_with_timeout(sock_t *sock, char *host, int port,
                              int timeout_seconds);
int sock_open_udp(sock_t *sock, char *dotaddr, int port);
int sock_connect(sock_t *sock, char *host, int port);
int sock_get_last_port(sock_t *sock);
char * sock_get_last_addr(sock_t *sock);
char * sock_get_last_name(sock_t *sock);
int sock_read(sock_t *sock, char *buf, int len);
/**
 * Inspect queued stream bytes without consuming them.
 * @param sock Connected socket; retains bytes and records native I/O errors.
 * @param buf Output buffer borrowed during the call.
 * @param len Buffer capacity in bytes.
 * @return Bytes inspected, zero for EOF, or SOCK_IS_ERROR (possibly temporary).
 */
int sock_peek(sock_t *sock, char *buf, int len);
/**
 * Finish stream output after all previously accepted writes, retaining input.
 * @param sock Owned connection, left open for bounded draining and sock_close.
 * @return SOCK_IS_OK or SOCK_IS_ERROR with the native error recorded.
 */
int sock_shutdown_write(sock_t *sock);
int sock_receive_any(sock_t *sock, char *buf, int len);
int sock_send_dest(sock_t *sock, char *host, int port, char *buf, int len);
int sock_write(sock_t *sock, char *buf, int len);
char *sock_get_addr_by_name(const char *name);
unsigned long sock_get_inet_by_addr(char *dotaddr);
void sock_get_local_hostname(char *name, unsigned size,
			     int search_domain_for_xpilot);
int sock_get_port(sock_t *sock);
int sock_get_error(sock_t *sock);
/**
 * Report whether the last socket failure permits retrying the operation.
 *
 * @param sock Socket containing the last native error code.
 * @return Nonzero for interrupted, in-progress, or would-block failures.
 */
int sock_error_is_temporary(const sock_t *sock);
int sock_set_broadcast(sock_t *sock, int flag);
int sock_set_receive_buffer_size(sock_t *sock, int size);
int sock_set_send_buffer_size(sock_t *sock, int size);
/**
 * Enable or disable immediate transmission of small TCP writes.
 *
 * @param sock Connected TCP socket.
 * @param flag Nonzero to enable TCP_NODELAY, zero to disable it.
 * @return SOCK_IS_OK on success or SOCK_IS_ERROR on failure.
 */
int sock_set_tcp_nodelay(sock_t *sock, int flag);
int sock_set_timeout(sock_t *sock, int seconds, int useconds);
int sock_readable(sock_t *sock);

#endif
