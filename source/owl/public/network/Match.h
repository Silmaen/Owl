/**
 * @file Match.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace owl::network {

/// Number of a player of a match: 0 for the host, the `ClientId` of a client.
using PlayerId = uint32_t;

/// Player number of the host, the player playing on the server.
constexpr PlayerId g_HostPlayer = 0;

/// How a player plays, known to the lobby (matchmaking filter, crossplay).
enum struct InputType : uint8_t {
	KeyboardMouse,///< Keyboard and mouse.
	Gamepad,///< A gamepad.
	Touch,///< A touch screen.
};

/// How the peers of a match share the game.
enum struct NetModel : uint8_t {
	Authoritative,///< The server simulates and sends snapshots; clients interpolate and predict (action games).
	Lockstep,///< Every peer simulates every tick from the inputs of all players (strategy, management games).
};

/**
 * @brief
 *  What a machine brings to a match, or the minimum a match asks for.
 */
struct OWL_API HardwareProfile {
	/// Hardware threads of the processor; as a minimum, 0 for none.
	uint32_t cpuThreads = 0;
	/// Mean duration of a frame, in milliseconds; as a minimum, the slowest frame allowed, 0 for none.
	float frameMs = 0.f;

	/**
	 * @brief
	 *  Whether this profile reaches a minimum.
	 * @param[in] iMinimum The minimum profile.
	 * @return True when every bound of the minimum is met.
	 */
	[[nodiscard]] auto meets(const HardwareProfile& iMinimum) const -> bool;

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other profile.
	 * @return True when both hold the same values.
	 */
	auto operator==(const HardwareProfile& iOther) const -> bool;
};

/**
 * @brief
 *  The rules of the matches of a game, from the project (`owl_project.yml`, `network:`).
 */
struct OWL_API MatchRules {
	/// How the peers share the game.
	NetModel model = NetModel::Authoritative;
	/// Most players of a match, the host included.
	uint32_t maxPlayers = 8;
	/// Whether players of every input type play together; off, the server refuses those not playing like the host.
	bool crossplay = true;
	/// The machine a player needs, checked when the match starts.
	HardwareProfile minimum;
	/// Lockstep: ticks between the sampling of an input and the tick it plays in (hides the latency).
	uint32_t inputDelay = 3;
	/// Lockstep: ticks between two world hash comparisons (desync detection).
	uint32_t hashInterval = 60;
	/// Authoritative: snapshots sent to each client per second.
	float snapshotRate = 20.f;
	/// Authoritative: how far in the past a client shows the received entities, in seconds.
	float interpolationDelay = 0.1f;
	/// Authoritative: most entities in a snapshot (interest management keeps the nearest and most important).
	uint32_t snapshotBudget = 64;
	/// Authoritative: seconds of hit sphere history the server keeps for the lag compensation.
	float lagCompensationWindow = 1.f;
	/// Authoritative: distance between a predicted entity and the server's one above which the client corrects it.
	float correctionThreshold = 0.25f;

	/**
	 * @brief
	 *  The rules with every value within its range.
	 * @return The clamped rules.
	 */
	[[nodiscard]] auto clamped() const -> MatchRules;

	/**
	 * @brief
	 *  The rules as a YAML map (the `network:` block of `owl_project.yml`, `Network:` of `runner.yml`).
	 * @return The YAML text.
	 */
	[[nodiscard]] auto toYaml() const -> std::string;

	/**
	 * @brief
	 *  Read rules from a YAML map; a missing key keeps its default, an invalid text gives the defaults.
	 * @param[in] iYaml The YAML text.
	 * @return The clamped rules.
	 */
	[[nodiscard]] static auto fromYaml(std::string_view iYaml) -> MatchRules;

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other rules.
	 * @return True when both hold the same values.
	 */
	auto operator==(const MatchRules& iOther) const -> bool;
};

/**
 * @brief
 *  A player of a lobby or of a match.
 */
struct OWL_API PlayerInfo {
	/// Player number.
	PlayerId id = g_HostPlayer;
	/// Name chosen by the player.
	std::string name;
	/// Whether the player is ready to start.
	bool ready = false;
	/// How the player plays.
	InputType inputType = InputType::KeyboardMouse;
	/// The player's machine.
	HardwareProfile profile;
	/// Whether the player's link is up (false while it is being restored).
	bool connected = true;
};

/**
 * @brief
 *  Version number of a game's messages from its version text, the same on every target (FNV-1a).
 * @param[in] iVersion The version text of the project (`1.2.0`).
 * @return The number a server compares.
 */
[[nodiscard]] OWL_API auto getVersionNumber(std::string_view iVersion) -> uint32_t;

/// Why the server refused a player in its lobby.
enum struct RefuseReason : uint8_t {
	MatchFull,///< The match has as many players as its rules allow.
	InputType,///< Crossplay is off and the player does not play like the host.
	MatchStarted,///< The match has started.
};

/// Why a match did not start.
enum struct StartError : uint8_t {
	NotHost,///< Only the host starts a match.
	NotInLobby,///< The session has no lobby, or its match has started.
	NotReady,///< A player is not ready.
	BelowMinimum,///< A player's machine does not reach the minimum profile.
	NoScene,///< No scene to play was given.
};

}// namespace owl::network
