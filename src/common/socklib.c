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

#include "xpcommon.h"

/* Debug macro */
#ifdef DEBUG
# define DEB(x) x
#else
# define DEB(x)
#endif

#ifndef timerclear
# define timerclear(tvp)   ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif

#define SOCK_GETHOST_TIMEOUT	6

static jmp_buf		env;


static int sock_native_error(void)
{
#ifdef _WINDOWS
    return WSAGetLastError();
#else
    return errno;
#endif
}

#ifdef _WINDOWS
static int sock_windows_error_to_errno(int error_code)
{
    switch (error_code) {
    case WSAEINTR:
	return EINTR;
    case WSAEWOULDBLOCK:
	return EWOULDBLOCK;
    case WSAEINPROGRESS:
	return EINPROGRESS;
    case WSAEALREADY:
	return EALREADY;
    case WSAEINVAL:
	return EINVAL;
    case WSAECONNRESET:
	return ECONNRESET;
    case WSAECONNREFUSED:
	return ECONNREFUSED;
    case WSAETIMEDOUT:
	return ETIMEDOUT;
    default:
	return EIO;
    }
}
#endif


static struct hostent *sock_get_host_by_name(const char *name);
static struct hostent *sock_get_host_by_addr(const char *addr,
					     int len, int type);


static void sock_flags_add(sock_t *sock, unsigned bits)
{
    sock->flags |= bits;
}

static void sock_flags_set(sock_t *sock, unsigned bits)
{
    sock->flags = bits;
}

static void sock_flags_remove(sock_t *sock, unsigned bits)
{
    sock->flags &= ~bits;
}

static int sock_flags_test_all(sock_t *sock, unsigned bits)
{
    return (sock->flags & bits) == (unsigned)bits;
}

static int sock_flags_test_any(sock_t *sock, unsigned bits)
{
    return (sock->flags & bits) != 0;
}

static int sock_set_error(sock_t *sock, int err, sock_call_t call, int line)
{
    DEB(printf("set error %d, %d, %d.  \"%s\"\n",
	       err, call, line, strerror(err)));
    errno = err;
    sock->error.error = err;
    sock->error.call = call;
    sock->error.line = line;

    return SOCK_IS_ERROR;
}

static int sock_set_native_error(sock_t *sock, sock_call_t call, int line)
{
    int native_error = sock_native_error();

#ifdef _WINDOWS
    errno = sock_windows_error_to_errno(native_error);
    sock->error.error = native_error;
    sock->error.call = call;
    sock->error.line = line;
    DEB(printf("set Winsock error %d, %d, %d\n",
	       native_error, call, line));
    return SOCK_IS_ERROR;
#else
    return sock_set_error(sock, native_error, call, line);
#endif
}

int sock_error_is_temporary(const sock_t *sock)
{
    if (sock == NULL)
	return false;
#ifdef _WINDOWS
    return sock->error.error == WSAEINTR
	|| sock->error.error == WSAEWOULDBLOCK
	|| sock->error.error == WSAEINPROGRESS
	|| sock->error.error == WSAEALREADY;
#else
    return sock->error.error == EINTR
	|| sock->error.error == EWOULDBLOCK
	|| sock->error.error == EAGAIN
	|| sock->error.error == EINPROGRESS
	|| sock->error.error == EALREADY;
#endif
}

static int sock_check(sock_t *sock)
{
    if (!sock_flags_test_all(sock, SOCK_FLAG_INIT))
	return sock_set_error(sock, EINVAL, SOCK_CALL_ANY, __LINE__);

    return SOCK_IS_OK;
}

static int sock_alloc_hostname(sock_t *sock)
{
    if (!sock->hostname) {
	sock->hostname = (char *) malloc(SOCK_HOSTNAME_LENGTH);
	if (!sock->hostname)
	    sock_set_error(sock, errno, SOCK_CALL_ANY, __LINE__);
	else
	    sock->hostname[0] = '\0';
    }

    return (sock->hostname) ? SOCK_IS_OK : SOCK_IS_ERROR;
}

static void sock_free_hostname(sock_t *sock)
{
    if (sock->hostname) {
	free(sock->hostname);
	sock->hostname = NULL;
    }
}

