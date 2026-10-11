/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 1991-2001 by
 *
 *      Bjørn Stabell        <bjoern@xpilot.org>
 *      Ken Ronny Schouten   <ken@xpilot.org>
 *      Bert Gijsbers        <bert@xpilot.org>
 *      Dick Balaska         <dick@xpilot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xpserver.h"

#include "utf8_names.h"

#include "contact_stream.h"
#include "contact_session.h"
#include "record_session.h"
#include "session_acceptor.h"
#include "session_protocol.h"
#include "websocket_transport.h"

#define MAX_PENDING_SESSIONS 4
#define MAX_PENDING_PER_IP 2
#define SESSION_ADMISSION_TIMEOUT 4

typedef enum {
    PENDING_READING,
    PENDING_GAME_REPLY,
    PENDING_RESUME_REPLY,
    PENDING_CONTROL_REPLY,
    PENDING_CLOSING
} pending_state_t;

typedef enum {
    CONTROL_RESPONSE_DONE,
    CONTROL_RESPONSE_TEXT,
    CONTROL_RESPONSE_OPTIONS
} control_response_t;

typedef struct {
    bool active;
    pending_state_t state;
    time_t opened_at;
    session_acceptor_t *acceptor;
    record_session_t *session;
    char address[SOCK_HOSTNAME_LENGTH];
    int peer_port;
    server_endpoint_t endpoint;
    session_game_request_t game;
    unsigned selected_version;
    bool promote_game;
    bool promote_resume;
    int resume_connection;
    record_session_id_t resume_claim_id;
    session_control_request_t control;
    control_response_t control_response;
    unsigned char control_status;
    char response_text[SERVER_SEND_SIZE];
    size_t response_offset;
    int option_index;
    char option_value[MSG_LEN];
    bool option_ready;
    char output[SESSION_PROTOCOL_MAX_RECORD_SIZE];
    size_t output_length;
    bool output_accepted;
} pending_session_t;

/*
 * Global variables
 */
int			NumQueuedPlayers = 0;
int			MaxQueuedPlayers = 20;
int			NumPseudoPlayers = 0;

typedef struct {
    server_endpoint_t endpoint;
    sock_t socket;
    bool input_registered;
    bool contact_stream;
} contact_listener_t;

static contact_listener_t listeners[SERVER_ENDPOINT_LIMIT];
static size_t listener_count;
/* Only the legacy UDP or split TCP path uses the contact packet buffer. */
static contact_listener_t *legacy_listener;
static sockbuf_t ibuf;
static bool contact_initialized;
static pending_session_t pending_sessions[MAX_PENDING_SESSIONS];

static bool Owner(int request, char *user_name, char *host_addr,
		  int host_port, int pass);
static int Queue_player(char *real, char *nick, char *disp, int team,
			char *addr, char *host, unsigned version, int port,
			const server_endpoint_t *endpoint, int *qpos,
			int *login_port);
static int Check_address(char *addr);
static void Contact_process(const server_endpoint_t *endpoint,
                            sockbuf_t *request, sockbuf_t *reply,
			    const char *peer_address, int peer_port,
			    bool stream);
static int Contact_stream_request(sockbuf_t *request, sockbuf_t *reply,
			  const char *peer_address, int peer_port);
static void Contact_accept_session(socket_handle_t fd, void *arg);
static void Contact_session_poll(void);
static void Pending_cleanup(pending_session_t *pending);
static bool Queue_name_in_use(const char *nick);

size_t Contact_endpoint_count(void)
{
    return listener_count;
}

const server_endpoint_t *Contact_endpoint(size_t index)
{
    return &listeners[index].endpoint;
}

sock_t *Contact_udp_socket(int port)
{
    if (!contact_initialized)
        return NULL;
    for (size_t i = 0; i < listener_count; i++) {
        if (listeners[i].endpoint.contact_transport == GAME_TRANSPORT_UDP
            && listeners[i].endpoint.port == port)
            return &listeners[i].socket;
    }
    return NULL;
}

bool Contact_recording_supported(void)
{
    for (size_t i = 0; i < listener_count; i++) {
        if (Server_endpoint_uses_session(&listeners[i].endpoint))
            return false;
    }
    return true;
}

bool Contact_configure(void)
{
    server_endpoint_t endpoints[SERVER_ENDPOINT_LIMIT];
    size_t count;

    if (options.listen != NULL) {
        const char *single_options[] = {
            "contactPort", "contactTransport", "gameTransport"
        };
        for (size_t i = 0; i < NELEM(single_options); i++) {
            optOrigin origin = OPT_INIT;
            Option_get_value(single_options[i], &origin);
            if (origin != OPT_INIT) {
                warn("listen cannot be combined with %s", single_options[i]);
                return false;
            }
        }
        const char *reason = Server_endpoints_parse(options.listen, endpoints, &count);
        if (reason != NULL) {
            warn("Invalid listen '%s': %s", options.listen, reason);
            return false;
        }
    } else {
        game_transport_t contactTransport, gameTransport;
        if (!Game_transport_parse(options.contactTransport, &contactTransport)) {
            warn("Invalid contactTransport '%s'; expected 'udp', 'tcp', or 'websocket'",
                 options.contactTransport);
            return false;
        }
        if (!Game_transport_parse(options.gameTransport, &gameTransport)) {
            warn("Invalid gameTransport '%s'; expected 'udp', 'tcp', or 'websocket'",
                 options.gameTransport);
            return false;
        }
        if (!Game_transport_pair_is_supported(contactTransport, gameTransport)) {
            warn("WebSocket must be selected for both contact/lobby and gameplay");
            return false;
        }
        if (options.contactPort < 1 || options.contactPort > 65535) {
            warn("contactPort must be in the range 1 through 65535");
            return false;
        }
        endpoints[0] = (server_endpoint_t) {
            contactTransport, gameTransport, options.contactPort
        };
        count = 1;
    }
    for (size_t i = 0; i < count; i++) {
        if (options.reportToMetaServer) {
            for (size_t j = 0; j < i; j++) {
                if (endpoints[i].port == endpoints[j].port) {
                    warn("Metaserver reporting requires a distinct port for each listener");
                    return false;
                }
            }
        }
        listeners[i].endpoint = endpoints[i];
    }
    listener_count = count;
    if (options.recordMode != 0 && !Contact_recording_supported()) {
        warn("Server recording and playback are not supported with fixed TCP or WebSocket sessions");
        return false;
    }
    return true;
}

void Contact_cleanup(void)
{
    if (!contact_initialized)
        return;
    for (int i = 0; i < MAX_PENDING_SESSIONS; i++) {
        if (pending_sessions[i].active)
            Pending_cleanup(&pending_sessions[i]);
    }
    for (size_t i = 0; i < listener_count; i++) {
        contact_listener_t *listener = &listeners[i];
        if (listener->contact_stream) {
            Contact_stream_cleanup();
            listener->contact_stream = false;
        } else {
            if (listener->input_registered)
                remove_input(listener->socket.fd);
            if (listener->socket.fd != SOCK_FD_INVALID)
                sock_close(&listener->socket);
        }
        listener->input_registered = false;
    }
    if (ibuf.buf != NULL)
        Sockbuf_cleanup(&ibuf);
    legacy_listener = NULL;
    contact_initialized = false;
}

