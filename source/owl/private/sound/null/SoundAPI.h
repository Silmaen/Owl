/**
 * @file SoundAPI.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "sound/SoundAPI.h"

/**
 * @brief Namespace for null sound management.
 */
namespace owl::sound::null {

/**
 * @brief Specialized class for null sound API.
 */
class OWL_API SoundAPI final : public sound::SoundAPI {
public:
	/**
	 * @brief Default constructor.
	 */
	SoundAPI() : sound::SoundAPI{Type::Null} {}
	SoundAPI(const SoundAPI&) = delete;
	SoundAPI(SoundAPI&&) = delete;
	auto operator=(const SoundAPI&) -> SoundAPI& = delete;
	auto operator=(SoundAPI&&) -> SoundAPI& = delete;
	/**
	 * @brief Default destructor.
	 */
	~SoundAPI() override;
	/**
	 * @brief Initialize the renderer.
	 */
	void init() override;

	/**
	 * @brief Play a sound.
	 * @param[in] iData The Sound to play.
	 */
	void playSound(const shared<SoundData>& iData) override;

	/**
	 * @brief The function to call every frame.
	 * @param iTs The time step.
	 */
	void frame(const core::Timestep& iTs) override;

private:
};

}// namespace owl::sound::null
