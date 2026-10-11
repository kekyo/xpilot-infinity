#include "test_helpers.h"
#include "xpcommon.h"
#include "session_connection.h"
#include "session_transport.h"
#include "pack.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    TEST_CHECK(argc == 5);
    TEST_CHECK(sock_startup() == 0);
    init_error(argv[0]);
    record_session_t *sessions[2] = { NULL, NULL };
    bool udp_first = strcmp(argv[3], "udp-duplicate") == 0;

    for (int i = 0; i < 2; i++) {
        if (i == 0 && udp_first) {
            sock_t socket;
            sockbuf_t packet;
            TEST_CHECK(sock_open_udp(&socket, "127.0.0.1", 0) == SOCK_IS_OK);
            TEST_CHECK(Sockbuf_init(&packet, NULL, CLIENT_SEND_SIZE,
                                   SOCKBUF_READ | SOCKBUF_WRITE | SOCKBUF_LOCK) == 0);
            unsigned magic = VERSION2MAGIC(Game_transport_protocol_version(
                GAME_TRANSPORT_UDP, true));
            TEST_CHECK(Packet_printf(&packet, "%u%s%hu%c%s%s%s%d", magic,
                "admission-test", sock_get_port(&socket), ENTER_QUEUE_pack,
                "PendingTCP", "test-display", "localhost", TEAM_NOT_SET) > 0);
            TEST_CHECK(sock_send_dest(&socket, "127.0.0.1", atoi(argv[1]),
                                      packet.buf, packet.len) == packet.len);
            sock_set_timeout(&socket, 10, 0);
            Sockbuf_clear(&packet);
            packet.len = sock_receive_any(&socket, packet.buf, packet.size);
            TEST_CHECK(packet.len > 0);
            unsigned reply_magic;
            unsigned char reply_type, reply_status;
            TEST_CHECK(Packet_scanf(&packet, "%u%c%c", &reply_magic,
                                     &reply_type, &reply_status) == 3);
            TEST_CHECK(reply_status == SUCCESS);
            TEST_CHECK(reply_type == ENTER_QUEUE_pack || reply_type == ENTER_GAME_pack);
            Sockbuf_cleanup(&packet);
            sock_close(&socket);
            continue;
        }
        game_transport_t transport = i == 0
            ? GAME_TRANSPORT_TCP : GAME_TRANSPORT_WEBSOCKET;
        record_transport_t *stream = Client_session_transport_connect(
            transport, "127.0.0.1", atoi(argv[i + 1]), 0, 0, 10, true);
        TEST_CHECK(stream != NULL);
        session_game_request_t request;
        memset(&request, 0, sizeof(request));
        game_transport_t advertised = i == 1 && strcmp(argv[3], "version") == 0
            ? GAME_TRANSPORT_TCP : transport;
        request.polygon_version = Game_transport_session_protocol_version(
            advertised, true);
        request.legacy_version = Game_transport_session_protocol_version(
            advertised, false);
        request.team = TEAM_NOT_SET;
        strlcpy(request.user, "admission-test", sizeof(request.user));
        strlcpy(request.nick, i == 0 || udp_first || strcmp(argv[3], "duplicate") == 0
                ? "PendingTCP" : "PendingWS", sizeof(request.nick));
        strlcpy(request.host, "localhost", sizeof(request.host));
        strlcpy(request.display, "test-display", sizeof(request.display));
        char confirmation[CLIENT_RECV_SIZE];
        char error_text[MSG_LEN];
        size_t confirmation_length;
        unsigned selected_version;
        int result = Session_connection_admit_game(
            stream, &request, 10, &sessions[i], confirmation,
            sizeof(confirmation), &confirmation_length, &selected_version,
            error_text, sizeof(error_text));
        if (i == 0 || strcmp(argv[4], "accepted") == 0) {
            if (result != 0)
                fprintf(stderr, "Admission failed: %s\n", error_text);
            TEST_CHECK(result == 0);
            TEST_CHECK(selected_version == request.polygon_version);
            TEST_CHECK(confirmation_length > 0);
        } else {
            fprintf(stderr, "Admission result: %s\n", error_text);
            TEST_CHECK(result == -1);
            TEST_CHECK(strcmp(error_text, argv[4]) == 0);
        }
    }
    for (int i = 0; i < 2; i++)
        Record_session_destroy(sessions[i]);
    sock_cleanup();
    return 0;
}