static bool Contact_open_listener(contact_listener_t *listener)
{
    const server_endpoint_t *endpoint = &listener->endpoint;
    sock_t *socket = &listener->socket;

    if (Server_endpoint_uses_session(endpoint)) {
        int backlog = Net_server_connection_limit() + MAX_PENDING_SESSIONS;
        if (sock_open_tcp_listener(socket, serverAddr, endpoint->port, backlog)
                == SOCK_IS_ERROR
            || sock_set_non_blocking(socket, 1) == SOCK_IS_ERROR)
            return false;
        if (install_input(Contact_accept_session, socket->fd, listener)
            == SOCK_IS_ERROR)
            return false;
        listener->input_registered = true;
    } else if (endpoint->contact_transport == GAME_TRANSPORT_TCP) {
        legacy_listener = listener;
        if (Sockbuf_init(&ibuf, NULL, SERVER_SEND_SIZE,
                        SOCKBUF_READ | SOCKBUF_WRITE | SOCKBUF_LOCK) == -1
            || Contact_stream_init(socket, serverAddr, endpoint->port,
                                   Contact_stream_request) == SOCK_IS_ERROR)
            return false;
        listener->contact_stream = true;
    } else {
        /* Create the datagram socket used by the traditional contact path. */
        legacy_listener = listener;
        if (sock_open_udp(socket, serverAddr, endpoint->port) == SOCK_IS_ERROR)
            return false;
        sock_set_timeout(socket, 0, 0);
        if (sock_set_non_blocking(socket, 1) == SOCK_IS_ERROR
            || Sockbuf_init(&ibuf, socket, SERVER_SEND_SIZE,
                            SOCKBUF_READ | SOCKBUF_WRITE | SOCKBUF_DGRAM) == -1)
            return false;
        if (install_input(Contact, socket->fd, listener) == SOCK_IS_ERROR)
            return false;
        listener->input_registered = true;
    }
    return true;
}

int Contact_init(void)
{
    for (size_t i = 0; i < listener_count; i++)
        sock_init(&listeners[i].socket);
    contact_initialized = true;
    for (size_t i = 0; i < listener_count; i++) {
        contact_listener_t *listener = &listeners[i];
        if (!Contact_open_listener(listener)) {
            error("Could not create %s contact listener on port %d",
                  Game_transport_name(listener->endpoint.contact_transport),
                  listener->endpoint.port);
            Contact_cleanup();
            return false;
        }
    }
    for (size_t i = 0; i < listener_count; i++) {
        xpprintf("Contact transport: %s on port %d.\n",
                 Game_transport_name(listeners[i].endpoint.contact_transport),
                 listeners[i].endpoint.port);
    }
    return true;
}

/*
 * Kick robot players?
 * Return the number of kicked robots.
 * Don't kick more than one robot.
 */
static int Kick_robot_players(int team)
{
    int i;

    if (NumRobots == 0)		/* no robots available for kicking */
	return 0;

    if (team == TEAM_NOT_SET) {
	if (BIT(world->rules->mode, TEAM_PLAY) && options.reserveRobotTeam) {
	    /* kick robot with lowest score from any team but robot team */
	    double low_score = FLT_MAX;
	    player_t *low_pl = NULL;

	    for (i = 0; i < NumPlayers; i++) {
		player_t *pl_i = Player_by_index(i);

		if (!Player_is_robot(pl_i) || pl_i->team == options.robotTeam)
		    continue;
		if (Get_Score(pl_i) < low_score) {
		    low_pl = pl_i;
		    low_score = Get_Score(pl_i);
		}
	    }
	    if (low_pl) {
		Robot_delete(low_pl, true);
		return 1;
	    }
	    return 0;
	} else {
	    /* kick random robot */
	    Robot_delete(NULL, true);
	    return 1;
	}
    } else {
	if (world->teams[team].NumRobots > 0) {
	    /* kick robot with lowest score from this team */
	    double low_score = FLT_MAX;
	    player_t *low_pl = NULL;

	    for (i = 0; i < NumPlayers; i++) {
		player_t *pl_i = Player_by_index(i);

		if (!Player_is_robot(pl_i) || pl_i->team != team)
		    continue;
		if (Get_Score(pl_i) < low_score) {
		    low_pl = pl_i;
		    low_score = Get_Score(pl_i);
		}
	    }
	    if (low_pl) {
		Robot_delete(low_pl, true);
		return 1;
	    }
	    return 0;
	} else
	    return 0;		/* no robots in this team */
    }
}

/*
 * Kick paused players?
 * Return the number of kicked players.
 */
static int do_kick(int team, int nonlast)
{
    int i, num_unpaused = 0;

    for (i = NumPlayers - 1; i >= 0; i--) {
	player_t *pl_i = Player_by_index(i);

	if (pl_i->conn != NULL
	    && Player_is_paused(pl_i)
	    && (team == TEAM_NOT_SET || (pl_i->team == team &&
					 pl_i->home_base != NULL))
	    && !(pl_i->privs & PRIV_NOAUTOKICK)
	    && (!nonlast || !(pl_i->privs & PRIV_AUTOKICKLAST))) {

	    if (team == TEAM_NOT_SET) {
		Set_message_f("The paused \"%s\" was kicked because the "
			      "game is full.", pl_i->name);
		Destroy_connection(pl_i->conn, "no pause with full game");
	    } else {
		Set_message_f("The paused \"%s\" was kicked because team %d "
			      "is full.", pl_i->name, team);
		Destroy_connection(pl_i->conn, "no pause with full team");
	    }
	    num_unpaused++;
	}
    }

    return num_unpaused;
}


static int Kick_paused_players(int team)
{
    int ret;

    ret = do_kick(team, 1);
    if (ret < 1)
	ret = do_kick(team, 0);

    return ret;
}


static int Reply(sockbuf_t *reply, char *host_addr, int port, bool stream)
{
    int i, result = -1;
    const int max_send_retries = 3;

    if (stream)
	return reply->len;

    for (i = 0; i < max_send_retries; i++) {
	result = sock_send_dest(&legacy_listener->socket, host_addr, port,
			        reply->buf, reply->len);
	if (result == SOCK_IS_ERROR)
	    sock_get_error(&legacy_listener->socket);
	else
	    break;
    }

    return result;
}


static int Check_names(char *nick_name, char *user_name, char *host_name)
{
    char *ptr;
    int i;

    /*
     * Bad input parameters?
     */
    if (user_name[0] == 0
	|| host_name[0] == 0
	|| nick_name[0] < 'A'
	|| nick_name[0] > 'Z')
	return E_INVAL;

    /*
     * All names must be unique (so we know who we're talking about).
     */
    /* strip trailing whitespace. */
    for (ptr = &nick_name[strlen(nick_name)]; ptr-- > nick_name; ) {
	if (isascii(*ptr) && isspace(*ptr))
	    *ptr = '\0';
	else
	    break;
    }
    for (i = 0; i < NumPlayers; i++) {
	player_t *pl_i = Player_by_index(i);

	if (strcasecmp(pl_i->name, nick_name) == 0) {
	    D(printf("%s %s\n", pl_i->name, nick_name));
	    return E_IN_USE;
	}
    }

    return SUCCESS;
}

static void Pending_cleanup(pending_session_t *pending)
{
    if (pending->resume_claim_id != RECORD_SESSION_ID_INVALID)
	Net_server_cancel_session_resume(
	    pending->resume_connection, pending->resume_claim_id);
    Session_acceptor_destroy(pending->acceptor);
    Record_session_destroy(pending->session);
    memset(pending, 0, sizeof(*pending));
}

static pending_session_t *Pending_find_free(void)
{
    int i;

    for (i = 0; i < MAX_PENDING_SESSIONS; i++) {
	if (!pending_sessions[i].active)
	    return &pending_sessions[i];
    }
    return NULL;
}

static int Pending_count_address(const char *address)
{
    int count = 0;
    int i;

    for (i = 0; i < MAX_PENDING_SESSIONS; i++) {
	if (pending_sessions[i].active
	    && strcmp(pending_sessions[i].address, address) == 0)
	    count++;
    }
    return count;
}

