/**
 * @file SoundAPI.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "sound/SoundAPI.h"

#include <cstdint>
#include <unordered_map>

/**
 * @brief
 *  Namespace for OpenAL sound management.
 */
namespace owl::sound::openal {
/**
 * @brief
 *  Specialized class for OpenAl sound API.
 */
class OWL_API SoundAPI final : public sound::SoundAPI {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SoundAPI() : sound::SoundAPI{Type::OpenAl} {}

	SoundAPI(const SoundAPI&) = delete;

	SoundAPI(SoundAPI&&) = delete;

	auto operator=(const SoundAPI&) -> SoundAPI& = delete;

	auto operator=(SoundAPI&&) -> SoundAPI& = delete;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~SoundAPI() override;

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	void init() override;

	/**
	 * @brief
	 *  Play a sound (fire-and-forget).
	 * @param[in] iData The Sound to play.
	 */
	void playSound(const shared<SoundData>& iData) override;

	/** @copydoc sound::SoundAPI::play */
	auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle override;

	/** @copydoc sound::SoundAPI::stop */
	void stop(SoundHandle iHandle) override;

	/** @copydoc sound::SoundAPI::pause */
	void pause(SoundHandle iHandle) override;

	/** @copydoc sound::SoundAPI::resume */
	void resume(SoundHandle iHandle) override;

	/** @copydoc sound::SoundAPI::setVolume */
	void setVolume(SoundHandle iHandle, float iVolume) override;

	/** @copydoc sound::SoundAPI::setPitch */
	void setPitch(SoundHandle iHandle, float iPitch) override;

	/** @copydoc sound::SoundAPI::setLoop */
	void setLoop(SoundHandle iHandle, bool iLoop) override;

	/** @copydoc sound::SoundAPI::setPosition */
	void setPosition(SoundHandle iHandle, const math::vec3f& iPosition) override;

	/** @copydoc sound::SoundAPI::setVelocity */
	void setVelocity(SoundHandle iHandle, const math::vec3f& iVelocity) override;

	/** @copydoc sound::SoundAPI::setListenerPosition */
	void setListenerPosition(const math::vec3f& iPosition) override;

	/** @copydoc sound::SoundAPI::setListenerOrientation */
	void setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) override;

	/** @copydoc sound::SoundAPI::setListenerGain */
	void setListenerGain(float iGain) override;

	/** @copydoc sound::SoundAPI::setFilter */
	void setFilter(SoundHandle iHandle, const Filter& iFilter) override;

	/** @copydoc sound::SoundAPI::setEnvironmentSend */
	void setEnvironmentSend(SoundHandle iHandle, bool iSend) override;

	/** @copydoc sound::SoundAPI::setReverb */
	void setReverb(const Reverb& iReverb, float iWet) override;

	/** @copydoc sound::SoundAPI::setListenerVelocity */
	void setListenerVelocity(const math::vec3f& iVelocity) override;

	/** @copydoc sound::SoundAPI::setDopplerFactor */
	void setDopplerFactor(float iFactor) override;

	/** @copydoc sound::SoundAPI::hasSource */
	[[nodiscard]] auto hasSource(SoundHandle iHandle) const -> bool override;

	/** @copydoc sound::SoundAPI::stopAll */
	void stopAll() override;

	/** @copydoc sound::SoundAPI::isPlaying */
	[[nodiscard]] auto isPlaying(SoundHandle iHandle) const -> bool override;

	/**
	 * @brief
	 *  The function to call every frame.
	 * @param iTs The time step.
	 */
	void frame(const core::Timestep& iTs) override;

private:
	/**
	 * @brief
	 *  Delete the sources that finished playing.
	 */
	void releaseStoppedSources();

	/**
	 * @brief
	 *  Make room for a new source when the device has none left: drop the finished ones, else stop the oldest sound
	 *  that does not loop.
	 * @return True when a source was freed.
	 */
	auto reclaimSource() -> bool;

	/**
	 * @brief
	 *  Delete the EFX filter of a source, if it has one.
	 * @param[in] iHandle The sound handle.
	 */
	void releaseFilter(SoundHandle iHandle);

	/// EFX filter of each filtered source, by handle.
	std::unordered_map<SoundHandle, uint32_t> m_handleToFilter;
	/// Mapping from SoundHandle to OpenAL source ID.
	std::unordered_map<SoundHandle, uint32_t> m_handleToSource;
	/// Next handle counter.
	SoundHandle m_nextHandle = 1;
};

}// namespace owl::sound::openal
