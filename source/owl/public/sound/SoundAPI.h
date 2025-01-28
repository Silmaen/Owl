/**
 * @file SoundAPI.h
 * @author Silmaen
 * @date 11/5/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "SoundData.h"

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
		OpenAL = 1,///< OpenAL Sound system.
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
	[[nodiscard]] auto requireInit() const -> bool { return m_type == Type::OpenAL; }

	/**
	 * @brief Play a sound.
	 * @param[in] iData The Sound to play.
	 */
	virtual void playSound(const shared<SoundData>& iData) = 0;

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