static int sock_alloc_lastaddr(sock_t *sock)
{
    if (!sock->lastaddr) {
	sock->lastaddr = (void *) calloc(1, sizeof(struct sockaddr_in));
	if (!sock->lastaddr)
	    sock_set_error(sock, errno, SOCK_CALL_ANY, __LINE__);
    }

    return (sock->lastaddr) ? SOCK_IS_OK : SOCK_IS_ERROR;
}

static void sock_free_lastaddr(sock_t *sock)
{
    if (sock->lastaddr) {
	free(sock->lastaddr);
	sock->lastaddr = NULL;
    }
}

int sock_startup()
{
#ifdef _WINDOWS
    WORD requested_version = MAKEWORD(2, 2);
    WSADATA wsa_data;
    int status;

    status = WSAStartup(requested_version, &wsa_data);
    if (status != 0)
	return SOCK_IS_ERROR;
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
	WSACleanup();
	return SOCK_IS_ERROR;
    }
#endif
    return SOCK_IS_OK; /* socket initialization only needed for windows */
}

void sock_cleanup(void)
{
#ifdef _WINDOWS
	WSACleanup();
#endif
}

int sock_init(sock_t *sock)
{
    memset(sock, 0, sizeof(*sock));

    sock_flags_set(sock, SOCK_FLAG_INIT);
    sock->fd = SOCK_FD_INVALID;
    sock->hostname = (char *) NULL;
    sock->lastaddr = (void *) NULL;
    sock->timeout.seconds = SOCK_TIMEOUT_SECONDS;

    return sock_check(sock);
}

static int sock_close_native(sock_t *sock)
{
#ifdef _WINDOWS
    if (closesocket(sock->fd) == SOCKET_ERROR)
	return sock_set_native_error(sock, SOCK_CALL_CLOSE, __LINE__);
#else
    if (close(sock->fd) < 0)
	return sock_set_native_error(sock, SOCK_CALL_CLOSE, __LINE__);
#endif
    return SOCK_IS_OK;
}

static int sock_close_tcp(sock_t *sock)
{
    int status = sock_close_native(sock);

    sock_flags_remove(sock, SOCK_FLAG_TCP);
    sock->fd = SOCK_FD_INVALID;

    return status;
}

static int sock_close_udp(sock_t *sock)
{
    int status = sock_close_native(sock);

    sock_flags_remove(sock, SOCK_FLAG_UDP);
    sock->fd = SOCK_FD_INVALID;

    return status;
}

int sock_close(sock_t *sock)
{
    sock_free_hostname(sock);
    sock_free_lastaddr(sock);
    if (sock_flags_test_any(sock, SOCK_FLAG_UDP))
	return sock_close_udp(sock);
    if (sock_flags_test_any(sock, SOCK_FLAG_TCP))
	return sock_close_tcp(sock);
    return sock_set_error(sock, EINVAL, SOCK_CALL_ANY, __LINE__);
}

int sock_open_tcp(sock_t *sock)
{
    if (sock_init(sock))
	return SOCK_IS_ERROR;

    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == SOCK_FD_INVALID)
	return sock_set_native_error(sock, SOCK_CALL_SOCKET, __LINE__);

    sock_flags_add(sock, SOCK_FLAG_TCP);

    return SOCK_IS_OK;
}

