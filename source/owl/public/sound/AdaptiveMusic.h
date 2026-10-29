/**
 * @file AdaptiveMusic.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Mixer.h"
#include "SoundHandle.h"
#include "core/Core.h"
#include "core/Macros.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::sound {

/**
 * @brief
 *  One layer of an adaptive music: a looping track played in sync with the others.
 */
struct OWL_API MusicLayer {
	/// Name of the layer, which the states list (`pads`, `drums`...).
	std::string name;
	/// Audio file or `.owlsound` cue, relative to the asset folders.
	std::string asset;
	/// Volume of the layer when a state turns it on.
	float volume{1.f};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two layers.
	 * @param[in] iOther The other layer.
	 * @return True when they match.
	 */
	auto operator==(const MusicLayer& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  One state of an adaptive music (`calm`, `danger`, `combat`): the layers it plays and the stinger heard on entry.
 */
struct OWL_API MusicState {
	/// Name of the state, set by the game (`sound.set_music_state`, a game state key).
	std::string name;
	/// Names of the layers heard in this state; the others fade out.
	std::vector<std::string> layers;
	/// One-shot sound played on the Music bus when the state is entered (empty: none).
	std::string stinger;

	/**
	 * @brief
	 *  Whether the state plays a layer.
	 * @param[in] iLayer The name of the layer.
	 * @return True when the layer is heard.
	 */
	[[nodiscard]] auto hasLayer(std::string_view iLayer) const -> bool;

	/**
	 * @brief
	 *  Compare two states.
	 * @param[in] iOther The other state.
	 * @return True when they match.
	 */
	auto operator==(const MusicState& iOther) const -> bool = default;
};

/**
 * @brief
 *  An adaptive music: layers playing together, and states turning them on and off through the mixer fades.
 */
struct OWL_API AdaptiveMusic {
	/// The layers.
	std::vector<MusicLayer> layers;
	/// The states; the first one is played when no initial state is given.
	std::vector<MusicState> states;
	/// Length of the fades between two states, in seconds.
	float fadeSeconds{2.f};

	/**
	 * @brief
	 *  Find a state by name.
	 * @param[in] iName The name.
	 * @return The state, null when none has this name.
	 */
	[[nodiscard]] auto findState(std::string_view iName) const -> const MusicState*;

	/**
	 * @brief
	 *  Target gain of a layer in a state: its volume when the state plays it, else 0.
	 * @param[in] iState The state.
	 * @param[in] iLayer Index of the layer.
	 * @return The gain.
	 */
	[[nodiscard]] auto getLayerGain(const MusicState& iState, size_t iLayer) const -> float;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two musics.
	 * @param[in] iOther The other music.
	 * @return True when they match.
	 */
	auto operator==(const AdaptiveMusic& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Plays an adaptive music on the Music bus of a mixer: every layer loops from the start, and a state change fades
 *  each layer to its gain in the new state.
 */
class OWL_API AdaptiveMusicPlayer final {
public:
	/// Function starting a sound (the asset, its parameters), returning its handle.
	using PlayFunction = std::function<SoundHandle(const std::string& iAsset, const PlayParams& iParams)>;

	/**
	 * @brief
	 *  Default constructor.
	 */
	AdaptiveMusicPlayer() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	AdaptiveMusicPlayer(const AdaptiveMusicPlayer&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	AdaptiveMusicPlayer(AdaptiveMusicPlayer&&) = default;

	/**
	 * @brief
	 *  Copy assignment.
	 * @return This player.
	 */
	auto operator=(const AdaptiveMusicPlayer&) -> AdaptiveMusicPlayer& = default;

	/**
	 * @brief
	 *  Move assignment.
	 * @return This player.
	 */
	auto operator=(AdaptiveMusicPlayer&&) -> AdaptiveMusicPlayer& = default;

	/**
	 * @brief
	 *  Destructor (the sounds are not stopped: call stop).
	 */
	~AdaptiveMusicPlayer() = default;

	/**
	 * @brief
	 *  Start every layer, silent, then fade in the layers of a state.
	 * @param[in] iMusic The music.
	 * @param[in] iState The state to start in (empty or unknown: the first one).
	 * @param[in,out] ioMixer The mixer.
	 * @param[in] iPlay The function starting a sound.
	 * @return True when at least one layer started.
	 */
	auto start(const AdaptiveMusic& iMusic, std::string_view iState, Mixer& ioMixer, const PlayFunction& iPlay) -> bool;

	/**
	 * @brief
	 *  Switch to a state: fade the layers to their gains in it and play its stinger.
	 * @param[in] iState The name of the state.
	 * @param[in,out] ioMixer The mixer.
	 * @param[in] iPlay The function starting the stinger.
	 * @return True when the state exists (already being in it is a success).
	 */
	auto setState(std::string_view iState, Mixer& ioMixer, const PlayFunction& iPlay) -> bool;

	/**
	 * @brief
	 *  Fade out and stop every layer.
	 * @param[in,out] ioMixer The mixer.
	 * @param[in] iSeconds Length of the fade (0: at once).
	 */
	void stop(Mixer& ioMixer, float iSeconds);

	/**
	 * @brief
	 *  Whether the music plays.
	 * @return True between start and stop.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return m_running; }

	/**
	 * @brief
	 *  Name of the current state.
	 * @return The state, empty when not running.
	 */
	[[nodiscard]] auto getState() const -> const std::string& { return m_state; }

	/**
	 * @brief
	 *  Handles of the layers, in the order of the music (invalid for a layer that did not start).
	 * @return The handles.
	 */
	[[nodiscard]] auto getLayerHandles() const -> const std::vector<SoundHandle>& { return m_layers; }

	/**
	 * @brief
	 *  Handle of the last stinger played.
	 * @return The handle, invalidSoundHandle when none was played.
	 */
	[[nodiscard]] auto getLastStinger() const -> SoundHandle { return m_stinger; }

private:
	/// The music played.
	AdaptiveMusic m_music;
	/// Handle of each layer.
	std::vector<SoundHandle> m_layers;
	/// Current state.
	std::string m_state;
	/// Last stinger played.
	SoundHandle m_stinger{invalidSoundHandle};
	/// Whether the music plays.
	bool m_running{false};
};

}// namespace owl::sound