int Contact_attach_transport(record_transport_t *transport,
			     const char *address, int peer_port,
                             const server_endpoint_t *endpoint)
{
    pending_session_t *pending;
    record_session_t *session;

    if (transport == NULL || address == NULL || endpoint == NULL
        || !Server_endpoint_uses_session(endpoint) || peer_port < 0
	|| peer_port > 65535) {
	Record_transport_destroy(transport);
	errno = EINVAL;
	return -1;
    }
    pending = Pending_find_free();
    if (rplayback || pending == NULL
	|| Pending_count_address(address) >= MAX_PENDING_PER_IP) {
	Record_transport_destroy(transport);
	errno = EBUSY;
	return -1;
    }
    session = Record_session_create(transport);
    if (session == NULL)
	return -1;

    memset(pending, 0, sizeof(*pending));
    pending->acceptor = Session_acceptor_create(session);
    if (pending->acceptor == NULL) {
	Record_session_destroy(session);
	return -1;
    }
    pending->active = true;
    pending->state = PENDING_READING;
    pending->resume_connection = -1;
    pending->opened_at = time(NULL);
    pending->peer_port = peer_port;
    pending->endpoint = *endpoint;
    strlcpy(pending->address, address, sizeof(pending->address));
    return 0;
}

static void Contact_accept_session(socket_handle_t fd, void *arg)
{
    sock_t accepted;
    record_transport_t *transport;
    char address[SOCK_HOSTNAME_LENGTH];
    int peer_port;

    contact_listener_t *listener = arg;
    const server_endpoint_t *endpoint = &listener->endpoint;

    UNUSED_PARAM(fd);


    /* Bound work per listener so an accept backlog cannot starve gameplay. */
    for (int i = 0; i < MAX_PENDING_SESSIONS; i++) {
	errno = 0;
	if (sock_accept(&listener->socket, &accepted) == SOCK_IS_ERROR) {
	    if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR)
		error("Cannot accept %s session (%d)",
		      Game_transport_name(endpoint->contact_transport), errno);
	    return;
	}

	strlcpy(address, sock_get_last_addr(&accepted), sizeof(address));
	peer_port = sock_get_last_port(&accepted);
	if (sock_set_non_blocking(&accepted, 1) == SOCK_IS_ERROR
	    || sock_set_tcp_nodelay(&accepted, 1) == SOCK_IS_ERROR) {
	    sock_close(&accepted);
	    continue;
	}
	if (sock_set_receive_buffer_size(
		&accepted, SERVER_RECV_SIZE + 256) == SOCK_IS_ERROR)
	    error("Cannot set session receive buffer size");
	if (sock_set_send_buffer_size(
		&accepted, SERVER_SEND_SIZE + 256) == SOCK_IS_ERROR)
	    error("Cannot set session send buffer size");
	transport = endpoint->contact_transport == GAME_TRANSPORT_WEBSOCKET
	    ? Record_transport_create_websocket_server(
		&accepted, SERVER_RECV_SIZE, SERVER_SEND_SIZE)
	    : Record_transport_create_tcp(
		&accepted, SERVER_RECV_SIZE, SERVER_SEND_SIZE);
	if (transport == NULL) {
	    sock_close(&accepted);
	    continue;
	}
	Contact_attach_transport(transport, address, peer_port, endpoint);
    }
}

static int Pending_reserved_games(const pending_session_t *current,
				  const char *nick)
{
    int reserved = 0;
    int i;

    for (i = 0; i < MAX_PENDING_SESSIONS; i++) {
	const pending_session_t *pending = &pending_sessions[i];

	if (pending == current || !pending->active || !pending->promote_game)
	    continue;
	reserved++;
	if (nick != NULL && strcasecmp(pending->game.nick, nick) == 0)
	    return -1;
    }
    return reserved;
}

static int Pending_reserved_address(const char *address)
{
    int reserved = 0;

    for (int i = 0; i < MAX_PENDING_SESSIONS; i++) {
        const pending_session_t *pending = &pending_sessions[i];
        if (pending->active && pending->promote_game
            && strcmp(pending->address, address) == 0)
            reserved++;
    }
    return reserved;
}

static int Admit_session_game(pending_session_t *pending)
{
    session_game_request_t *game = &pending->game;
    int limit;
    int reserved;
    int status;

    Fix_nick_name(game->nick);
    Fix_host_name(game->host);
    if (Check_utf8_user_name(game->user) == NAME_ERROR
	|| Check_utf8_disp_name(game->display) == NAME_ERROR) {
	return E_INVAL;
    }
    if (game->team < 0 || game->team >= MAX_TEAMS)
	game->team = TEAM_NOT_SET;

    if (!Server_endpoint_accepts_session_versions(
            &pending->endpoint, game->polygon_version, game->legacy_version))
	return E_VERSION;
    pending->selected_version = is_polygon_map
	? Server_endpoint_protocol_version(&pending->endpoint, true)
	: Server_endpoint_protocol_version(&pending->endpoint, false);
    if (Check_address(pending->address))
	return E_GAME_LOCKED;
    status = Check_names(game->nick, game->user, game->host);
    if (status != SUCCESS)
	return status;

    reserved = Pending_reserved_games(pending, game->nick);
    if (reserved < 0 || Net_server_name_in_use(game->nick)
        || Queue_name_in_use(game->nick))
	return E_IN_USE;
    if (game_lock && !rplayback && !options.baselessPausing)
	return E_GAME_LOCKED;
    if (Check_max_clients_per_IP(pending->address,
                                Pending_reserved_address(pending->address)))
	return E_GAME_LOCKED;

    limit = (int)MIN(options.playerLimit,
		     options.baselessPausing ? 1000000 : Num_bases());
    if (NumPlayers - NumPseudoPlayers + login_in_progress + reserved
	>= limit) {
	if (game_lock
	    || ((!Kick_robot_players(TEAM_NOT_SET)
		 || NumPlayers - NumPseudoPlayers + login_in_progress
		    + reserved >= limit)
		&& (!Kick_paused_players(TEAM_NOT_SET)
		    || NumPlayers - NumPseudoPlayers + login_in_progress
		       + reserved >= limit)))
	    return E_GAME_FULL;
    }

    if (BIT(world->rules->mode, TEAM_PLAY)) {
	if (game->team >= 0 && game->team < MAX_TEAMS) {
	    if (game_lock
		|| (game->team == options.robotTeam
		    && options.reserveRobotTeam)
		|| (world->teams[game->team].NumMembers
		    >= world->teams[game->team].NumBases
		    && !Kick_robot_players(game->team)
		    && !Kick_paused_players(game->team)))
		game->team = TEAM_NOT_SET;
	}
	if (game->team == TEAM_NOT_SET)
	    game->team = Pick_team(PL_TYPE_HUMAN);
	if (game->team == TEAM_NOT_SET && !game_lock
	    && NumRobots > world->teams[options.robotTeam].NumRobots) {
	    Kick_robot_players(TEAM_NOT_SET);
	    game->team = Pick_team(PL_TYPE_HUMAN);
	}
	if (game->team == TEAM_NOT_SET)
	    return E_TEAM_FULL;
    } else
	game->team = TEAM_NOT_SET;

    return SUCCESS;
}

static const char *Session_status_reason(int status)
{
    switch (status) {
    case SUCCESS:
	return "accepted";
    case E_NOT_OWNER:
	return "permission denied";
    case E_GAME_FULL:
	return "game is full";
    case E_TEAM_FULL:
	return "team is full";
    case E_GAME_LOCKED:
	return "game is locked";
    case E_IN_USE:
	return "name is already in use";
    case E_INVAL:
	return "invalid request";
    case E_VERSION:
	return "incompatible protocol version";
    case E_NOT_FOUND:
	return "player not found";
    case E_NOENT:
	return "option not found";
    case E_UNDEFINED:
	return "command is not supported";
    default:
	return "session failed";
    }
}