static int sock_bind_tcp(sock_t *sock, char *dotaddr, int port)
{
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = dotaddr != NULL ? inet_addr(dotaddr) : INADDR_ANY;
    if (bind(sock->fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_IS_ERROR)
	return sock_set_native_error(sock, SOCK_CALL_BIND, __LINE__);
    return SOCK_IS_OK;
}

int sock_open_tcp_bound(sock_t *sock, char *dotaddr, int port)
{
    if (sock_open_tcp(sock) == SOCK_IS_ERROR)
	return SOCK_IS_ERROR;
    if (sock_bind_tcp(sock, dotaddr, port) == SOCK_IS_ERROR) {
	sock_close(sock);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_open_tcp_listener(sock_t *sock, char *dotaddr, int port, int backlog)
{
    int flag = 1;

    if (backlog <= 0) {
	sock_init(sock);
	errno = EINVAL;
	return sock_set_error(sock, errno, SOCK_CALL_LISTEN, __LINE__);
    }

    if (sock_open_tcp(sock) == SOCK_IS_ERROR)
	return SOCK_IS_ERROR;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR,
		   (void *)&flag, sizeof(flag)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SETSOCKOPT, __LINE__);
	sock_close(sock);
	return SOCK_IS_ERROR;
    }
    if (sock_bind_tcp(sock, dotaddr, port) == SOCK_IS_ERROR) {
	sock_close(sock);
	return SOCK_IS_ERROR;
    }
    if (listen(sock->fd, backlog) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_LISTEN, __LINE__);
	sock_close(sock);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_accept(sock_t *listener, sock_t *accepted)
{
    socklen_t addrlen;

    if (sock_init(accepted) == SOCK_IS_ERROR)
	return SOCK_IS_ERROR;
    if (sock_alloc_lastaddr(accepted) == SOCK_IS_ERROR)
	return SOCK_IS_ERROR;

    addrlen = sizeof(struct sockaddr_in);
    accepted->fd = accept(listener->fd,
			  (struct sockaddr *)accepted->lastaddr, &addrlen);
    if (accepted->fd == SOCK_FD_INVALID) {
	sock_set_native_error(accepted, SOCK_CALL_ACCEPT, __LINE__);
	sock_free_lastaddr(accepted);
	return SOCK_IS_ERROR;
    }
    sock_flags_add(accepted, SOCK_FLAG_TCP | SOCK_FLAG_CONNECT);
    return SOCK_IS_OK;
}

int sock_set_non_blocking(sock_t *sock, int flag)
{
#ifdef _WINDOWS
    u_long mode = flag != 0 ? 1UL : 0UL;

    if (ioctlsocket(sock->fd, FIONBIO, &mode) == 0)
	return SOCK_IS_OK;
    return sock_set_native_error(sock, SOCK_CALL_FCNTL, __LINE__);
#else
/*
 * There are some problems on some particular systems (suns) with
 * getting sockets to be non-blocking.  Just try all possible ways
 * until one of them succeeds.  Please keep us informed by e-mail
 * to xpilot@xpilot.org.
 */

#ifndef USE_FCNTL_O_NONBLOCK
# ifndef USE_FCNTL_O_NDELAY
#  ifndef USE_FCNTL_FNDELAY
#   ifndef USE_IOCTL_FIONBIO

#    if defined(_SEQUENT_) || defined(__svr4__) || defined(SVR4)
#     define USE_FCNTL_O_NDELAY
#    elif defined(__sun__) && defined(FNDELAY)
#     define USE_FCNTL_FNDELAY
#    elif defined(FIONBIO)
#     define USE_IOCTL_FIONBIO
#    elif defined(FNDELAY)
#     define USE_FCNTL_FNDELAY
#    elif defined(O_NONBLOCK)
#     define USE_FCNTL_O_NONBLOCK
#    else
#     define USE_FCNTL_O_NDELAY
#    endif

#    if 0
#     if defined(FNDELAY) && defined(F_SETFL)
#      define USE_FCNTL_FNDELAY
#     endif
#     if defined(O_NONBLOCK) && defined(F_SETFL)
#      define USE_FCNTL_O_NONBLOCK
#     endif
#     if defined(FIONBIO)
#      define USE_IOCTL_FIONBIO
#     endif
#     if defined(O_NDELAY) && defined(F_SETFL)
#      define USE_FCNTL_O_NDELAY
#     endif
#    endif

#   endif
#  endif
# endif
#endif

    char buf[128];

#ifdef USE_FCNTL_FNDELAY
    if (fcntl(sock->fd, F_SETFL, (flag != 0) ? FNDELAY : 0) != -1)
	return SOCK_IS_OK;
    sock_set_native_error(sock, SOCK_CALL_FCNTL, __LINE__);
    sprintf(buf, "fcntl FNDELAY failed in socklib.c line %d", __LINE__);
    perror(buf);
#endif

#ifdef USE_IOCTL_FIONBIO
    if (ioctl(sock->fd, FIONBIO, &flag) == 0)
	return SOCK_IS_OK;
    sock_set_native_error(sock, SOCK_CALL_FCNTL, __LINE__);
    sprintf(buf, "ioctl FIONBIO failed in socklib.c line %d", __LINE__);
    perror(buf);
#endif

#ifdef USE_FCNTL_O_NONBLOCK
    if (fcntl(sock->fd, F_SETFL, (flag != 0) ? O_NONBLOCK : 0) != -1)
	return SOCK_IS_OK;
    sock_set_native_error(sock, SOCK_CALL_FCNTL, __LINE__);
    sprintf(buf, "fcntl O_NONBLOCK failed in socklib.c line %d", __LINE__);
    perror(buf);
#endif

#ifdef USE_FCNTL_O_NDELAY
    if (fcntl(sock->fd, F_SETFL, (flag != 0) ? O_NDELAY : 0) != -1)
	return SOCK_IS_OK;
    sock_set_native_error(sock, SOCK_CALL_FCNTL, __LINE__);
    sprintf(buf, "fcntl O_NDELAY failed in socklib.c line %d", __LINE__);
    perror(buf);
#endif

    return SOCK_IS_ERROR;
#endif
}

int sock_open_tcp_connected_non_blocking(sock_t *sock, char *host, int port)
{
    struct sockaddr_in	dest;
    struct hostent	*hp;

    if (sock_open_tcp(sock))
	return SOCK_IS_ERROR;

    /*
     * On error a message will have been printed
     * and we want to continue regardless.
     */
    sock_set_non_blocking(sock, 1);

    memset(&dest, 0, sizeof(dest));
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons((unsigned short)port);
    dest.sin_addr.s_addr = inet_addr(host);
    if ((dest.sin_addr.s_addr & 0xFFFFFFFF) == 0xFFFFFFFF) {
	/*
	 * Cannot use h_errno because of portability problems.
	 * Let's hope errno is meaningful too.
	 */
	errno = 0;
	if ((hp = sock_get_host_by_name(host)) == NULL) {
	    sock_set_native_error(sock, SOCK_CALL_GETHOSTBYNAME, __LINE__);
	    sock_close(sock);
	    return SOCK_IS_ERROR;
	}

	dest.sin_addr.s_addr
	    = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    }

    if (connect(sock->fd, (struct sockaddr *)&dest,
		sizeof(struct sockaddr_in)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_CONNECT, __LINE__);
	if (!sock_error_is_temporary(sock)) {
	    sock_close(sock);
	    return SOCK_IS_ERROR;
	}
    }
    sock_flags_add(sock, SOCK_FLAG_CONNECT);

    return SOCK_IS_OK;
}

static int sock_set_connect_result_error(sock_t *sock, int error_code,
                                         int line)
{
#ifdef _WINDOWS
    errno = sock_windows_error_to_errno(error_code);
    sock->error.error = error_code;
    sock->error.call = SOCK_CALL_CONNECT;
    sock->error.line = line;
    return SOCK_IS_ERROR;
#else
    return sock_set_error(sock, error_code, SOCK_CALL_CONNECT, line);
#endif
}

int sock_connect_with_timeout(sock_t *sock, char *host, int port,
                              int timeout_seconds)
{
    struct sockaddr_in destination;
    struct hostent *host_entry;
    struct timeval timeout;
    fd_set write_fds;
    fd_set error_fds;
    int socket_error = 0;
    socklen_t error_size = sizeof(socket_error);
    int status;

    if (sock == NULL || sock->fd == SOCK_FD_INVALID || host == NULL
        || port <= 0 || port > 65535 || timeout_seconds <= 0) {
        if (sock != NULL)
            return sock_set_error(sock, EINVAL, SOCK_CALL_CONNECT, __LINE__);
        errno = EINVAL;
        return SOCK_IS_ERROR;
    }
    if (sock_set_non_blocking(sock, 1) == SOCK_IS_ERROR)
        return SOCK_IS_ERROR;

    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_port = htons((unsigned short)port);
    destination.sin_addr.s_addr = inet_addr(host);
    if ((destination.sin_addr.s_addr & 0xFFFFFFFF) == 0xFFFFFFFF) {
        errno = 0;
        host_entry = sock_get_host_by_name(host);
        if (host_entry == NULL)
            return sock_set_error(sock, errno != 0 ? errno : EHOSTUNREACH,
                                  SOCK_CALL_GETHOSTBYNAME, __LINE__);
        destination.sin_addr.s_addr =
            ((struct in_addr *)host_entry->h_addr_list[0])->s_addr;
    }

    status = connect(sock->fd, (struct sockaddr *)&destination,
                     sizeof(destination));
    if (status == SOCK_IS_ERROR) {
        sock_set_native_error(sock, SOCK_CALL_CONNECT, __LINE__);
        if (!sock_error_is_temporary(sock))
            return SOCK_IS_ERROR;

        FD_ZERO(&write_fds);
        FD_ZERO(&error_fds);
        FD_SET(sock->fd, &write_fds);
        FD_SET(sock->fd, &error_fds);
        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;
#ifdef _WINDOWS
        status = select(0, NULL, &write_fds, &error_fds, &timeout);
#else
        status = select(sock->fd + 1, NULL, &write_fds, &error_fds,
                        &timeout);
#endif
        if (status == 0)
            return sock_set_error(sock, ETIMEDOUT, SOCK_CALL_CONNECT,
                                  __LINE__);
        if (status == SOCK_IS_ERROR)
            return sock_set_native_error(sock, SOCK_CALL_SELECT, __LINE__);

        if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR,
#ifdef _WINDOWS
                       (char *)&socket_error,
#else
                       (void *)&socket_error,
#endif
                       &error_size) == SOCK_IS_ERROR)
            return sock_set_native_error(sock, SOCK_CALL_GETSOCKOPT,
                                         __LINE__);
        if (socket_error != 0)
            return sock_set_connect_result_error(sock, socket_error,
                                                 __LINE__);
    }

    if (sock_alloc_lastaddr(sock) == SOCK_IS_ERROR)
        return SOCK_IS_ERROR;
    memcpy(sock->lastaddr, &destination, sizeof(destination));
    sock_flags_add(sock, SOCK_FLAG_CONNECT);
    return SOCK_IS_OK;
}

