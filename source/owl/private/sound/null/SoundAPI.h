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

	/// @copydoc sound::SoundAPI::play
	auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle override;
	/// @copydoc sound::SoundAPI::stop
	void stop(SoundHandle iHandle) override;
	/// @copydoc sound::SoundAPI::pause
	void pause(SoundHandle iHandle) override;
	/// @copydoc sound::SoundAPI::resume
	void resume(SoundHandle iHandle) override;
	/// @copydoc sound::SoundAPI::setVolume
	void setVolume(SoundHandle iHandle, float iVolume) override;
	/// @copydoc sound::SoundAPI::setPitch
	void setPitch(SoundHandle iHandle, float iPitch) override;
	/// @copydoc sound::SoundAPI::setLoop
	void setLoop(SoundHandle iHandle, bool iLoop) override;
	/// @copydoc sound::SoundAPI::setPosition
	void setPosition(SoundHandle iHandle, const math::vec3f& iPosition) override;
	/// @copydoc sound::SoundAPI::setVelocity
	void setVelocity(SoundHandle iHandle, const math::vec3f& iVelocity) override;
	/// @copydoc sound::SoundAPI::setListenerPosition
	void setListenerPosition(const math::vec3f& iPosition) override;
	/// @copydoc sound::SoundAPI::setListenerOrientation
	void setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) override;
	/// @copydoc sound::SoundAPI::setListenerGain
	void setListenerGain(float) override {}
	/// @copydoc sound::SoundAPI::stopAll
	void stopAll() override {}
	/// @copydoc sound::SoundAPI::isPlaying
	[[nodiscard]] auto isPlaying(SoundHandle iHandle) const -> bool override;

	/**
	 * @brief The function to call every frame.
	 * @param iTs The time step.
	 */
	void frame(const core::Timestep& iTs) override;

private:
	/// Next handle to return.
	SoundHandle m_nextHandle = 1;
};

}// namespace owl::sound::null
