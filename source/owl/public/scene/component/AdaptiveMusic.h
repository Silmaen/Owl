/**
 * @file AdaptiveMusic.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "sound/AdaptiveMusic.h"

#include <string>

namespace owl::scene::component {

/**
 * @brief
 *  Adaptive music of a scene: layers looping together on the Music bus and states (calm, danger, combat) fading them
 *  in and out.
 *
 * The state changes through `sound.set_music_state` in Lua or, when `stateKey` is set, by following the value of that
 * key of the game state.
 */
struct OWL_API AdaptiveMusic {
	/// Layers, states and fade length.
	sound::AdaptiveMusic music;
	/// State the music starts in (empty: the first one).
	std::string initialState;
	/// Game state key whose text value selects the state (empty: Lua only).
	std::string stateKey;
	/// Whether the music starts with the scene.
	bool playOnStart = true;

	/// Runtime player (not serialized, shared by the copies of the component).
	shared<sound::AdaptiveMusicPlayer> runtimePlayer;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Adaptive Music"; }

	/**
	 * @brief
	 *  Key of the component in the files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "AdaptiveMusic"; }

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut The serializer.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component.
	 * @param[in] iNode The serializer.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
