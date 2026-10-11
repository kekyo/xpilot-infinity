/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "server_endpoint.h"

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