int sock_open_udp(sock_t *sock, char *dotaddr, int port)
{
    struct sockaddr_in	addr;

    if (sock_init(sock))
	return SOCK_IS_ERROR;

    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->fd == SOCK_FD_INVALID)
	return sock_set_native_error(sock, SOCK_CALL_SOCKET, __LINE__);

    sock_flags_add(sock, SOCK_FLAG_UDP);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family	 = AF_INET;
    addr.sin_port	 = htons((unsigned short)port);
    addr.sin_addr.s_addr = (dotaddr) ? inet_addr(dotaddr) : INADDR_ANY;
    if (bind(sock->fd, (struct sockaddr *)&addr, sizeof(addr))
	== SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_BIND, __LINE__);
	sock_close(sock);
	return SOCK_IS_ERROR;
    }

    return SOCK_IS_OK;
}

int sock_connect(sock_t *sock, char *host, int port)
{
    struct sockaddr_in		dest;
    struct hostent		*hp;

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons((unsigned short) port);
    dest.sin_addr.s_addr = inet_addr(host);
    if ((dest.sin_addr.s_addr & 0xFFFFFFFF) == 0xFFFFFFFF) {
	errno = 0;
	if ((hp = sock_get_host_by_name(host)) == NULL) {
	    sock_set_error(sock, errno, SOCK_CALL_GETHOSTBYNAME, __LINE__);
	    return SOCK_IS_ERROR;
	}

	dest.sin_addr.s_addr
	    = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    }

    if (connect(sock->fd, (struct sockaddr *)&dest, sizeof(dest))
	== SOCK_IS_ERROR)
	return sock_set_native_error(sock, SOCK_CALL_CONNECT, __LINE__);

    sock_flags_add(sock, SOCK_FLAG_CONNECT);

    return SOCK_IS_OK;
}