static int Queue_session_game_reply(pending_session_t *pending, int status)
{
    session_game_reply_t reply;

    memset(&reply, 0, sizeof(reply));
    reply.status = (unsigned char)status;
    reply.selected_version = (unsigned short)pending->selected_version;
    strlcpy(reply.reason, Session_status_reason(status),
	    sizeof(reply.reason));
    if (Session_protocol_encode_game_reply(
	    pending->output, sizeof(pending->output),
	    &pending->output_length, &reply) == -1)
	return -1;
    pending->output_accepted = false;
    pending->state = PENDING_GAME_REPLY;
    pending->promote_game = status == SUCCESS;
    return 0;
}

static int Queue_session_resume_reply(pending_session_t *pending, int status)
{
    session_resume_reply_t reply;
    const char *reason;

    memset(&reply, 0, sizeof(reply));
    reply.status = (unsigned char)status;
    reason = status == E_NOT_FOUND || status == E_IN_USE
	? "session unavailable" : Session_status_reason(status);
    strlcpy(reply.reason, reason, sizeof(reply.reason));
    if (Session_protocol_encode_resume_reply(
	    pending->output, sizeof(pending->output),
	    &pending->output_length, &reply) == -1)
	return -1;
    pending->output_accepted = false;
    pending->state = PENDING_RESUME_REPLY;
    pending->promote_resume = status == SUCCESS;
    return 0;
}

static bool Session_control_is_public(unsigned char command)
{
    return command == CONTACT_pack || command == REPORT_STATUS_pack
	|| command == OPTION_LIST_pack;
}

static bool Session_control_is_owner(const pending_session_t *pending)
{
    return (strncmp(pending->address, "127.", 4) == 0
	    || strcmp(pending->address, "::1") == 0)
	&& strcmp(pending->control.user, Server.owner) == 0;
}

static bool Session_control_next_option(pending_session_t *pending,
					char *value)
{
    int result;

    for (;;) {
	result = Parser_list_option(&pending->option_index, value);
	if (result < 0)
	    return false;
	pending->option_index++;
	if (result > 0)
	    return true;
    }
}

static int Queue_session_control_frame(pending_session_t *pending,
				       const char *payload, bool more)
{
    session_control_reply_t reply;

    memset(&reply, 0, sizeof(reply));
    reply.command = pending->control.command;
    reply.status = pending->control_status;
    reply.more = more;
    strlcpy(reply.payload, payload, sizeof(reply.payload));
    if (Session_protocol_encode_control_reply(
	    pending->output, sizeof(pending->output),
	    &pending->output_length, &reply) == -1)
	return -1;
    pending->output_accepted = false;
    pending->state = PENDING_CONTROL_REPLY;
    return 0;
}

static int Queue_next_session_control_frame(pending_session_t *pending)
{
    char payload[MSG_LEN];
    bool more;

    if (pending->control_response == CONTROL_RESPONSE_TEXT) {
	size_t total = strlen(pending->response_text);
	size_t remaining = total - pending->response_offset;
	size_t length = MIN(remaining, sizeof(payload) - 1);

	memcpy(payload, pending->response_text + pending->response_offset,
	       length);
	payload[length] = '\0';
	pending->response_offset += length;
	more = pending->response_offset < total;
	if (!more)
	    pending->control_response = CONTROL_RESPONSE_DONE;
	return Queue_session_control_frame(pending, payload, more);
    }
    if (pending->control_response == CONTROL_RESPONSE_OPTIONS) {
	if (!pending->option_ready) {
	    pending->option_ready = Session_control_next_option(
		pending, pending->option_value);
	    if (!pending->option_ready) {
		pending->control_response = CONTROL_RESPONSE_DONE;
		return Queue_session_control_frame(pending, "", false);
	    }
	}
	strlcpy(payload, pending->option_value, sizeof(payload));
	pending->option_ready = Session_control_next_option(
	    pending, pending->option_value);
	more = pending->option_ready;
	if (!more)
	    pending->control_response = CONTROL_RESPONSE_DONE;
	return Queue_session_control_frame(pending, payload, more);
    }
    return -1;
}

static int Queue_simple_session_control_reply(pending_session_t *pending,
					      int status)
{
    pending->control_status = (unsigned char)status;
    pending->control_response = CONTROL_RESPONSE_DONE;
    return Queue_session_control_frame(
	pending, Session_status_reason(status), false);
}

static int Execute_session_control(pending_session_t *pending)
{
    session_control_request_t *control = &pending->control;
    char argument[MSG_LEN];
    char *separator;
    int status = SUCCESS;

    if (Check_utf8_user_name(control->user) == NAME_ERROR)
	return Queue_simple_session_control_reply(pending, E_INVAL);
    if (!Server_endpoint_accepts_session_versions(
            &pending->endpoint, control->polygon_version,
            control->legacy_version))
	return Queue_simple_session_control_reply(pending, E_VERSION);
    if (!Session_control_is_public(control->command)
	&& !Session_control_is_owner(pending))
	return Queue_simple_session_control_reply(pending, E_NOT_OWNER);

    switch (control->command) {
    case CONTACT_pack:
	return Queue_simple_session_control_reply(pending, SUCCESS);
    case REPORT_STATUS_pack:
	xpprintf("%s %s@%s asked for info about current game.\n",
		 showtime(), control->user, pending->address);
	Server_info(pending->response_text, sizeof(pending->response_text));
	pending->response_offset = 0;
	pending->control_status = SUCCESS;
	pending->control_response = CONTROL_RESPONSE_TEXT;
	return Queue_next_session_control_frame(pending);
    case OPTION_LIST_pack:
	xpprintf("%s %s@%s asked for an option list.\n",
		 showtime(), control->user, pending->address);
	pending->option_index = 0;
	pending->option_ready = false;
	pending->control_status = SUCCESS;
	pending->control_response = CONTROL_RESPONSE_OPTIONS;
	return Queue_next_session_control_frame(pending);
    case MESSAGE_pack:
	if (control->argument[0] == '\0')
	    status = E_INVAL;
	else
	    Set_message_f("%s [%s SPEAKING FROM ABOVE]",
			  control->argument, control->user);
	break;
    case LOCK_GAME_pack:
	if (!strcasecmp(control->argument, "1")
	    || !strcasecmp(control->argument, "on")
	    || !strcasecmp(control->argument, "true"))
	    game_lock = true;
	else if (!strcasecmp(control->argument, "0")
		 || !strcasecmp(control->argument, "off")
		 || !strcasecmp(control->argument, "false"))
	    game_lock = false;
	else
	    status = E_INVAL;
	break;
    case SHUTDOWN_pack:
	Server_shutdown(control->user, 1,
		control->argument[0] != '\0'
		? control->argument : "shutdown requested");
	break;
    case KICK_PLAYER_pack:
    {
	player_t *player = Get_player_by_name(control->argument, NULL, NULL);

	if (player == NULL)
	    status = E_NOT_FOUND;
	else {
	    Set_message_f("\"%s\" upset the gods and was kicked out "
			  "of the game.", player->name);
	    if (player->conn == NULL)
		Delete_player(player);
	    else
		Destroy_connection(player->conn, "kicked out");
	    updateScores = true;
	}
	break;
    }
    case OPTION_TUNE_pack:
	strlcpy(argument, control->argument, sizeof(argument));
	separator = strchr(argument, ':');
	if (separator == NULL || separator == argument
	    || separator[1] == '\0')
	    status = E_INVAL;
	else {
	    int tune_result;

	    *separator++ = '\0';
	    tune_result = Tune_option(argument, separator);
	    if (tune_result == 1) {
		if (strcasecmp(argument, "password")) {
		    char value[MAX_CHARS];

		    Get_option_value(argument, value, sizeof(value));
		    Set_message_f(" < Option %s set to %s by %s "
				  "FROM ABOVE. >",
				  argument, value, control->user);
		}
	    } else if (tune_result == -1)
		status = E_UNDEFINED;
	    else if (tune_result == -2)
		status = E_NOENT;
	    else if (tune_result != 1)
		status = E_INVAL;
	}
	break;
    default:
	status = E_UNDEFINED;
	break;
    }
    return Queue_simple_session_control_reply(pending, status);
}

