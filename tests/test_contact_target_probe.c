#include "xpclient.h"

#include <stdlib.h>
#include <string.h>

int clientPortStart;
int clientPortEnd;

int main(int argc, char **argv)
{
    Connect_param_t connection;
    Connect_defaults_t defaults = {
        1,
        GAME_TRANSPORT_UDP,
        GAME_TRANSPORT_TCP
    };
    Connect_target_t *targets;
    Connect_target_status_t status;
    game_transport_t expected_contact_transport;
    game_transport_t expected_game_transport;
    int auto_connect;
    int first_target;
    int invalid_index;
    int target_count;
    Contact_servers_result_t result;
    int list_servers = argc == 3 && strcmp(argv[1], "--status") == 0;

    if (argc == 3 && (strcmp(argv[1], "--interactive") == 0 || list_servers)) {
        auto_connect = list_servers;
        first_target = 2;
        target_count = 1;
    } else if (argc == 3) {
        auto_connect = 1;
        first_target = 1;
        target_count = 2;
    } else {
        return 2;
    }
    init_error(argv[0]);
    if (sock_startup() != 0)
        return 3;

    memset(&connection, 0, sizeof(connection));
    strlcpy(connection.nick_name, "TargetProbe",
            sizeof(connection.nick_name));
    strlcpy(connection.user_name, "target-probe",
            sizeof(connection.user_name));
    strlcpy(connection.host_name, "localhost",
            sizeof(connection.host_name));
    strlcpy(connection.disp_name, "test-display",
            sizeof(connection.disp_name));
    connection.team = TEAM_NOT_SET;

    status = Connect_targets_parse(target_count, &argv[first_target],
                                   &defaults, &targets, &invalid_index);
    if (status != CONNECT_TARGET_STATUS_OK) {
        sock_cleanup();
        return 4;
    }
    if (auto_connect && !list_servers) {
        expected_contact_transport = GAME_TRANSPORT_UDP;
        expected_game_transport = GAME_TRANSPORT_UDP;
    } else {
        expected_contact_transport = targets[0].contact_transport;
        expected_game_transport = targets[0].game_transport;
    }

    result = Contact_servers_detailed(
        target_count, targets, auto_connect, list_servers, 0, NULL, &connection);
    free(targets);
    sock_cleanup();
    if (list_servers)
        return result.contacted ? 0 : 5;
    if (!result.contacted || !result.connected)
        return 5;
    if (connection.contact_transport != expected_contact_transport
        || connection.game_transport != expected_game_transport)
        return 6;
    return 0;
}
