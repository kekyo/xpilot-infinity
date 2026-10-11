#include "test_helpers.h"

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

int main(void)
{
    TEST_CHECK(test_endpoint_versions() == 0);
    TEST_CHECK(test_resume_requires_original_endpoint() == 0);
    return 0;
}
