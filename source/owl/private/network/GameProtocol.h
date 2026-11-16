/**
 * @file GameProtocol.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "network/ByteStream.h"
#include "script/CallValue.h"

#include <cstdint>
#include <optional>

namespace owl::network::protocol {

/// Lobby, match control and replicated spawns: reliable.
constexpr uint8_t g_ControlChannel = 0;
/// Remote procedure calls, reliable or not.
constexpr uint8_t g_RpcChannel = 1;
/// Snapshots and owned states: unreliable.
constexpr uint8_t g_StateChannel = 2;
/// Lockstep inputs: reliable.
constexpr uint8_t g_LockstepChannel = 3;

/// Most arguments of a remote procedure call.
constexpr uint8_t g_MaxRpcArgs = 32;

/// First byte of every game session message.
enum struct Message : uint8_t {
	Hello = 1,///< Client to host: name, input type, machine, ready.
	Roster = 2,///< Host to clients: the lobby.
	Refused = 3,///< Host to a client before it is kicked: the reason.
	Start = 4,///< Host to clients: the scene, the seed and the model of the match.
	Owner = 5,///< Host to clients: the new owner of an entity.
	Spawn = 6,///< Host to a client: an entity created at run time.
	Despawn = 7,///< Host to a client: an entity gone, or out of its interest.
	ComponentState = 8,///< Host to a client: a replicated component that changed.
	Hash = 9,///< Client to host: the world hash of a lockstep tick.
	Desync = 10,///< Host to clients: the tick whose world hashes differ.
	Rpc = 20,///< A remote procedure call.
	Snapshot = 30,///< Host to a client: transforms of the entities it receives.
	OwnerState = 31,///< Client to host: transforms of the entities it decides.
	Input = 40,///< Lockstep input of a player for a tick.
};

/**
 * @brief
 *  Write a script value.
 * @param[in,out] ioWriter The writer.
 * @param[in] iValue The value.
 */
void writeValue(ByteWriter& ioWriter, const script::CallValue& iValue);

/**
 * @brief
 *  Read a script value.
 * @param[in,out] ioReader The reader.
 * @return The value, none when the bytes are truncated or of an unknown kind.
 */
[[nodiscard]] auto readValue(ByteReader& ioReader) -> std::optional<script::CallValue>;

/**
 * @brief
 *  Write a double, by its bits.
 * @param[in,out] ioWriter The writer.
 * @param[in] iValue The value.
 */
void writeF64(ByteWriter& ioWriter, double iValue);

/**
 * @brief
 *  Read a double, by its bits.
 * @param[in,out] ioReader The reader.
 * @return The value, none when the bytes are truncated.
 */
[[nodiscard]] auto readF64(ByteReader& ioReader) -> std::optional<double>;

}// namespace owl::network::protocol
