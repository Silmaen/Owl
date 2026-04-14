/**
 * @file SoundAPI.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundData.h"
#include "SoundHandle.h"

#include "core/Timestep.h"

namespace owl::sound {

/**
 * @brief Abstract class for sound API.
 */
class OWL_API SoundAPI {
public:
	/// Render API types.
	enum struct Type : uint8_t {
		Null = 0,///< Null Sound system.
		OpenAl = 1,///< OpenAL Sound system.
	};
	/**
	 * @brief Default constructor.
	 */
	explicit SoundAPI(const Type& iType) : m_type{iType} {}
	SoundAPI(const SoundAPI&) = delete;
	SoundAPI(SoundAPI&&) = delete;
	auto operator=(const SoundAPI&) -> SoundAPI& = delete;
	auto operator=(SoundAPI&&) -> SoundAPI& = delete;
	/**
	 * @brief Default destructor.
	 */
	virtual ~SoundAPI();

	/**
	 * @brief Initialize the renderer.
	 */
	virtual void init() = 0;

	/// Sound API states.
	enum struct State : uint8_t {
		Created,///< Just created.
		Ready,///< Ready to work.
		Error///< in error.
	};

	/**
	 * @brief Get the actual API type.
	 * @return API Type.
	 */
	[[nodiscard]] auto getApi() const -> Type { return m_type; }

	/**
	 * @brief Static method to create a Render API.
	 * @param[in] iType Type of API.
	 * @return Render.
	 */
	static auto create(const Type& iType) -> uniq<SoundAPI>;

	/**
	 * @brief Get the actual API state.
	 * @return API State.
	 */
	[[nodiscard]] auto getState() const -> State { return m_state; }

	/**
	 * @brief Check if the API type require initializations.
	 * @return tRue if initialization required.
	 */
	[[nodiscard]] auto requireInit() const -> bool { return m_type == Type::OpenAl; }

	/**
	 * @brief Play a sound (fire-and-forget, backward compatibility).
	 * @param[in] iData The Sound to play.
	 */
	virtual void playSound(const shared<SoundData>& iData) = 0;

	/**
	 * @brief Play a sound with full parameters.
	 * @param[in] iData The Sound to play.
	 * @param[in] iParams The playback parameters.
	 * @return Handle to the sound source for subsequent control.
	 */
	virtual auto play(const shared<SoundData>& iData, const PlayParams& iParams) -> SoundHandle = 0;

	/**
	 * @brief Stop a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	virtual void stop(SoundHandle iHandle) = 0;

	/**
	 * @brief Pause a playing sound.
	 * @param[in] iHandle The sound handle.
	 */
	virtual void pause(SoundHandle iHandle) = 0;

	/**
	 * @brief Resume a paused sound.
	 * @param[in] iHandle The sound handle.
	 */
	virtual void resume(SoundHandle iHandle) = 0;

	/**
	 * @brief Set the volume of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVolume The volume gain [0..∞).
	 */
	virtual void setVolume(SoundHandle iHandle, float iVolume) = 0;

	/**
	 * @brief Set the pitch of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPitch The pitch multiplier (> 0).
	 */
	virtual void setPitch(SoundHandle iHandle, float iPitch) = 0;

	/**
	 * @brief Set whether a sound source loops.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iLoop True to loop.
	 */
	virtual void setLoop(SoundHandle iHandle, bool iLoop) = 0;

	/**
	 * @brief Set the 3D position of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iPosition The 3D position.
	 */
	virtual void setPosition(SoundHandle iHandle, const math::vec3f& iPosition) = 0;

	/**
	 * @brief Set the 3D velocity of a sound source.
	 * @param[in] iHandle The sound handle.
	 * @param[in] iVelocity The 3D velocity.
	 */
	virtual void setVelocity(SoundHandle iHandle, const math::vec3f& iVelocity) = 0;

	/**
	 * @brief Set the listener 3D position.
	 * @param[in] iPosition The listener position.
	 */
	virtual void setListenerPosition(const math::vec3f& iPosition) = 0;

	/**
	 * @brief Set the listener orientation.
	 * @param[in] iForward The forward direction vector.
	 * @param[in] iUp The up direction vector.
	 */
	virtual void setListenerOrientation(const math::vec3f& iForward, const math::vec3f& iUp) = 0;

	/**
	 * @brief Set the listener gain (master volume scaling).
	 * @param[in] iGain Gain value (0.0 = silent, 1.0 = full).
	 */
	virtual void setListenerGain(float iGain) = 0;

	/**
	 * @brief Check if a sound source is currently playing.
	 * @param[in] iHandle The sound handle.
	 * @return True if the source is playing.
	 */
	[[nodiscard]] virtual auto isPlaying(SoundHandle iHandle) const -> bool = 0;

	/**
	 * @brief The function to call every frame.
	 * @param iTs The time step.
	 */
	virtual void frame(const core::Timestep& iTs) = 0;

protected:
	/**
	 * @brief Define the API State.
	 * @param[in] iState The new API State.
	 */
	void setState(const State& iState) { m_state = iState; }

private:
	/// Type of Renderer API.
	Type m_type = Type::Null;
	/// The current state of the API.
	State m_state = State::Created;
};

}// namespace owl::sound
