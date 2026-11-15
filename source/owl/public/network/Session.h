/**
 * @file Session.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <string>

namespace owl::network {

/// Identifier of a client of a server, kept across a reconnection.
using ClientId = uint32_t;

/// Version of the session protocol (handshake and message framing); a client of another version is rejected.
constexpr uint16_t g_SessionProtocolVersion = 1;

/// What a server and its clients agree on before exchanging game messages.
struct SessionConfig {
	/// Name of the game: a client of another game is rejected.
	std::string gameId;
	/// Version of the game's own messages: a client of another version is rejected.
	uint32_t gameVersion = 0;
	/// Maximum number of clients of a server, suspended ones included.
	uint32_t maxClients = 16;
	/// Time a client must greet the server once connected, in seconds.
	float handshakeTimeout = 5.f;
	/// Time a lost client keeps its place on the server, and keeps trying to reconnect, in seconds.
	float reconnectGrace = 10.f;
	/// Time between two reconnection attempts of a client, in seconds.
	float reconnectInterval = 1.f;
};

/// Why a server refused a client.
enum struct RejectReason : uint8_t {
	WrongProtocol,///< Another session protocol version.
	WrongGame,///< Another game or game version.
	ServerFull,///< No free client slot.
	UnknownSession,///< A reconnection whose session expired.
};

/// Why a client left a server.
enum struct LeaveReason : uint8_t {
	Left,///< The client or the server closed the session.
	TimedOut,///< The link was lost and not restored within the grace time.
	Kicked,///< The server removed the client.
	Rejected,///< The server refused the client during the handshake.
	Unreachable,///< The server never answered.
};

}// namespace owl::network
