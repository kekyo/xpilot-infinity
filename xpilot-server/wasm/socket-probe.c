/* Exercise the public Node socket backend independently of the game. */
#define _POSIX_C_SOURCE 200809L
#include <emscripten.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

EM_JS(void, notify_ready, (void), {
    if (Module['onSocketReady']) Module['onSocketReady']();
});

static void socket_ready(int fd, void *context)
{
    (void)fd;
    (void)context;
    notify_ready();
}

/** Register socket callbacks on the current WASM instance. */
void probe_init(void)
{
    emscripten_set_socket_open_callback(NULL, socket_ready);
    emscripten_set_socket_listen_callback(NULL, socket_ready);
    emscripten_set_socket_connection_callback(NULL, socket_ready);
    emscripten_set_socket_message_callback(NULL, socket_ready);
    emscripten_set_socket_close_callback(NULL, socket_ready);
}

/** Open a loopback socket; the caller owns a successful descriptor. */
int probe_open(int tcp, int port)
{
    int fd = socket(AF_INET, tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (fd < 0) return -errno;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        fcntl(fd, F_SETFL, O_NONBLOCK) < 0 || (tcp && listen(fd, 4) < 0)) {
        int error = errno;
        close(fd);
        return -error;
    }
    return fd;
}

/** Return the actual bound port, including an OS-assigned ephemeral port. */
int probe_port(int fd)
{
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    if (getsockname(fd, (struct sockaddr *)&address, &length) < 0)
        return -errno;
    return ntohs(address.sin_port);
}

/** Accept a descriptor owned by the caller, or zero while no peer is ready. */
int probe_accept(int fd)
{
    int peer = accept(fd, NULL, NULL);
    if (peer < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -errno;
    if (fcntl(peer, F_SETFL, O_NONBLOCK) < 0) {
        int error = errno;
        close(peer);
        return -error;
    }
    return peer;
}

/** Echo ready data; zero means pending and minus one means peer EOF. */
int probe_echo(int fd, int tcp)
{
    char buffer[1024];
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    int count = recvfrom(fd, buffer, sizeof(buffer), 0,
                         (struct sockaddr *)&address, &length);
    if (count < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -errno;
    if (!count) return -1;
    return tcp ? send(fd, buffer, count, 0)
        : sendto(fd, buffer, count, 0, (struct sockaddr *)&address, length);
}

/** Release a descriptor returned by probe_open or probe_accept. */
void probe_close(int fd)
{
    close(fd);
}
