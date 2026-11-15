/**
 * @file SessionProtocol.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "network/ByteStream.h"
#include "network/Session.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/**
 * @brief
 *  Session messages between `NetServer` and `NetClient`: each one starts with a `MessageKind` byte.
 */
namespace owl::network::protocol {

/// First bytes of a greeting, "OWLN" read as a little-endian integer.
constexpr uint32_t g_Magic = 0x4e4c574f;

/// Kinds of session messages.
enum struct MessageKind : uint8_t {
	Hello = 1,///< Client to server: protocol, game, version, reconnection token.
	Welcome = 2,///< Server to client: client identifier and reconnection token.
	Reject = 3,///< Server to client: why it refuses.
	Goodbye = 4,///< Server to client: why the session ends.
	Data = 5,///< Either way: a game message.
};

/// A client's greeting.
struct Hello {
	/// Session protocol version.
	uint16_t protocol = g_SessionProtocolVersion;
	/// Name of the game.
	std::string gameId;
	/// Version of the game's messages.
	uint32_t gameVersion = 0;
	/// Reconnection token, 0 for a new client.
	uint64_t token = 0;
};

/**
 * @brief
 *  Encode a greeting.
 * @param[in] iHello The greeting.
 * @return The message.
 */
auto encodeHello(const Hello& iHello) -> std::vector<uint8_t>;

/**
 * @brief
 *  Decode a greeting.
 * @param[in] iBytes The message.
 * @return The greeting, or nothing when the message is not a valid greeting of this protocol family.
 */
auto decodeHello(std::span<const uint8_t> iBytes) -> std::optional<Hello>;

/**
 * @brief
 *  Encode a welcome.
 * @param[in] iClient Identifier given to the client.
 * @param[in] iToken Its reconnection token.
 * @return The message.
 */
auto encodeWelcome(ClientId iClient, uint64_t iToken) -> std::vector<uint8_t>;

/**
 * @brief
 *  Encode a message made of its kind and one reason byte (`Reject`, `Goodbye`).
 * @param[in] iKind The kind.
 * @param[in] iReason The reason.
 * @return The message.
 */
auto encodeReason(MessageKind iKind, uint8_t iReason) -> std::vector<uint8_t>;

/**
 * @brief
 *  Frame a game message.
 * @param[in] iPayload The bytes of the game message.
 * @param[out] oFrame Receives the framed message, reusing its memory.
 */
void encodeData(std::span<const uint8_t> iPayload, std::vector<uint8_t>& oFrame);

}// namespace owl::network::protocol
