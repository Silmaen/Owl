/**
 * @file Timer.h
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Timestep.h"

namespace owl::core::task {

class Scheduler;

using clock = std::chrono::steady_clock;

/**
 * @brief Parameter of the timer.
 */
struct TimerParam {
	/// Function to exec.
	std::function<void()> exec;
	/// Duration Between execution.
	Timestep::duration frequency;
	/// Should function be run in asynchron mode.
	bool async = false;
	/// Number of repeat before end (0: means infinite).
	size_t iteration = 0;
};

/**
 * @brief Class defining a regularly called task.
 */
class OWL_API Timer final {
public:
	/**
	 * @brief Default constructor.
	 * @param[in] iParam Timer parameter.
	 */
	explicit Timer(const TimerParam& iParam);
	/**
	 * @brief Default destructor.
	 */
	~Timer();
	/**
	 * @brief Default copy constructor.
	 */
	Timer(const Timer&) = default;
	/**
	 * @brief Default move constructor.
	 */
	Timer(Timer&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const Timer&) -> Timer& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(Timer&&) noexcept -> Timer& = default;

	/**
	 * @brief Frame call.
	 * @param[in] iTimestep The Timer.
	 * @param[in] iScheduler Pointer to the current task scheduler.
	 */
	void frame(const Timestep& iTimestep, Scheduler* iScheduler = nullptr);

	/**
	 * @brief Pause this timer.
	 */
	void pause();
	/**
	 * @brief Resume this timer.
	 */
	void resume();
	/**
	 * @brief Set the pause state of the timer.
	 * @param iPaused The state to set.
	 */
	void setPaused(bool iPaused);
	/**
	 * @brief Toggle the pause state.
	 */
	void togglePaused();

	/**
	 * @brief The Timer state.
	 */
	enum struct State : uint8_t {
		Started,///< Timer started.
		Running,///< Timer is active.
		Paused,///< Timer still exists but not executed.
		Expired,///< Timer has expired.
	};

	/**
	 * @brief Get the state of the timer.
	 * @return The timer's state.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

private:
	/// The starting parameters.
	TimerParam m_param;
	/// Actual state of the Timer.
	State m_state = State::Started;
	/// Counter of the
	size_t m_counter = 0;
	/// Time point of the last call.
	Timestep::timePoint m_lastCall;
	void execute(Scheduler* iScheduler, const Timestep::timePoint& iCurrentTime);
};

}// namespace owl::core::task
