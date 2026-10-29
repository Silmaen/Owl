/**
 * @file SoundAPI.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "sound/SoundAPI.h"

#include <cstddef>
#include <unordered_map>

/**
 * @brief
 *  Namespace for null sound management.
 */
namespace owl::sound::null {

/**
 * @brief
 *  State of a source of the Null backend: what an audible backend would play.
 */
struct NullSource {
	/// Parameters given at play, then changed by the setters (volume, pitch, loop, position, velocity, filter).
	PlayParams params;
	/// Whether the source is paused.
	bool paused{false};
	/// Whether the source feeds the reverb of the environment.
	bool environmentSend{false};
};

/**
 * @brief
 *  Specialized class for null sound API: silent, it keeps the state of every source and of the listener so the
 *  mixing logic can be checked headless.
 *
 * A one-shot source is released at the next frame, a looping one when stopped. `isPlaying` is always false.
 */
class OWL_API SoundAPI final : public sound::SoundAPI {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SoundAPI() : sound::SoundAPI{Type::Null} {}

	SoundAPI(const SoundAPI&) = delete;

	SoundAPI(SoundAPI&&) = delete;

	auto operator=(const SoundAPI&) -> SoundAPI& = delete;

	auto operator=(SoundAPI&&) -> SoundAPI& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~SoundAPI() override;

	/**
	 * @brief
	 *  Initialize the sound API.
	 */
	void init() override;

	/**
	 * @brief
	 *  Play a sound (fire-and-forget).
	 * @param[in] iData The Sound to play.
	 */
	void playSound(const shared<SoundData>& iData) override;

	/**
	 * @brief
	 *  Play a sound with full parameters.
	 * @param[in] iData The Sound to play.
	 * @param[in] iParams The playback parameters.
	 * @return Handle to the sound source.
	 */
	auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle override;

	/**
	 * @brief
	 *  Stop a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	void stop(SoundHandle iHandle) override;

	/**
	 * @brief
	 *  Pause a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	void pause(SoundHandle iHandle) override;

	/**
	 * @brief
	 *  Resume a paused sound.
	 * @param[in] iHandle The sound handle.
	 */
	void resume(SoundHandle iHandle) override;

	/**
	 * @brief
	 *  Set the volume of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVolume The volume gain.
	 */
	void setVolume(SoundHandle iHandle, float iVolume) override;

	/**
	 * @brief
	 *  Set the pitch of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPitch The pitch multiplier.
	 */
	void setPitch(SoundHandle iHandle, float iPitch) override;

	/**
	 * @brief
	 *  Set whether a sound source loops.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iLoop True to loop.
	 */
	void setLoop(SoundHandle iHandle, bool iLoop) override;

	/**
	 * @brief
	 *  Set the 3D position of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPosition The 3D position.
	 */
	void setPosition(SoundHandle iHandle, const math::vec3f& iPosition) override;

	/**
	 * @brief
	 *  Set the 3D velocity of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVelocity The 3D velocity.
	 */
	void setVelocity(SoundHandle iHandle, const math::vec3f& iVelocity) override;

	/**
	 * @brief
	 *  Set the listener 3D position.
	 * @param[in] iPosition The listener position.
	 */
	void setListenerPosition(const math::vec3f& iPosition) override;

	/**
	 * @brief
	 *  Set the listener orientation.
	 * @param[in] iForward The forward direction vector.
	 * @param[in] iUp The up direction vector.
	 */
	void setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) override;

	/**
	 * @brief
	 *  Set the listener gain.
	 * @param[in] iGain The gain.
	 */
	void setListenerGain(float iGain) override { m_listenerGain = iGain; }

	/**
	 * @brief
	 *  Set the direct filter of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iFilter The filter.
	 */
	void setFilter(SoundHandle iHandle, const Filter& iFilter) override;

	/**
	 * @brief
	 *  Set whether a sound source feeds the reverb of the environment.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iSend True to send it to the reverb.
	 */
	void setEnvironmentSend(SoundHandle iHandle, bool iSend) override;

