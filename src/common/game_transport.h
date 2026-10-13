/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2026 XPilot Infinity contributors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GAME_TRANSPORT_H
#define GAME_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>

/** UDP gameplay protocol version for polygon maps. */
#define GAME_PROTOCOL_UDP_POLYGON_VERSION 0x4F15
/** UDP gameplay protocol version for legacy maps. */
#define GAME_PROTOCOL_UDP_LEGACY_VERSION 0x4501
/** Framed TCP gameplay protocol version before session resumption. */
#define GAME_PROTOCOL_TCP_POLYGON_PRE_RECONNECT_VERSION 0x4F16
/** Legacy-map TCP protocol version before session resumption. */
#define GAME_PROTOCOL_TCP_LEGACY_PRE_RECONNECT_VERSION 0x4502
/** Framed TCP gameplay protocol version with session resumption. */
#define GAME_PROTOCOL_TCP_POLYGON_VERSION 0x4F17
/** Legacy-map TCP protocol version with session resumption. */
#define GAME_PROTOCOL_TCP_LEGACY_VERSION 0x4503
/** Fixed TCP session protocol before gameplay session resumption. */
#define GAME_PROTOCOL_TCP_SESSION_POLYGON_PRE_RECONNECT_VERSION 0x4F18
/** Legacy-map fixed TCP protocol before gameplay session resumption. */
#define GAME_PROTOCOL_TCP_SESSION_LEGACY_PRE_RECONNECT_VERSION 0x4504
/** WebSocket session protocol before gameplay session resumption. */
#define GAME_PROTOCOL_WEBSOCKET_SESSION_POLYGON_PRE_RECONNECT_VERSION 0x4F19
/** Legacy-map WebSocket protocol before gameplay session resumption. */
#define GAME_PROTOCOL_WEBSOCKET_SESSION_LEGACY_PRE_RECONNECT_VERSION 0x4505
/** Fixed-endpoint TCP session protocol version for polygon maps. */
#define GAME_PROTOCOL_TCP_SESSION_POLYGON_VERSION 0x4F1A
/** Fixed-endpoint TCP session protocol version for legacy maps. */
#define GAME_PROTOCOL_TCP_SESSION_LEGACY_VERSION 0x4506
/** WebSocket session protocol version for polygon maps. */
#define GAME_PROTOCOL_WEBSOCKET_SESSION_POLYGON_VERSION 0x4F1B
/** WebSocket session protocol version for legacy maps. */
#define GAME_PROTOCOL_WEBSOCKET_SESSION_LEGACY_VERSION 0x4507

/** Gameplay reconnection grace period shared by client and server. */
#define GAME_RECONNECT_GRACE_SECONDS 30

/** Network transport selected for an endpoint or client connection. */
typedef enum {
    /** Preserve packet boundaries with UDP datagrams. */
    GAME_TRANSPORT_UDP = 0,
    /** Preserve packet boundaries with length-prefixed TCP records. */
    GAME_TRANSPORT_TCP = 1,
    /** Preserve record boundaries with RFC 6455 binary messages. */
    GAME_TRANSPORT_WEBSOCKET = 2
} game_transport_t;

/* Server endpoints and client connections each carry their transport pair. */

/**
 * Parse a gameplay transport option value.
 *
 * @param value Case-insensitive value: `udp`, `tcp`, `websocket`, or `ws`.
 * @param transport Receives the parsed transport.
 * @return `true` when the complete value is valid, otherwise `false`.
 */
bool Game_transport_parse(const char *value, game_transport_t *transport);

/**
 * Return the option spelling for a gameplay transport.
 *
 * @param transport Transport to name.
 * @return `udp`, `tcp`, `websocket`, or `unknown` for an invalid value.
 */
const char *Game_transport_name(game_transport_t transport);

/**
 * Return the wire protocol version for a gameplay transport and map format.
 *
 * @param transport Selected gameplay transport.
 * @param polygon_map Whether the server uses the polygon map format.
 * @return Protocol version, or zero for an invalid transport.
 */
unsigned Game_transport_protocol_version(game_transport_t transport,
                                         bool polygon_map);

/**
 * Return the fixed-endpoint session protocol version for a transport.
 *
 * @param transport Selected gameplay transport.
 * @param polygon_map Whether the server uses the polygon map format.
 * @return Session protocol version, or zero when the transport has no
 *         fixed-endpoint session protocol.
 */
unsigned Game_transport_session_protocol_version(game_transport_t transport,
                                                 bool polygon_map);

/**
 * Determine whether contact and gameplay transports can be used together.
 *
 * UDP and TCP retain their existing mixed-mode support. WebSocket uses one
 * fixed session for admission and gameplay, so it must be selected at both
 * boundaries.
 *
 * @param contact Contact and lobby transport.
 * @param gameplay Gameplay transport.
 * @return `true` when the pair is implemented.
 */
bool Game_transport_pair_is_supported(game_transport_t contact,
                                      game_transport_t gameplay);

/**
 * Determine the gameplay transport represented by a protocol version.
 *
 * Historical protocol versions are classified as UDP. Known framed TCP and
 * fixed-session versions are classified by their selected transport.
 *
 * @param version Protocol version from a contact packet.
 * @param transport Receives the represented gameplay transport.
 * @return `true` when the version identifies a supported transport.
 */
bool Game_transport_from_protocol_version(unsigned version,
                                          game_transport_t *transport);

/**
 * Determine whether a protocol version supports gameplay session resumption.
 *
 * @param version Protocol version negotiated during contact.
 * @return `true` only for protocol versions carrying resumption tokens.
 */
bool Game_transport_protocol_supports_reconnect(unsigned version);

/**
 * Determine whether a protocol version uses session admission and records.
 *
 * @param version Protocol version negotiated during session admission.
 * @return `true` for fixed-endpoint session protocol versions.
 */
bool Game_transport_protocol_uses_session(unsigned version);

/**
 * Append contact and gameplay transport metadata to a version string.
 *
 * The resulting value is safe to pass through the existing metaserver
 * version field and has the form `VERSION+ct=udp+gt=tcp`.
 *
 * @param output Destination buffer.
 * @param output_size Size of the destination buffer.
 * @param version Display version without transport metadata.
 * @param contact Contact and lobby transport to advertise.
 * @param gameplay Gameplay transport to advertise.
 * @return `true` when the complete value fits and both transports are valid.
 */
bool Game_transport_format_meta_version(char *output, size_t output_size,
                                        const char *version,
                                        game_transport_t contact,
                                        game_transport_t gameplay);

/**
 * Parse transport metadata embedded in a metaserver version field.
 *
 * @param version Version value containing `+ct=...+gt=...` metadata.
 * @param contact Receives the advertised contact transport.
 * @param gameplay Receives the advertised gameplay transport.
 * @param base_length Receives the length of the display version before the
 * metadata marker.
 * @return `true` only when a complete, valid metadata suffix is present.
 */
bool Game_transport_parse_meta_version(const char *version,
                                       game_transport_t *contact,
                                       game_transport_t *gameplay,
                                       size_t *base_length);

#endif /* GAME_TRANSPORT_H */
