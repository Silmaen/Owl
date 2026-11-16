/**
 * @file LockstepBuffer.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "input/ActionFrame.h"
#include "network/ByteStream.h"
#include "network/Match.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace owl::network {

/// The actions of one player for one tick, in the order of the game's action map.
using PlayerInput = std::vector<input::ActionState>;

/**
 * @brief
 *  Write the actions of a player for one tick.
 * @param[in,out] ioWriter The writer.
 * @param[in] iInput The actions.
 */
OWL_API void writePlayerInput(ByteWriter& ioWriter, const PlayerInput& iInput);

/**
 * @brief
 *  Read the actions of a player for one tick.
 * @param[in,out] ioReader The reader.
 * @return The actions, none when the bytes are truncated.
 */
[[nodiscard]] OWL_API auto readPlayerInput(ByteReader& ioReader) -> std::optional<PlayerInput>;

/**
 * @brief
 *  The inputs of every player of a lockstep match, by tick: a tick plays once every player's input for it is known.
 */
class OWL_API LockstepBuffer final {
public:
	/**
	 * @brief
	 *  Start a match: the players, and the empty inputs of the first ticks (the input delay).
	 * @param[in] iPlayers The players of the match.
	 * @param[in] iDelay Ticks between the sampling of an input and its tick.
	 */
	void reset(std::span<const PlayerId> iPlayers, uint32_t iDelay);

	/**
	 * @brief
	 *  Remove a player who left: the ticks no longer wait for its inputs.
	 * @param[in] iPlayer The player.
	 */
	void removePlayer(PlayerId iPlayer);

	/**
	 * @brief
	 *  Keep the input of a player for a tick (a second one for the same tick is ignored).
	 * @param[in] iTick The tick.
	 * @param[in] iPlayer The player.
	 * @param[in] iInput The actions.
	 */
	void put(uint64_t iTick, PlayerId iPlayer, const PlayerInput& iInput);

	/**
	 * @brief
	 *  Whether every player's input of a tick is known.
	 * @param[in] iTick The tick.
	 * @return True when the tick can play.
	 */
	[[nodiscard]] auto isComplete(uint64_t iTick) const -> bool;

	/**
	 * @brief
	 *  Number of the ticks that can play from the start of the match: every tick below it is complete.
	 * @param[in] iFrom A tick known to be complete or the first one missing, to resume the search.
	 * @return The first tick that is not complete.
	 */
	[[nodiscard]] auto getPlayableEnd(uint64_t iFrom) const -> uint64_t;

	/**
	 * @brief
	 *  Input of a player for a tick.
	 * @param[in] iTick The tick.
	 * @param[in] iPlayer The player.
	 * @return The actions, nullptr when unknown.
	 */
	[[nodiscard]] auto get(uint64_t iTick, PlayerId iPlayer) const -> const PlayerInput*;

	/**
	 * @brief
	 *  Forget the ticks before one, played already.
	 * @param[in] iTick The first tick to keep.
	 */
	void discardBefore(uint64_t iTick);

	/**
	 * @brief
	 *  The players the ticks wait for.
	 * @return The players.
	 */
	[[nodiscard]] auto getPlayers() const -> const std::vector<PlayerId>& { return m_players; }

private:
	/// Players the ticks wait for.
	std::vector<PlayerId> m_players;
	/// Inputs by tick, then by player.
	std::map<uint64_t, std::map<PlayerId, PlayerInput>> m_inputs;
	/// Ticks before this one were discarded.
	uint64_t m_firstKept = 0;
};

}// namespace owl::network