	/**
	 * @brief
	 *  Set the reverb of the environment.
	 * @param[in] iReverb The reverb parameters.
	 * @param[in] iWet Gain of the reverb.
	 */
	void setReverb(const Reverb& iReverb, float iWet) override;

	/**
	 * @brief
	 *  Set the listener velocity.
	 * @param[in] iVelocity The velocity.
	 */
	void setListenerVelocity(const math::vec3f& iVelocity) override { m_listenerVelocity = iVelocity; }

	/**
	 * @brief
	 *  Set the scale of the Doppler effect.
	 * @param[in] iFactor The Doppler factor.
	 */
	void setDopplerFactor(const float iFactor) override { m_dopplerFactor = iFactor; }

	/**
	 * @brief
	 *  Whether a handle still has a source.
	 * @param[in] iHandle The sound handle.
	 * @return True when the source exists.
	 */
	[[nodiscard]] auto hasSource(SoundHandle iHandle) const -> bool override;

	/**
	 * @brief
	 *  Stop all playing sounds and release their sources.
	 */
	void stopAll() override { m_sources.clear(); }

	/**
	 * @brief
	 *  Check if a sound source is currently playing (never, the backend is silent).
	 * @param[in] iHandle The sound handle.
	 * @return False.
	 */
	[[nodiscard]] auto isPlaying(SoundHandle iHandle) const -> bool override;

	/**
	 * @brief
	 *  The function to call every frame: releases the one-shot sources.
	 * @param iTs The time step.
	 */
	void frame(const core::Timestep& iTs) override;

	/**
	 * @brief
	 *  State of a source.
	 * @param[in] iHandle The sound handle.
	 * @return The source, null when the handle has none.
	 */
	[[nodiscard]] auto getSource(SoundHandle iHandle) const -> const NullSource*;

	/**
	 * @brief
	 *  Number of sources alive.
	 * @return The count.
	 */
	[[nodiscard]] auto getSourceCount() const -> size_t { return m_sources.size(); }

	/**
	 * @brief
	 *  Listener position last set.
	 * @return The position.
	 */
	[[nodiscard]] auto getListenerPosition() const -> const math::vec3f& { return m_listenerPosition; }

	/**
	 * @brief
	 *  Listener velocity last set.
	 * @return The velocity.
	 */
	[[nodiscard]] auto getListenerVelocity() const -> const math::vec3f& { return m_listenerVelocity; }

	/**
	 * @brief
	 *  Listener gain last set.
	 * @return The gain.
	 */
	[[nodiscard]] auto getListenerGain() const -> float { return m_listenerGain; }

	/**
	 * @brief
	 *  Doppler factor last set.
	 * @return The factor.
	 */
	[[nodiscard]] auto getDopplerFactor() const -> float { return m_dopplerFactor; }

	/**
	 * @brief
	 *  Reverb of the environment last set.
	 * @return The reverb.
	 */
	[[nodiscard]] auto getReverb() const -> const Reverb& { return m_reverb; }

	/**
	 * @brief
	 *  Gain of the reverb last set.
	 * @return The wet gain.
	 */
	[[nodiscard]] auto getReverbWet() const -> float { return m_reverbWet; }

private:
	/// Next handle to return.
	SoundHandle m_nextHandle = 1;
	/// Sources alive, by handle.
	std::unordered_map<SoundHandle, NullSource> m_sources;
	/// Listener position.
	math::vec3f m_listenerPosition{0.f, 0.f, 0.f};
	/// Listener velocity.
	math::vec3f m_listenerVelocity{0.f, 0.f, 0.f};
	/// Listener gain.
	float m_listenerGain{1.f};
	/// Doppler factor.
	float m_dopplerFactor{1.f};
	/// Reverb of the environment.
	Reverb m_reverb;
	/// Gain of the reverb.
	float m_reverbWet{0.f};
};

}// namespace owl::sound::null
