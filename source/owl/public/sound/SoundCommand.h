/**
 * @file SoundCommand.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundAPI.h"
#include "core/Core.h"

#include <core/Timestep.h>

namespace owl::sound {
/**
 * @brief
 *  Class gathering the sound's commands.
 */
class OWL_API SoundCommand {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SoundCommand() = default;

	SoundCommand(const SoundCommand&) = delete;

	SoundCommand(SoundCommand&&) = delete;

	auto operator=(const SoundCommand&) -> SoundCommand& = delete;

	auto operator=(SoundCommand&&) -> SoundCommand& = delete;

	/**
	 * @brief
	 *  Default destructor.
	 */
	~SoundCommand() = default;

	/**
	 * @brief
	 *  Create or replace the API base on it type.
	 * @param[in] iType The type of the new render API.
	 */
	static void create(const SoundAPI::Type& iType);

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	static void init() {
		if (m_soundApi)
			m_soundApi->init();
	}

	/**
	 * @brief
	 *  Reset RenderAPI.
	 */
	static void invalidate() { m_soundApi.reset(); }

	/**
	 * @brief
	 *  Get the state of the API.
	 * @return API state.
	 */
	static auto getState() -> SoundAPI::State;

	/**
	 * @brief
	 *  Get the actual API type.
	 * @return API Type.
	 */
	static auto getApi() -> SoundAPI::Type {
		if (m_soundApi)
			return m_soundApi->getApi();
		return static_cast<SoundAPI::Type>(-1);// NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	/**
	 * @brief
	 *  Check if the API type require initializations.
	 * @return True if initialization required.
	 */
	static auto requireInit() -> bool {
		if (m_soundApi)
			return m_soundApi->requireInit();
		return false;
	}

	/**
	 * @brief
	 *  Play a sound (fire-and-forget, backward compatibility).
	 * @param[in] iData The Sound to play.
	 */
	static void playSound(const shared<SoundData>& iData) {
		if (m_soundApi)
			m_soundApi->playSound(iData);
	}

	/**
	 * @brief
	 *  Play a sound with full parameters.
	 * @param[in] iData The Sound to play.
	 * @param[in] iParams The playback parameters.
	 * @return Handle to the sound source for subsequent control.
	 */
	static auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle;

	/**
	 * @brief
	 *  Stop a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	static void stop(SoundHandle iHandle);

	/**
	 * @brief
	 *  Pause a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	static void pause(SoundHandle iHandle);

	/**
	 * @brief
	 *  Resume a paused sound.
	 * @param[in] iHandle The sound handle.
	 */
	static void resume(SoundHandle iHandle);

	/**
	 * @brief
	 *  Set the volume of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVolume The volume gain.
	 */
	static void setVolume(SoundHandle iHandle, float iVolume);

	/**
	 * @brief
	 *  Set the pitch of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPitch The pitch multiplier.
	 */
	static void setPitch(SoundHandle iHandle, float iPitch);

	/**
	 * @brief
	 *  Set whether a sound source loops.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iLoop True to loop.
	 */
	static void setLoop(SoundHandle iHandle, bool iLoop);

	/**
	 * @brief
	 *  Set the 3D position of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPosition The 3D position.
	 */
	static void setPosition(SoundHandle iHandle, const math::vec3f& iPosition);

	/**
	 * @brief
	 *  Set the 3D velocity of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVelocity The 3D velocity.
	 */
	static void setVelocity(SoundHandle iHandle, const math::vec3f& iVelocity);

	/**
	 * @brief
	 *  Set the listener 3D position.
	 * @param[in] iPosition The listener position.
	 */
	static void setListenerPosition(const math::vec3f& iPosition);

	/**
	 * @brief
	 *  Set the listener orientation.
	 * @param[in] iForward The forward direction vector.
	 * @param[in] iUp The up direction vector.
	 */
	static void setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp);

	/**
	 * @brief
	 *  Set the listener gain (master volume).
	 * @param[in] iGain The gain value (0.0 = silent, 1.0 = full volume).
	 */
	static void setListenerGain(float iGain);

	/**
	 * @brief
	 *  Stop all currently playing sounds and release their sources.
	 */
	static void stopAll();

	/**
	 * @brief
	 *  Check if a sound source is currently playing.
	 * @param[in] iHandle The sound handle.
	 * @return True if the source is playing.
	 */
	[[nodiscard]] static auto isPlaying(SoundHandle iHandle) -> bool;

	/**
	 * @brief
	 *  The function to call every frame.
	 * @param iTs The time step.
	 */
	static void frame(const core::Timestep& iTs) {
		if (m_soundApi)
			m_soundApi->frame(iTs);
	}

private:
	/// Pointer to the sound API.
	static uniq<SoundAPI> m_soundApi;
};

}// namespace owl::sound