int sock_get_last_port(sock_t *sock)
{
    struct sockaddr_in	*lastaddr;

    if (sock->lastaddr) {
	lastaddr = (struct sockaddr_in *)(sock->lastaddr);
	return ntohs(lastaddr->sin_port);
    }

    sock_set_error(sock, EINVAL, SOCK_CALL_ANY, __LINE__);

    return 0;
}

char * sock_get_last_addr(sock_t *sock)
{
    static char		error_addr[] = "255.255.255.255";
    char		*str;
    struct sockaddr_in	*lastaddr;

    if (sock->lastaddr) {
	lastaddr = (struct sockaddr_in *)(sock->lastaddr);
	str = inet_ntoa(lastaddr->sin_addr);
	if (sock_alloc_hostname(sock))
	    return str;
	strlcpy(sock->hostname, str, SOCK_HOSTNAME_LENGTH);
	return sock->hostname;
    }

    sock_set_error(sock, EINVAL, SOCK_CALL_ANY, __LINE__);

    return error_addr;
}

char * sock_get_last_name(sock_t *sock)
{
    static char		error_addr[] = "255.255.255.255";
    char		*str;
    struct hostent	*hp;
    struct sockaddr_in	*lastaddr;

    if (sock->lastaddr) {
	lastaddr = (struct sockaddr_in *)(sock->lastaddr);
	hp = sock_get_host_by_addr((char *)&(lastaddr->sin_addr),
				   sizeof(lastaddr->sin_addr), AF_INET);
	if (hp == NULL)
	    str = inet_ntoa(lastaddr->sin_addr);
	else
	    str = hp->h_name;

	if (sock_alloc_hostname(sock))
	    return str;

	strlcpy(sock->hostname, str, SOCK_HOSTNAME_LENGTH);
	return sock->hostname;
    }

    sock_set_error(sock, EINVAL, SOCK_CALL_ANY, __LINE__);

    return error_addr;
}

