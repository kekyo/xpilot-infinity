/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "server_endpoint.h"

#include <ctype.h>
#include <string.h>

const char *Server_endpoints_parse(const char *value,
                                   server_endpoint_t *endpoints, size_t *count)
{
    server_endpoint_t parsed[SERVER_ENDPOINT_LIMIT];
    size_t used = 0;
    const char *cursor = value;
    *count = 0;

    for (;;) {
        while (isspace((unsigned char)*cursor))
            cursor++;
        const char *name = cursor;
        while (*cursor && *cursor != ':' && *cursor != ',')
            cursor++;
        if (*cursor != ':' || cursor == name)
            return "expected protocol:port";
        size_t name_length = (size_t)(cursor - name);
        char transport_name[sizeof("websocket")];
        if (name_length >= sizeof(transport_name))
            return "unknown transport; expected udp, tcp, or websocket";
        memcpy(transport_name, name, name_length);
        transport_name[name_length] = '\0';
        game_transport_t transport;
        if (!Game_transport_parse(transport_name, &transport))
            return "unknown transport; expected udp, tcp, or websocket";
        cursor++;
        if (*cursor < '0' || *cursor > '9')
            return "expected a port in the range 1 through 65535";
        unsigned port = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            port = port * 10 + (unsigned)(*cursor++ - '0');
            if (port > 65535)
                return "port is outside the range 1 through 65535";
        }
        if (port == 0)
            return "port is outside the range 1 through 65535";
        while (isspace((unsigned char)*cursor))
            cursor++;
        if (*cursor != '\0' && *cursor != ',')
            return "expected a comma between listeners";
        for (size_t i = 0; i < used; i++) {
            if (parsed[i].contact_transport == transport)
                return "each transport may have only one listener";
            if (transport != GAME_TRANSPORT_UDP
                && parsed[i].contact_transport != GAME_TRANSPORT_UDP
                && parsed[i].port == (int)port)
                return "TCP and WebSocket must use different ports";
        }
        if (used == SERVER_ENDPOINT_LIMIT)
            return "too many listeners";
        parsed[used++] = (server_endpoint_t) { transport, transport, (int)port };
        if (*cursor == '\0')
            break;
        cursor++;
    }
    memcpy(endpoints, parsed, used * sizeof(*endpoints));
    *count = used;
    return NULL;
}

bool Server_endpoint_uses_session(const server_endpoint_t *endpoint)
{
    return endpoint->contact_transport == endpoint->game_transport
        && Game_transport_session_protocol_version(
               endpoint->game_transport, true) != 0;
}

unsigned Server_endpoint_protocol_version(const server_endpoint_t *endpoint,
                                          bool polygon_map)
{
    return Server_endpoint_uses_session(endpoint)
        ? Game_transport_session_protocol_version(
              endpoint->game_transport, polygon_map)
        : Game_transport_protocol_version(endpoint->game_transport,
                                           polygon_map);
}

bool Server_endpoint_accepts_session_versions(
    const server_endpoint_t *endpoint, unsigned polygon_version,
    unsigned legacy_version)
{
    return Server_endpoint_uses_session(endpoint)
        && polygon_version == Server_endpoint_protocol_version(endpoint, true)
        && legacy_version == Server_endpoint_protocol_version(endpoint, false);
}

bool Server_endpoint_can_resume(const server_endpoint_t *original,
                                const server_endpoint_t *replacement)
{
    return Server_endpoint_uses_session(original)
        && original->contact_transport == replacement->contact_transport
        && original->game_transport == replacement->game_transport
        && original->port == replacement->port;
}