static int Pending_flush_output(pending_session_t *pending)
{
    record_flush_result_t flush_result;
    record_send_result_t send_result;

    if (!pending->output_accepted && pending->output_length > 0) {
	send_result = Record_session_send(
	    pending->session, pending->output, pending->output_length,
	    RECORD_DELIVERY_REQUIRED);
	if (send_result == RECORD_SEND_BACKPRESSURED)
	    return 0;
	if (send_result != RECORD_SEND_ACCEPTED)
	    return -1;
	pending->output_length = 0;
	pending->output_accepted = true;
    }
    flush_result = Record_session_flush(pending->session);
    if (flush_result == RECORD_FLUSH_ERROR
	|| flush_result == RECORD_FLUSH_CLOSED)
	return -1;
    if (flush_result == RECORD_FLUSH_PENDING)
	return 0;
    pending->output_accepted = false;
    return 1;
}

static void Promote_session_game(pending_session_t *pending)
{
    session_game_request_t game = pending->game;
    record_session_t *session = pending->session;
    char address[SOCK_HOSTNAME_LENGTH];
    int peer_port = pending->peer_port;
    server_endpoint_t endpoint = pending->endpoint;
    unsigned version = pending->selected_version;

    strlcpy(address, pending->address, sizeof(address));
    pending->session = NULL;
    memset(pending, 0, sizeof(*pending));
    Setup_session_connection(
	session, &endpoint, game.user, game.nick, game.display,
	game.team, address, game.host,
	version, peer_port);
}

static void Promote_session_resume(pending_session_t *pending)
{
    record_session_t *replacement = pending->session;

    if (Net_server_complete_session_resume(
	    pending->resume_connection, pending->resume_claim_id,
	    replacement, pending->peer_port) == -1) {
	Pending_cleanup(pending);
	return;
    }
    pending->session = NULL;
    memset(pending, 0, sizeof(*pending));
}

static void Pending_finish_response(pending_session_t *pending)
{
    /* Let WebSocket clients consume the final response and initiate the close
     * handshake before releasing the stream, including admission failures. */
    if (pending->endpoint.contact_transport == GAME_TRANSPORT_WEBSOCKET) {
        pending->state = PENDING_CLOSING;
        pending->opened_at = time(NULL);
    } else
        Pending_cleanup(pending);
}

static void Process_pending_session(pending_session_t *pending)
{
    session_acceptor_result_t accept_result;
    record_receive_result_t receive_result;
    session_open_t open;
    char ignored[SESSION_PROTOCOL_MAX_RECORD_SIZE];
    size_t ignored_length;
    int status;

    if ((pending->state == PENDING_READING
	 || pending->state == PENDING_CLOSING)
	&& time(NULL) >= pending->opened_at + SESSION_ADMISSION_TIMEOUT) {
	Pending_cleanup(pending);
	return;
    }

    if (pending->state == PENDING_CLOSING) {
	receive_result = Record_session_receive(
	    pending->session, ignored, sizeof(ignored), &ignored_length);
	if (receive_result == RECORD_RECEIVE_EMPTY)
	    return;
	Pending_cleanup(pending);
	return;
    }

    if (pending->state == PENDING_READING) {
	accept_result = Session_acceptor_step(pending->acceptor, &open);
	if (accept_result == SESSION_ACCEPTOR_PENDING)
	    return;
	if (accept_result == SESSION_ACCEPTOR_ERROR) {
	    Pending_cleanup(pending);
	    return;
	}
	pending->session = Session_acceptor_take_session(pending->acceptor);
	Session_acceptor_destroy(pending->acceptor);
	pending->acceptor = NULL;
	if (pending->session == NULL) {
	    Pending_cleanup(pending);
	    return;
	}
	if (accept_result == SESSION_ACCEPTOR_GAME_READY) {
	    pending->game = open.request.game;
	    status = Admit_session_game(pending);
	    if (Queue_session_game_reply(pending, status) == -1) {
		Pending_cleanup(pending);
		return;
	    }
	} else if (accept_result == SESSION_ACCEPTOR_CONTROL_READY) {
	    pending->control = open.request.control;
	    if (Execute_session_control(pending) == -1) {
		Pending_cleanup(pending);
		return;
	    }
	} else {
	    record_session_id_t claim_id = Record_session_id(
		pending->session);

	    pending->resume_connection = -1;
	    status = Net_server_claim_session_resume(
		&open.request.resume.token, pending->address, &pending->endpoint,
		claim_id, &pending->resume_connection);
	    if (status == SUCCESS)
		pending->resume_claim_id = claim_id;
	    if (Queue_session_resume_reply(pending, status) == -1) {
		Pending_cleanup(pending);
		return;
	    }
	}
    }

    status = Pending_flush_output(pending);
    if (status < 0) {
	Pending_cleanup(pending);
	return;
    }
    if (status == 0)
	return;
    if (pending->state == PENDING_GAME_REPLY) {
	if (pending->promote_game)
	    Promote_session_game(pending);
	else
	    Pending_finish_response(pending);
	return;
    }
    if (pending->state == PENDING_RESUME_REPLY) {
	if (pending->promote_resume)
	    Promote_session_resume(pending);
	else
	    Pending_finish_response(pending);
	return;
    }
    if (pending->control_response == CONTROL_RESPONSE_DONE) {
	Pending_finish_response(pending);
	return;
    }
    if (Queue_next_session_control_frame(pending) == -1)
	Pending_cleanup(pending);
}

static void Contact_session_poll(void)
{
    int i;

    for (i = 0; i < MAX_PENDING_SESSIONS; i++) {
	if (pending_sessions[i].active)
	    Process_pending_session(&pending_sessions[i]);
    }
}


/*
 * Support some older clients, which don't know
 * that they can join the current version.
 *
 * IMPORTANT! Adjust the next code if you're changing version numbers.
 */
static unsigned Version_to_magic(const server_endpoint_t *endpoint,
                                 unsigned version)
{
    game_transport_t requested_transport;
    unsigned current_version = Server_endpoint_protocol_version(
        endpoint, is_polygon_map);

    if (Game_transport_from_protocol_version(version, &requested_transport)
	&& requested_transport == endpoint->game_transport
	&& version >= 0x4203 && version <= current_version)
	return VERSION2MAGIC(version);
    return VERSION2MAGIC(current_version);
}

void Contact(socket_handle_t fd, void *arg)
{
    contact_listener_t *listener = arg;
    int bytes;

    UNUSED_PARAM(fd);
    Sockbuf_clear(&ibuf);
    bytes = sock_receive_any(&legacy_listener->socket, ibuf.buf, ibuf.size);
    if (bytes <= 8) {
	if (bytes < 0 && errno != EWOULDBLOCK && errno != EAGAIN
	    && errno != EINTR)
	    sock_get_error(&legacy_listener->socket);
	return;
    }
    ibuf.len = bytes;
    Contact_process(&listener->endpoint, &ibuf, &ibuf, sock_get_last_addr(&legacy_listener->socket),
		    sock_get_last_port(&legacy_listener->socket), false);
}

static int Contact_stream_request(sockbuf_t *request, sockbuf_t *reply,
			  const char *peer_address, int peer_port)
{
    Contact_process(&legacy_listener->endpoint, request, reply,
                    peer_address, peer_port, true);
    return reply->len;
}

