#include "test_helpers.h"

#include <string.h>

#include "server_endpoint.h"

static int test_endpoint_versions(void)
{
    const server_endpoint_t udp = {
        GAME_TRANSPORT_UDP, GAME_TRANSPORT_UDP, 15345
    };
    const server_endpoint_t tcp = {
        GAME_TRANSPORT_TCP, GAME_TRANSPORT_TCP, 15346
    };
    const server_endpoint_t websocket = {
        GAME_TRANSPORT_WEBSOCKET, GAME_TRANSPORT_WEBSOCKET, 15347
    };
    const server_endpoint_t split = {
        GAME_TRANSPORT_UDP, GAME_TRANSPORT_TCP, 15345
    };

    TEST_CHECK(!Server_endpoint_uses_session(&udp));
    TEST_CHECK(!Server_endpoint_uses_session(&split));
    TEST_CHECK(Server_endpoint_uses_session(&tcp));
    TEST_CHECK(Server_endpoint_uses_session(&websocket));
    TEST_CHECK(Server_endpoint_protocol_version(&udp, true)
               == GAME_PROTOCOL_UDP_POLYGON_VERSION);
    TEST_CHECK(Server_endpoint_protocol_version(&split, false)
               == GAME_PROTOCOL_TCP_LEGACY_VERSION);
    TEST_CHECK(Server_endpoint_protocol_version(&tcp, true)
               == GAME_PROTOCOL_TCP_SESSION_POLYGON_VERSION);
    TEST_CHECK(Server_endpoint_protocol_version(&websocket, false)
               == GAME_PROTOCOL_WEBSOCKET_SESSION_LEGACY_VERSION);
    TEST_CHECK(Server_endpoint_accepts_session_versions(
                   &tcp, GAME_PROTOCOL_TCP_SESSION_POLYGON_VERSION,
                   GAME_PROTOCOL_TCP_SESSION_LEGACY_VERSION));
    TEST_CHECK(!Server_endpoint_accepts_session_versions(
                   &websocket, GAME_PROTOCOL_TCP_SESSION_POLYGON_VERSION,
                   GAME_PROTOCOL_TCP_SESSION_LEGACY_VERSION));
    TEST_CHECK(!Server_endpoint_accepts_session_versions(
                   &tcp, GAME_PROTOCOL_TCP_SESSION_POLYGON_VERSION,
                   GAME_PROTOCOL_WEBSOCKET_SESSION_LEGACY_VERSION));
    TEST_CHECK(!Server_endpoint_accepts_session_versions(
                   &split, GAME_PROTOCOL_TCP_POLYGON_VERSION,
                   GAME_PROTOCOL_TCP_LEGACY_VERSION));
    return 0;
}

static int test_resume_requires_original_endpoint(void)
{
    const server_endpoint_t original = {
        GAME_TRANSPORT_TCP, GAME_TRANSPORT_TCP, 15346
    };
    server_endpoint_t replacement = original;

    TEST_CHECK(Server_endpoint_can_resume(&original, &replacement));
    replacement.port++;
    TEST_CHECK(!Server_endpoint_can_resume(&original, &replacement));
    replacement = original;
    replacement.contact_transport = GAME_TRANSPORT_WEBSOCKET;
    replacement.game_transport = GAME_TRANSPORT_WEBSOCKET;
    TEST_CHECK(!Server_endpoint_can_resume(&original, &replacement));
    replacement = original;
    replacement.contact_transport = GAME_TRANSPORT_UDP;
    TEST_CHECK(!Server_endpoint_can_resume(&original, &replacement));
    return 0;
}

static int test_listener_sets(void)
{
    const char *valid[] = {
        "udp:15345", "tcp:15346", "websocket:15347",
        "udp:15345,tcp:15346", "udp:15345,websocket:15347",
        "tcp:15346,websocket:15347",
        "udp:15345,tcp:15346,websocket:15347"
    };
    const char *invalid[] = {
        "", ",", "udp", "udp:", "udp:0", "udp:65536",
        "udp:-1", "udp:+1", "udp:1x", "udp:99999999999999999999",
        "sctp:15345", "udp:1,", ",udp:1", "udp:1,,tcp:2",
        "udp:1,udp:2", "ws:1,websocket:2", "tcp:1,websocket:1"
    };
    server_endpoint_t endpoints[SERVER_ENDPOINT_LIMIT];
    size_t count;

    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        TEST_CHECK(Server_endpoints_parse(valid[i], endpoints, &count) == NULL);
        size_t expected = 1;
        for (const char *p = valid[i]; *p; p++)
            expected += *p == ',';
        TEST_CHECK(count == expected);
    }
    TEST_CHECK(Server_endpoints_parse(
                   " WS:65535 , UDP:1 , TCP:1 ", endpoints, &count) == NULL);
    TEST_CHECK(count == 3);
    TEST_CHECK(endpoints[0].contact_transport == GAME_TRANSPORT_WEBSOCKET);
    TEST_CHECK(endpoints[0].port == 65535);
    TEST_CHECK(endpoints[1].game_transport == GAME_TRANSPORT_UDP);
    TEST_CHECK(endpoints[2].game_transport == GAME_TRANSPORT_TCP);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        count = 99;
        TEST_CHECK(Server_endpoints_parse(
                       invalid[i], endpoints, &count) != NULL);
        TEST_CHECK(count == 0);
    }
    return 0;
}

int main(void)
{
    TEST_CHECK(test_endpoint_versions() == 0);
    TEST_CHECK(test_resume_requires_original_endpoint() == 0);
    TEST_CHECK(test_listener_sets() == 0);
    return 0;
}