int sock_read(sock_t *sock, char *buf, int len)
{
    int			count;

    count = recv(sock->fd, buf, len, 0);
    if (count == SOCK_IS_ERROR)
	sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);

    return count;
}

int sock_peek(sock_t *sock, char *buf, int len)
{
    int count = recv(sock->fd, buf, len, MSG_PEEK);
    if (count == SOCK_IS_ERROR)
        sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);
    return count;
}

int sock_shutdown_write(sock_t *sock)
{
#ifdef _WINDOWS
    int result = shutdown(sock->fd, SD_SEND);
#else
    int result = shutdown(sock->fd, SHUT_WR);
#endif
    if (result == SOCK_IS_ERROR)
        sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);
    return result;
}

int sock_receive_any(sock_t *sock, char *buf, int len)
{
    int			count;
    socklen_t		addrlen;

    if (sock_alloc_lastaddr(sock) == SOCK_IS_ERROR)
	return SOCK_IS_ERROR;
    addrlen = sizeof(struct sockaddr_in);
    count = recvfrom(sock->fd, buf, len, 0,
		     (struct sockaddr *)(sock->lastaddr), &addrlen);
    if (count == SOCK_IS_ERROR)
	sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);

    return count;
}

int sock_send_dest(sock_t *sock, char *host, int port, char *buf, int len)
{
    struct sockaddr_in		dest;
    struct hostent		*hp;
    int				count;

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons((unsigned short) port);
    dest.sin_addr.s_addr = inet_addr(host);
    if ((dest.sin_addr.s_addr & 0xFFFFFFFF) == 0xFFFFFFFF) {
	errno = 0;
	if ((hp = sock_get_host_by_name(host)) == NULL)
	    return sock_set_error(sock, errno, SOCK_CALL_GETHOSTBYNAME,
				  __LINE__);

	dest.sin_addr.s_addr
	    = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    }

    count = sendto(sock->fd, buf, len, 0,
		   (struct sockaddr *) &dest, sizeof(dest));
    if (count == SOCK_IS_ERROR)
	sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);

    return count;
}

int sock_write(sock_t *sock, char *buf, int len)
{
    int			count;

    count = send(sock->fd, buf, len, 0);
    if (count == SOCK_IS_ERROR)
	sock_set_native_error(sock, SOCK_CALL_IO, __LINE__);

    return count;
}

char *sock_get_addr_by_name(const char *name)
{
    struct hostent	*hp;

    hp = sock_get_host_by_name(name);

    if (!hp)
	return (char *) NULL;

    return inet_ntoa(*(struct in_addr *)(hp->h_addr_list[0]));
}

unsigned long sock_get_inet_by_addr(char *dotaddr)
{
    return inet_addr(dotaddr);
}

