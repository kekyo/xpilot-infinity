#include "xpcommon.h"
#include "web_front.h"
#include "utf8_files.h"
#define TEST_CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); exit(1); \
} } while (0)
#ifdef _WINDOWS
#include <direct.h>
#endif

static int upgrades;
static char upgrade_address[SOCK_HOSTNAME_LENGTH];
static void Upgraded(sock_t *socket, const char *address, int port,
                     const server_endpoint_t *endpoint)
{
    TEST_CHECK(port > 0 && endpoint->contact_transport == GAME_TRANSPORT_WEBSOCKET);
    char header[8];
    TEST_CHECK(sock_peek(socket, header, sizeof(header)) == sizeof(header));
    TEST_CHECK(!memcmp(header, "GET /xpi", sizeof(header)));
    strlcpy(upgrade_address, address, sizeof(upgrade_address));
    upgrades++;
    char discarded[8192];
    TEST_CHECK(sock_read(socket, discarded, sizeof(discarded)) > 0);
    sock_close(socket);
}

static void Exchange(web_front_t *front, const char *message,
                     char *response, size_t capacity)
{
    sock_t listener, client, accepted;
    TEST_CHECK(sock_open_tcp_listener(&listener, "127.0.0.1", 0, 2) == 0);
    TEST_CHECK(sock_open_tcp(&client) == 0);
    int port = sock_get_port(&listener);
    TEST_CHECK(sock_connect_with_timeout(&client, "127.0.0.1", port, 5) == 0);
    TEST_CHECK(sock_accept(&listener, &accepted) == 0);
    sock_close(&listener);
    TEST_CHECK(sock_set_non_blocking(&accepted, 1) == 0);
    server_endpoint_t endpoint = { GAME_TRANSPORT_WEBSOCKET, GAME_TRANSPORT_WEBSOCKET, port };
    Web_front_accept(front, &accepted, "127.0.0.1", 1234, &endpoint);
    TEST_CHECK(sock_write(&client, (char *)message, (int)strlen(message)) == (int)strlen(message));
    size_t used = 0;
    time_t deadline = time(NULL) + 10;
    for (;;) {
        TEST_CHECK(time(NULL) <= deadline);
        Web_front_step(front);
        int count = sock_read(&client, response + used, (int)(capacity - used - 1));
        if (!count) break;
        if (count < 0) TEST_CHECK(sock_error_is_temporary(&client));
        else used += (size_t)count;
        TEST_CHECK(used < capacity - 1);
    }
    response[used] = '\0';
    sock_close(&client);
}

int main(void)
{
    TEST_CHECK(sock_startup() == 0);
    init_error("test-web-front");
    char root[128], file[180], response[4096];
    snprintf(root, sizeof(root), "web-front-test-%ld", (long)Get_process_id());
#ifdef _WINDOWS
    TEST_CHECK(_mkdir(root) == 0);
#else
    TEST_CHECK(mkdir(root, 0700) == 0);
#endif
    snprintf(file, sizeof(file), "%s/index.html", root);
    FILE *output = Xp_fopen(file, "wb");
    TEST_CHECK(output != NULL);
    TEST_CHECK(fputs("browser test body", output) >= 0);
    TEST_CHECK(fclose(output) == 0);
    web_front_t *front = Web_front_create(root, "https://game.example", "127.0.0.1", Upgraded);
    TEST_CHECK(front != NULL);
    Exchange(front, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n", response, sizeof(response));
    TEST_CHECK(strstr(response, "200 OK") && strstr(response, "browser test body"));
    Exchange(front, "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n", response, sizeof(response));
    TEST_CHECK(strstr(response, "Content-Length: 17") && !strstr(response, "browser test body"));
    char oversized[9000];
    memset(oversized, 'x', sizeof(oversized));
    const char prefix[] = "GET / HTTP/1.1\r\nHost: localhost\r\nX-Large: ";
    memcpy(oversized, prefix, sizeof(prefix) - 1);
    memcpy(oversized + sizeof(oversized) - 5, "\r\n\r\n", 5);
    Exchange(front, oversized, response, sizeof(response));
    TEST_CHECK(strstr(response, "431 Request Header Fields Too Large"));
    Exchange(front, "GET /../index.html HTTP/1.1\r\nHost: localhost\r\n\r\n", response, sizeof(response));
    TEST_CHECK(strstr(response, "404 Not Found"));
    Exchange(front, "GET /xpilot HTTP/1.1\r\nHost: localhost\r\nOrigin: https://game.example\r\nX-Real-IP: 192.0.2.1\r\n\r\n", response, sizeof(response));
    TEST_CHECK(upgrades == 1 && !strcmp(upgrade_address, "192.0.2.1"));
    Exchange(front, "GET /xpilot HTTP/1.1\r\nHost: localhost\r\nOrigin: https://game.example\r\nX-Real-IP: 2001:0db8:0:0:0:0:0:1\r\n\r\n", response, sizeof(response));
    TEST_CHECK(upgrades == 2 && !strcmp(upgrade_address, "2001:db8::1"));
    Web_front_destroy(front);
    TEST_CHECK(Xp_remove(file) == 0);
    TEST_CHECK(rmdir(root) == 0);
    sock_cleanup();
    return 0;
}
