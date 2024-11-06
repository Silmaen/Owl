/**
 * @file SoundCommand.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundAPI.h"
#include "core/Core.h"

namespace owl::sound {

/**
 * @brief Class SoundCommand.
 */
class OWL_API SoundCommand {
public:
	/**
	 * @brief Default constructor.
	 */
	SoundCommand() = default;
	SoundCommand(const SoundCommand&) = delete;
	SoundCommand(SoundCommand&&) = delete;
	auto operator=(const SoundCommand&) -> SoundCommand& = delete;
	auto operator=(SoundCommand&&) -> SoundCommand& = delete;
	/**
	 * @brief Default destructor.
	 */
	~SoundCommand() = default;
	/**
	 * @brief Create or replace the API base on it type.
	 * @param[in] iType The type of the new render API.
	 */
	static void create(const SoundAPI::Type& iType);

	/**
	 * @brief Initialize the renderer.
	 */
	static void init() {
		if (m_soundAPI)
			m_soundAPI->init();
	}

	/**
	 * @brief Reset RenderAPI.
	 */
	static void invalidate() { m_soundAPI.reset(); }

	/**
	 * @brief Get the state of the API.
	 * @return API state.
	 */
	static auto getState() -> SoundAPI::State;

	/**
	 * @brief Get the actual API type.
	 * @return API Type.
	 */
	static auto getApi() -> SoundAPI::Type {
		if (m_soundAPI)
			return m_soundAPI->getApi();
		// NOLINTBEGIN(clang-analyzer-optin.core.EnumCastOutOfRange)
		return static_cast<SoundAPI::Type>(-1);
		// NOLINTEND(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	/**
	 * @brief Check if the API type require initializations.
	 * @return True if initialization required.
	 */
	static auto requireInit() -> bool {
		if (m_soundAPI)
			return m_soundAPI->requireInit();
		return false;
	}
	/**
	 * @brief Play a sound.
	 * @param[in] iData The Sound to play.
	 */
	static void playSound(const shared<SoundData>& iData) {
		if (m_soundAPI)
			return m_soundAPI->playSound(iData);
	}

private:
	/// Pointer to the render API
	static uniq<SoundAPI> m_soundAPI;
};

}// namespace owl::sound