void sock_get_local_hostname(char *name, unsigned size,
			     int search_domain_for_xpilot)
{
    struct hostent	*he = NULL;
    struct hostent 	*xpilot_he = NULL;
#ifndef _WINDOWS
    int			xpilot_len;
    char		*dot;
    char		xpilot_hostname[SOCK_HOSTNAME_LENGTH];
#endif
    static const char	xpilot[] = "xpilot";

    gethostname(name, size);
    if ((he = sock_get_host_by_name(name)) == NULL)
	return;
    strlcpy(name, he->h_name, size);

    /*
     * If there are no dots in the name then we don't have the FQDN,
     * and if the address is of the normal Internet type
     * then we try to get the FQDN via the backdoor of the IP address.
     * Let's hope it works :)
     */
    if (strchr(he->h_name, '.') == NULL
	&& he->h_addrtype == AF_INET) {
	struct in_addr in;
	memcpy((void *)&in, he->h_addr_list[0], sizeof(in));
	if ((he = sock_get_host_by_addr((char *)&in, sizeof(in), AF_INET))
	    != NULL
	    && strchr(he->h_name, '.') != NULL)
	    strlcpy(name, he->h_name, size);
	else {
	    /* Let's try to find the domain from /etc/resolv.conf. */
	    FILE *fp = fopen("/etc/resolv.conf", "r");
	    if (fp) {
		char *s, buf[256];
		while (fgets(buf, sizeof buf, fp)) {
		    if ((s = strtok(buf, " \t\r\n")) != NULL
			&& !strcmp(s, "domain")
			&& (s = strtok(NULL, " \t\r\n")) != NULL) {
			strcat(name, ".");
			strcat(name, s);
			break;
		    }
		}
		fclose(fp);
	    }
	}
	/* make sure this is a valid FQDN. */
	if ((he = sock_get_host_by_name(name)) == NULL) {
	    gethostname(name, size);
	    return;
	}
    }

    if (search_domain_for_xpilot != 1)
	return;

#ifndef _WINDOWS	/* the lookup of xpilot can take FOREVER! zzzz...  */

    /* if name starts with "xpilot" then we're done. */
    xpilot_len = strlen(xpilot);
    if (!strncmp(name, xpilot, xpilot_len))
	return;

    /* Make a wild guess that a "xpilot" hostname or alias is in this domain */
    dot = name;
    while ((dot = strchr(dot, '.')) != NULL) {
	if (xpilot_len + strlen(dot) < sizeof(xpilot_hostname)) {
	    strlcpy(xpilot_hostname, xpilot, SOCK_HOSTNAME_LENGTH);
	    strlcat(xpilot_hostname, dot, SOCK_HOSTNAME_LENGTH);
	    /*
	     * If there is a CNAME the h_name must be identical to the
	     * FQDN we guessed above.  It is hard to know our IP to know
	     * that an A record points to us.
	     */
	    if ((xpilot_he = sock_get_host_by_name(xpilot_hostname)) != NULL &&
		!strcmp(name, xpilot_he->h_name))
		break;
	    xpilot_he = NULL;
	}
	++dot;
    }
    if (xpilot_he != NULL)
	strlcpy(name, xpilot_hostname, size);

#endif
}

int sock_get_port(sock_t *sock)
{
    struct sockaddr_in	addr;
    socklen_t		len = sizeof(addr);
    unsigned short	port;

    if (getsockname(sock->fd, (struct sockaddr *)&addr, &len)
	== SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_GETSOCKNAME, __LINE__);
	return SOCK_IS_ERROR;
    }

    port = ntohs(addr.sin_port);

    return port;
}

int sock_get_error(sock_t *sock)
{
    int			err;
    socklen_t		size = sizeof(err);

    if (getsockopt(sock->fd, SOL_SOCKET, SO_ERROR,
		   (void *)&err, &size) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_GETSOCKOPT, __LINE__);
	return SOCK_IS_ERROR;
    }
#ifdef _WINDOWS
    errno = err == 0 ? 0 : sock_windows_error_to_errno(err);
#else
    errno = err;
#endif
    sock->error.error = err;
    sock->error.call = SOCK_CALL_GETSOCKOPT;
    sock->error.line = __LINE__;
    return SOCK_IS_OK;
}

