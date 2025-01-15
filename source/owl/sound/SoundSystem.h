/**
 * @file SoundSystem.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundCommand.h"

#include <core/assets/AssetLibrary.h>

namespace owl::sound {

/**
 * @brief Class SoundSystem.
 */
class OWL_API SoundSystem {
public:
	using soundLibrary = core::assets::AssetLibrary<SoundData>;
	/**
	 * @brief Default constructor.
	 */
	SoundSystem() = default;
	SoundSystem(const SoundSystem&) = delete;
	SoundSystem(SoundSystem&&) = delete;
	auto operator=(const SoundSystem&) -> SoundSystem& = delete;
	auto operator=(SoundSystem&&) -> SoundSystem& = delete;
	/**
	 * @brief Default destructor.
	 */
	~SoundSystem() = default;
	/**
		 * @brief Initialize the renderer.
		 */
	static void init();

	/**
	 * @brief Stops the renderer.
	 */
	static void shutdown();

	/**
	 * @brief Reset the renderer.
	 */
	static void reset();

	/**
	 * @brief The state of the renderer.
	 */
	enum struct State : uint8_t {
		Created,///< Renderer just created.
		Running,///< Renderer in stable state.
		Stopped,///< Renderer stopped.
		Error,///< Render has an error.
	};

	static auto getState() -> State { return m_internalState; }
	/**
	 * @brief Access to the sound Library.
	 * @return The sound library.
	 */
	static auto getSoundLibrary() -> soundLibrary& { return *m_soundLibrary; }

private:
	/// The state of the renderer.
	static State m_internalState;

	/// The sound's library.
	static shared<soundLibrary> m_soundLibrary;
};

}// namespace owl::sound