static void Contact_process(const server_endpoint_t *endpoint,
                            sockbuf_t *request, sockbuf_t *reply,
			    const char *peer_address, int peer_port,
			    bool stream)
{
    int i, team, delay, qpos, status, login_port;
    char reply_to, ch;
    unsigned magic, version, my_magic;
    unsigned current_version = Server_endpoint_protocol_version(
        endpoint, is_polygon_map);
    unsigned server_magic = VERSION2MAGIC(current_version);
    uint16_t port;
    char user_name[MAX_CHARS], disp_name[MAX_CHARS], nick_name[MAX_CHARS];
    char host_name[MAX_CHARS], host_addr[SOCK_HOSTNAME_LENGTH], str[MSG_LEN];

    strlcpy(host_addr, peer_address, sizeof(host_addr));
    xpprintf("%s Checking Address:(%s)\n", showtime(), host_addr);
    if (Check_address(host_addr)) {
	xpprintf("%s Host blocked!:(%s)\n", showtime(), host_addr);
	return;
    }

    /*
     * Determine if we can talk with this client.
     */
    if (Packet_scanf(request, "%u", &magic) <= 0
	|| (magic & 0xFFFF) != MAGIC_WORD) {
	D(printf("Incompatible packet from %s (0x%08x)", host_addr, magic));
	return;
    }
    version = MAGIC2VERSION(magic);

    /*
     * Read core of packet.
     */
    if (Packet_scanf(request, "%s%hu%c", user_name, &port, &ch) <= 0) {
	D(printf("Incomplete packet from %s", host_addr));
	return;
    }
    Fix_user_name(user_name);
    reply_to = (ch & 0xFF);	/* no sign extension. */

    /* Ignore the advertised port; replies target the observed peer port. */
    port = (uint16_t)peer_port;

    /*
     * Now see if we have the same (or a compatible) version.
     * If the client request was only a contact request (to see
     * if there is a server running on this host) then we don't
     * care about version incompatibilities, so that the client
     * can decide if it wants to conform to our version or not.
     */
    if (version != 0
	&& (reply_to == ENTER_GAME_pack || reply_to == ENTER_QUEUE_pack)) {
	game_transport_t requested_transport;

	if (!Game_transport_from_protocol_version(version,
					      &requested_transport)
	    || requested_transport != endpoint->game_transport) {
	    D(error("Gameplay transport mismatch with %s@%s (%s,%04x)",
		    user_name, host_addr,
		    Game_transport_name(endpoint->game_transport), version));
	    Sockbuf_clear(reply);
	    Packet_printf(reply, "%u%c%c", server_magic, reply_to, E_VERSION);
	    Reply(reply, host_addr, port, stream);
	    return;
	}
    }

    if (version < MIN_CLIENT_VERSION
	|| (version > current_version
	    && reply_to != CONTACT_pack)) {
	D(error("Incompatible version with %s@%s (%04x,%04x)",
		user_name, host_addr, current_version, version));
	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", server_magic, reply_to, E_VERSION);
	Reply(reply, host_addr, port, stream);
	return;
    }

    my_magic = Version_to_magic(endpoint, version);

    status = SUCCESS;

    if (reply_to & PRIVILEGE_PACK_MASK) {
	long key;
	static long credentials;

	if (!credentials) {
	    credentials = (time(NULL) * (time_t)Get_process_id());
	    credentials ^= (long)(uintptr_t)Contact;
	    credentials	+= (long)key + (long)(uintptr_t)&key;
	    credentials ^= (long)randomMT() << 1;
	    credentials &= 0xFFFFFFFF;
	}
	if (Packet_scanf(request, "%ld", &key) <= 0)
	    return;

	if (!Owner((int)reply_to, user_name, host_addr, port,
		   key == credentials)) {
	    Sockbuf_clear(reply);
	    Packet_printf(reply, "%u%c%c", my_magic, reply_to, E_NOT_OWNER);
	    Reply(reply, host_addr, port, stream);
	    return;
	}
	if (reply_to == CREDENTIALS_pack) {
	    Sockbuf_clear(reply);
	    Packet_printf(reply, "%u%c%c%ld", my_magic, reply_to, SUCCESS,
			  credentials);
	    Reply(reply, host_addr, port, stream);
	    return;
	}
    }

    /*
     * Now decode the packet type field and do something witty.
     */
    switch (reply_to) {

    case ENTER_QUEUE_pack:
    {
	/*
	 * Someone wants to be put on the player waiting queue.
	 */
	if (Packet_scanf(request, "%s%s%s%d", nick_name, disp_name, host_name,
			 &team) <= 0) {
	    D(printf("Incomplete enter queue from %s@%s",
		     user_name, host_addr));
	    return;
	}
	Fix_nick_name(nick_name);
	Fix_disp_name(disp_name);
	Fix_host_name(host_name);
	if (team < 0 || team >= MAX_TEAMS)
	    team = TEAM_NOT_SET;

	status = Queue_player(user_name, nick_name,
			      disp_name, team,
			      host_addr, host_name,
			      version, port,
			      endpoint,
			      &qpos, &login_port);
	if (status < 0)
	    return;

	Sockbuf_clear(reply);
	if (status == SUCCESS && login_port > 0)
	    Packet_printf(reply, "%u%c%c%hu", my_magic, ENTER_GAME_pack,
			  status, login_port);
	else
	    Packet_printf(reply, "%u%c%c%hu", my_magic, reply_to,
			  status, qpos);
    }
    break;


    case REPORT_STATUS_pack:
    {
	/*
	 * Someone asked for information.
	 */

	xpprintf("%s %s@%s asked for info about current game.\n",
		 showtime(), user_name, host_addr);
	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, SUCCESS);
	assert(reply->size - reply->len >= 0);
	Server_info(reply->buf + reply->len, (size_t)(reply->size - reply->len));
	reply->buf[reply->size - 1] = '\0';
	reply->len += strlen(reply->buf + reply->len) + 1;
    }
    break;


    case MESSAGE_pack:
    {
	/*
	 * Someone wants to transmit a message to the server.
	 */

	if (Packet_scanf(request, "%s", str) <= 0)
	    status = E_INVAL;
	else
	    Set_message_f("%s [%s SPEAKING FROM ABOVE]", str, user_name);

	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;


    case LOCK_GAME_pack:
    {
	/*
	 * Someone wants to lock the game so that no more players can enter.
	 */

	game_lock = game_lock ? false : true;
	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;


    case CONTACT_pack:
    {
	/*
	 * Got contact message from client.
	 */

	D(printf("Got CONTACT from %s.\n", host_addr));
	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;


    case SHUTDOWN_pack:
    {
	char reason[MAX_CHARS];
	/*
	 * Shutdown the entire server.
	 */

	if (Packet_scanf(request, "%d%s", &delay, reason) <= 0)
	    status = E_INVAL;
	else
	    Server_shutdown(user_name, delay, reason);

	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;


    case KICK_PLAYER_pack:
    {
	/*
	 * Kick someone from the game.
	 */
	if (Packet_scanf(request, "%s", str) <= 0)
	    status = E_INVAL;
	else {
	    player_t *pl_found = Get_player_by_name(str, NULL, NULL);

	    if (!pl_found)
		status = E_NOT_FOUND;
	    else {
		Set_message_f("\"%s\" upset the gods and was kicked out "
			      "of the game.", pl_found->name);
		if (pl_found->conn == NULL)
		    Delete_player(pl_found);
		else
		    Destroy_connection(pl_found->conn, "kicked out");
		updateScores = true;
	    }
	}

	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;

    case OPTION_TUNE_pack:
    {
	/*
	 * Tune a server option.  (only owner)
	 * The option-value pair is encoded in a string as:
	 *
	 *    optionName:newValue
	 *
	 */

	char *opt, *val;

	if (Packet_scanf(request, "%S", str) <= 0
		 || (opt = strtok(str, ":")) == NULL
		 || (val = strtok(NULL, "")) == NULL)
	    status = E_INVAL;
	else {
	    i = Tune_option(opt, val);
	    if (i == 1) {
		status = SUCCESS;
		if (strcasecmp(opt, "password")) {
		    char value[MAX_CHARS];

		    Get_option_value(opt, value, sizeof(value));
		    Set_message_f(" < Option %s set to %s by %s FROM ABOVE. >",
				  opt, value, user_name);
		}
	    }
	    else if (i == 0)
		status = E_INVAL;
	    else if (i == -1)
		status = E_UNDEFINED;
	    else if (i == -2)
		status = E_NOENT;
	    else
		status = E_INVAL;
	}
	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);
    }
    break;

    case OPTION_LIST_pack:
    {
	/*
	 * List the server options and their current values.
	 */
	bool		bad = false, full, change;

	xpprintf("%s %s@%s asked for an option list.\n",
		 showtime(), user_name, host_addr);
	i = 0;
	do {
	    Sockbuf_clear(reply);
	    Packet_printf(reply, "%u%c%c", my_magic, reply_to, status);

	    for (change = false, full = false; !full && !bad; ) {
		switch (Parser_list_option(&i, str)) {
		case -1:
		    bad = true;
		    break;
		case 0:
		    i++;
		    break;
		default:
		    switch (Packet_printf(reply, "%s", str)) {
		    case 0:
			full = true;
			bad = (change) ? false : true;
			break;
		    case -1:
			bad = true;
			break;
		    default:
			change = true;
			i++;
			break;
		    }
		    break;
		}
	    }
	    if (change && Reply(reply, host_addr, port, stream) == -1)
		bad = true;
	    if (stream)
		bad = true;

	} while (!bad);
    }
    return;

    default:
	/*
	 * Incorrect packet type.
	 */
	D(printf("Unknown packet type (%d) from %s@%s.\n",
		 reply_to, user_name, host_addr));

	Sockbuf_clear(reply);
	Packet_printf(reply, "%u%c%c", my_magic, reply_to, E_VERSION);
    }

    Reply(reply, host_addr, port, stream);
}


struct queued_player {
    struct queued_player	*next;
    char			user_name[MAX_CHARS];
    char			nick_name[MAX_CHARS];
    char			disp_name[MAX_CHARS];
    char			host_name[MAX_CHARS];
    char			host_addr[24];
    int				port;
    int				team;
    unsigned			version;
    int				login_port;
    server_endpoint_t		endpoint;
    long			last_ack_sent;
    long			last_ack_recv;
};

struct queued_player	*qp_list;

static bool Queue_name_in_use(const char *nick)
{
    for (const struct queued_player *qp = qp_list; qp != NULL; qp = qp->next) {
        if (!strcasecmp(qp->nick_name, nick))
            return true;
    }
    return false;
}

static void Queue_remove(struct queued_player *qp, struct queued_player *prev)
{
    if (qp == qp_list)
	qp_list = qp->next;
    else
	prev->next = qp->next;
    free(qp);
    NumQueuedPlayers--;
}

void Queue_kick(const char *nick)
{
    unsigned int magic;
    struct queued_player *qp = qp_list, *prev = NULL;

    while (qp) {
	if (!strcasecmp(qp->nick_name, nick))
	    break;
	prev = qp;
	qp = qp->next;
    }

    if (!qp)
	return;

    if (qp->endpoint.contact_transport == GAME_TRANSPORT_UDP) {
	magic = Version_to_magic(&qp->endpoint, qp->version);
	Sockbuf_clear(&ibuf);
	Packet_printf(&ibuf, "%u%c%c", magic, ENTER_GAME_pack, E_IN_USE);
	Reply(&ibuf, qp->host_addr, qp->port, false);
    }
    Queue_remove(qp, prev);

    return;
}

static void Queue_ack(struct queued_player *qp, int qpos)
{
    unsigned my_magic = Version_to_magic(&qp->endpoint, qp->version);

    if (qp->endpoint.contact_transport == GAME_TRANSPORT_TCP) {
	qp->last_ack_sent = main_loops;
	return;
    }
    Sockbuf_clear(&ibuf);
    if (qp->login_port == -1)
	Packet_printf(&ibuf, "%u%c%c%hu",
		      my_magic, ENTER_QUEUE_pack, SUCCESS, qpos);
    else
	Packet_printf(&ibuf, "%u%c%c%hu",
		      my_magic, ENTER_GAME_pack, SUCCESS, qp->login_port);
    Reply(&ibuf, qp->host_addr, qp->port, false);
    qp->last_ack_sent = main_loops;
}

void Queue_loop(void)
{
    struct queued_player *qp, *prev = 0, *next = 0;
    int qpos = 0, login_port;
    static long last_unqueued_loops;

    Contact_session_poll();
    if (legacy_listener != NULL && legacy_listener->contact_stream)
	Contact_stream_poll();

    for (qp = qp_list; qp && qp->login_port > 0; ) {
	next = qp->next;

	if (qp->last_ack_recv + 30 * FPS < main_loops) {
	    Queue_remove(qp, prev);
	    qp = next;
	    continue;
	}
	if (qp->last_ack_sent + 2 < main_loops) {
	    login_port = Check_connection(qp->user_name, qp->nick_name,
					  qp->disp_name, qp->host_addr);
	    if (login_port == -1) {
		Queue_remove(qp, prev);
		qp = next;
		continue;
	    }
	    if (qp->last_ack_sent + 2 + (FPS >> 2) < main_loops) {
		Queue_ack(qp, 0);

		/* don't do too much at once. */
		return;
	    }
	}

	prev = qp;
	qp = next;
    }

    /* here's a player in the queue without a login port. */
    if (qp) {

	if (qp->last_ack_recv + 30 * FPS < main_loops) {
	    Queue_remove(qp, prev);
	    return;
	}

	/* slow down the rate at which players enter the game. */
	if (last_unqueued_loops + 2 + (FPS >> 2) < main_loops) {
	    int reserved = Pending_reserved_games(NULL, NULL);
	    int lim = (int)MIN(options.playerLimit,
			       options.baselessPausing
			       ? 1e6 : Num_bases());

	    /* Recheck reservations made since this player entered the queue. */
            if (Check_max_clients_per_IP(qp->host_addr,
                                        Pending_reserved_address(qp->host_addr)))
                return;
	    /* is there a homebase available? */
	    if (NumPlayers - NumPseudoPlayers + login_in_progress + reserved < lim
		|| !game_lock && ((Kick_robot_players(TEAM_NOT_SET)
		    && NumPlayers - NumPseudoPlayers + login_in_progress + reserved < lim)
		|| (Kick_paused_players(TEAM_NOT_SET) &&
		    NumPlayers - NumPseudoPlayers + login_in_progress + reserved < lim))){

		/* find a team for this fellow. */
		if (BIT(world->rules->mode, TEAM_PLAY)) {
		    /* see if he has a reasonable suggestion. */
		    if (qp->team >= 0 && qp->team < MAX_TEAMS) {
			if (game_lock ||
			    (qp->team == options.robotTeam
			     && options.reserveRobotTeam) ||
			    (world->teams[qp->team].NumMembers
			     >= world->teams[qp->team].NumBases &&
			     !Kick_robot_players(qp->team) &&
			     !Kick_paused_players(qp->team)))
			    qp->team = TEAM_NOT_SET;
		    }
		    if (qp->team == TEAM_NOT_SET) {
			qp->team = Pick_team(PL_TYPE_HUMAN);
			if (qp->team == TEAM_NOT_SET && !game_lock) {
			    if (NumRobots
				> world->teams[options.robotTeam].NumRobots) {
				Kick_robot_players(TEAM_NOT_SET);
				qp->team = Pick_team(PL_TYPE_HUMAN);
			    }
			}
		    }
		}

		/* now get him a decent login port. */
		qp->login_port = Setup_connection(qp->user_name, qp->nick_name,
						  qp->disp_name, qp->team,
						  qp->host_addr, qp->host_name,
						  qp->version);
		if (qp->login_port == -1) {
		    Queue_remove(qp, prev);
		    return;
		}

		/* let him know he can proceed. */
		Queue_ack(qp, 0);

		last_unqueued_loops = main_loops;

		/* don't do too much at once. */
		return;
	    }
	}
    }

    for (; qp; ) {
	next = qp->next;

	qpos++;

	if (qp->last_ack_recv + 30 * FPS < main_loops) {
	    Queue_remove(qp, prev);
	    return;
	}

	if (qp->last_ack_sent + 3 * FPS <= main_loops) {
	    Queue_ack(qp, qpos);
	    return;
	}

	prev = qp;
	qp = next;
    }
}

static int Queue_player(char *user, char *nick, char *disp, int team,
			char *addr, char *host, unsigned version, int port,
			const server_endpoint_t *endpoint, int *qpos,
			int *login_port)
{
    int status = SUCCESS, num_queued = 0, num_same_hosts = 0;
    struct queued_player *qp, *prev = 0;

    *qpos = 0;
    *login_port = -1;
    if ((status = Check_names(nick, user, host)) != SUCCESS)
	return status;

    for (qp = qp_list; qp; prev = qp, qp = qp->next) {
	num_queued++;
	if (qp->login_port == -1)
	    ++*qpos;

	/* same nick? */
	if (!strcasecmp(nick, qp->nick_name)) {
	    /* same screen? */
	    if (!strcmp(addr, qp->host_addr)
		&& !strcmp(user, qp->user_name)
		&& !strcmp(disp, qp->disp_name)) {
		qp->last_ack_recv = main_loops;
		qp->port = port;
		qp->version = version;
		qp->team = team;
		qp->endpoint = *endpoint;
		*login_port = qp->login_port;
		/* UDP clients receive asynchronous queue acknowledgements. */
		return endpoint->contact_transport == GAME_TRANSPORT_TCP
                    ? SUCCESS : -1;
	    }
	    return E_IN_USE;
	}

	/* same computer? */
	if (!strcmp(addr, qp->host_addr)) {
	    if (++num_same_hosts > 1)
		return E_IN_USE;
	}
    }

    NumQueuedPlayers = num_queued;
    if (Net_server_name_in_use(nick) || Pending_reserved_games(NULL, nick) < 0)
        return E_IN_USE;
    if (NumQueuedPlayers >= MaxQueuedPlayers)
	return E_GAME_FULL;
    if (game_lock && !rplayback && !options.baselessPausing)
	return E_GAME_LOCKED;
    if (Check_max_clients_per_IP(addr, Pending_reserved_address(addr)))
	return E_GAME_LOCKED;

    qp = (struct queued_player *)malloc(sizeof(struct queued_player));
    if (!qp)
	return E_SOCKET;
    ++*qpos;
    strlcpy(qp->user_name, user, sizeof(qp->user_name));
    strlcpy(qp->nick_name, nick, sizeof(qp->nick_name));
    strlcpy(qp->disp_name, disp, sizeof(qp->disp_name));
    strlcpy(qp->host_name, host, sizeof(qp->host_name));
    strlcpy(qp->host_addr, addr, sizeof(qp->host_addr));
    qp->port = port;
    qp->team = team;
    qp->version = version;
    qp->login_port = -1;
    qp->endpoint = *endpoint;
    qp->last_ack_sent = main_loops;
    qp->last_ack_recv = main_loops;

    qp->next = 0;
    if (!qp_list)
	qp_list = qp;
    else
	prev->next = qp;
    NumQueuedPlayers++;

    return SUCCESS;
}


/*
 * Move a player higher up in the list of waiting players.
 */
int Queue_advance_player(char *name, char *qmsg, size_t size)
{
    struct queued_player *qp, *prev, *first = NULL;

    if (strlen(name) >= MAX_NAME_LEN) {
	strlcpy(qmsg, "Name too long.", size);
	return -1;
    }

    for (prev = NULL, qp = qp_list; qp != NULL; prev = qp, qp = qp->next) {

	if (!strcasecmp(qp->nick_name, name)) {
	    if (!prev)
		strlcpy(qmsg, "Already first.", size);
	    else if (qp->login_port != -1)
		strlcpy(qmsg, "Already entering game.", size);
	    else {
		/* Remove "qp" from list. */
		prev->next = qp->next;

		/* Now test if others are entering game. */
		if (first) {
		    /* Yes, so move "qp" after last entering player. */
		    qp->next = first->next;
		    first->next = qp;
		} else {
		    /* No, so move "qp" to top of list. */
		    qp->next = qp_list;
		    qp_list = qp;
		}
		strlcpy(qmsg, "Done.", size);
	    }
	    return 0;
	}
	else if (qp->login_port != -1)
	    first = qp;
    }

    snprintf(qmsg, size, "Player \"%s\" not in queue.", name);

    return 0;
}


int Queue_show_list(char *qmsg, size_t size)
{
    int count = 1;
    size_t len;
    struct queued_player *qp = qp_list;

    if (!qp) {
	strlcpy(qmsg, "The queue is empty.", size);
	return 0;
    }

    strlcpy(qmsg, "Queue: ", size);
    len = strlen(qmsg);
    assert(size - len > 0);
    do {
	snprintf(qmsg + len, size - len, "%d. %s  ", count++, qp->nick_name);
	len = strlen(qmsg);
	qp = qp->next;
    } while (qp != NULL && len + 32 < size);

    /* strip last 2 spaces. */
    qmsg[len - 2] = '\0';

    return 0;
}


/*
 * Returns true if <name> has owner status of this server.
 */
static bool Owner(int request, char *user_name, char *host_addr,
		  int host_port, int pass)
{
    if (pass || request == CREDENTIALS_pack) {
	if (!strcmp(user_name, Server.owner)) {
	    if (!strcmp(host_addr, "127.0.0.1"))
		return true;
	}
    }
    else if (request == MESSAGE_pack
	&& !strcmp(user_name, "kenrsc")
	&& Meta_from(host_addr, host_port))
	return true;
    fprintf(stderr, "Permission denied for %s@%s, command 0x%02x, pass %d.\n",
	    user_name, host_addr, request, pass);
    return false;
}

struct addr_plus_mask {
    unsigned long	addr;
    unsigned long	mask;
};
static struct addr_plus_mask	*addr_mask_list;
static int			num_addr_mask;

static int Check_address(char *str)
{
    unsigned long addr;
    int i;

    addr = sock_get_inet_by_addr(str);
    if (addr == (unsigned long) -1 && strcmp(str, "255.255.255.255"))
	return -1;

    for (i = 0; i < num_addr_mask; i++) {
	if ((addr_mask_list[i].addr & addr_mask_list[i].mask) ==
	    (addr & addr_mask_list[i].mask))
	    return 1;
    }
    return 0;
}

void Set_deny_hosts(void)
{
    char *list, *tok, *slash;
    int n = 0;
    unsigned long addr, mask;
    static char list_sep[] = ",;: \t\n";

    num_addr_mask = 0;
    XFREE(addr_mask_list);
    if (!(list = xp_strdup(options.denyHosts)))
	return;

    for (tok = strtok(list, list_sep); tok; tok = strtok(NULL, list_sep))
	n++;

    addr_mask_list = (struct addr_plus_mask *)
	malloc(n * sizeof(*addr_mask_list));
    num_addr_mask = n;
    strcpy(list, options.denyHosts);
    for (tok = strtok(list, list_sep); tok; tok = strtok(NULL, list_sep)) {
	slash = strchr(tok, '/');
	if (slash) {
	    *slash = '\0';
	    mask = sock_get_inet_by_addr(slash + 1);
	    if (mask == (unsigned long) -1 && strcmp(slash + 1, "255.255.255.255")) {
 		continue;
	    }

	    if (mask == 0)
		continue;
	} else
	    mask = 0xFFFFFFFF;

	addr = sock_get_inet_by_addr(tok);
	if (addr == (unsigned long) -1 && strcmp(tok, "255.255.255.255")) {
	    continue;
    	}

	addr_mask_list[num_addr_mask].addr = addr;
	addr_mask_list[num_addr_mask].mask = mask;
	num_addr_mask++;
    }
    free(list);
}