int sock_set_broadcast(sock_t *sock, int flag)
{
    if (setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST,
		   (void *)&flag, sizeof(flag)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SETSOCKOPT, __LINE__);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_set_receive_buffer_size(sock_t *sock, int size)
{
    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF,
		   (void *)&size, sizeof(size)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SETSOCKOPT, __LINE__);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_set_send_buffer_size(sock_t *sock, int size)
{
    if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDBUF,
		   (void *)&size, sizeof(size)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SETSOCKOPT, __LINE__);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_set_tcp_nodelay(sock_t *sock, int flag)
{
    if (setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY,
		   (void *)&flag, sizeof(flag)) == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SETSOCKOPT, __LINE__);
	return SOCK_IS_ERROR;
    }
    return SOCK_IS_OK;
}

int sock_set_timeout(sock_t *sock, int seconds, int useconds)
{
    sock->timeout.seconds = seconds;
    sock->timeout.useconds = useconds;

    return SOCK_IS_OK;
}

int sock_readable(sock_t *sock)
{
    int			n;
    fd_set		readfds;
    struct timeval	timeout;

    timerclear(&timeout); /* macro */
    timeout.tv_sec = sock->timeout.seconds;
    timeout.tv_usec = sock->timeout.useconds;

    FD_ZERO(&readfds);
    FD_SET(sock->fd, &readfds);

#ifdef _WINDOWS
    n = select(0, &readfds, NULL, NULL, &timeout);
#else
    n = select(sock->fd + 1, &readfds, NULL, NULL, &timeout);
#endif
    if (n == SOCK_IS_ERROR) {
	sock_set_native_error(sock, SOCK_CALL_SELECT, __LINE__);
	if (!sock_error_is_temporary(sock))
	    return SOCK_IS_ERROR;
	return SOCK_IS_OK;
    }

    if ((n > 0) && FD_ISSET(sock->fd, &readfds))
	return 1;

    return SOCK_IS_OK;
}

static void sock_catch_alarm(int signum)
{
    UNUSED_PARAM(signum);
    printf("DNS lookup cancelled\n");

    longjmp(env, 1);
}

static struct hostent *sock_get_host_by_name(const char *name)
{
#ifndef _WINDOWS

    struct hostent	*hp;

    if (setjmp(env)) {
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	return (struct hostent *) NULL;
    }

    signal(SIGALRM, sock_catch_alarm);
    alarm(SOCK_GETHOST_TIMEOUT);

    hp = gethostbyname(name);

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    return hp;

#else
    
    /*
     * If you aren't connected to the net, then gethostbyname()
     * can take many minutes to time out.  WSACancelBlockingCall()
     * doesn't affect it.
     *
    
    static char     chp[MAXGETHOSTSTRUCT+1];
    struct hostent* hp = (struct hostent*)&chp;
    HANDLE h;
    MSG msg;
    int i;
    
    h = WSAAsyncGetHostByName(notifyWnd, WM_GETHOSTNAME, name, 
        chp, MAXGETHOSTSTRUCT);
    
    for(i = 0; i < SOCK_GETHOST_TIMEOUT; i++) {
        if (PeekMessage(&msg, NULL, WM_GETHOSTNAME,
			WM_GETHOSTNAME, PM_REMOVE))
            return (WSAGETASYNCERROR(msg.lParam)) ? NULL : hp;
        Sleep(1000);
    }
    WSACancelAsyncRequest(h);
    return NULL;
	*/
	return gethostbyname(name);

#endif
}

static struct hostent *sock_get_host_by_addr(const char *addr,
					     int len, int type)
{
#ifndef _WINDOWS

    struct hostent	*hp;

    if (setjmp(env)) {
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	return (struct hostent *) NULL;
    }

    signal(SIGALRM, sock_catch_alarm);
    alarm(SOCK_GETHOST_TIMEOUT);

    hp = gethostbyaddr(addr, len, type);

    alarm(0);
    signal(SIGALRM, SIG_DFL);

    return hp;

#else

    struct hostent	*hp;

    hp = gethostbyaddr(addr, len, type);

    return hp;

#endif
}
