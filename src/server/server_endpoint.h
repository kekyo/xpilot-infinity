/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef SERVER_ENDPOINT_H
#define SERVER_ENDPOINT_H

#include "game_transport.h"

/** Maximum number of distinct server transport listeners. */
#define SERVER_ENDPOINT_LIMIT 3

/** One configured server contact endpoint and its gameplay policy. */
typedef struct {
    /** Protocol accepted by the contact listener. */
    game_transport_t contact_transport;
    /** Protocol used by players admitted through this endpoint. */
    game_transport_t game_transport;
    /** Local contact port, in the range 1 through 65535. */
    int port;
} server_endpoint_t;

/**
 * Parse a comma-separated list of protocol:port listeners.
 * @param value Complete list; surrounding ASCII whitespace is accepted.
 * @param endpoints Storage for SERVER_ENDPOINT_LIMIT endpoints.
 * @param count Receives the number of endpoints, or zero on failure.
 * @return NULL on success, otherwise a static diagnostic string.
 * @remarks Each protocol may occur once. TCP and WebSocket require distinct
 * ports. Endpoint storage is changed only after the entire list is validated.
 */
const char *Server_endpoints_parse(const char *value,
                                   server_endpoint_t *endpoints, size_t *count);

/**
 * Determine whether an endpoint retains its contact stream for gameplay.
 * @param endpoint Valid endpoint to inspect.
 * @return True for matching TCP or WebSocket contact and gameplay protocols.
 */
bool Server_endpoint_uses_session(const server_endpoint_t *endpoint);

/**
 * Select the wire version for an endpoint and map format.
 * @param endpoint Valid endpoint used by this connection.
 * @param polygon_map Whether the map uses polygon geometry.
 * @return The corresponding session or separate gameplay protocol version.
 */
unsigned Server_endpoint_protocol_version(const server_endpoint_t *endpoint,
                                          bool polygon_map);

/**
 * Validate both map versions offered on a fixed-endpoint session.
 * @param endpoint Endpoint that accepted the underlying transport.
 * @param polygon_version Offered polygon-map version.
 * @param legacy_version Offered legacy-map version.
 * @return True only when both versions match this endpoint's session protocol.
 */
bool Server_endpoint_accepts_session_versions(
    const server_endpoint_t *endpoint, unsigned polygon_version,
    unsigned legacy_version);

/**
 * Check whether a replacement arrives at the original session endpoint.
 * @param original Endpoint of the suspended session.
 * @param replacement Endpoint accepting the replacement transport.
 * @return True only for the same fixed-session protocols and local port.
 * @remarks Token, peer address, and session state are checked separately.
 */
bool Server_endpoint_can_resume(const server_endpoint_t *original,
                                const server_endpoint_t *replacement);

#endif /* SERVER_ENDPOINT_H */
