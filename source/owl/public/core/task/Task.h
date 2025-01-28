/**
 * @file Task.h
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/Core.h"
#include <future>

namespace owl::core::task {

class Scheduler;

/**
 * @brief Class defining a simple task for mutlithreading.
 */
class OWL_API Task final {
public:
	/**
	 * @brief Default constructor.
	 */
	explicit Task(const std::function<void()>& iExec, const std::function<void()>& iEnds = [] {});
	/**
	 * @brief Default destructor.
	 */
	~Task();
	/**
	 * @brief Default move constructor.
	 */
	Task(Task&&) noexcept;

	/// @brief The task State.
	enum struct State : uint8_t {
		Waiting,
		Running,
		Terminated,
	};

	/**
	 * @brief Access to the task's state.
	 * @return The task's state.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	/**
	 * @brief Start the Task.
	 */
	void run();

	/**
	 * @brief Check if process is still running. If terminated, execute it corresponding termination program.
	 */
	void poll();

private:
	/// The Task state.
	State m_state = State::Waiting;
	/// The future function.
	std::future<void> m_future;
	/// What to run.
	std::function<void()> m_action;
	/// What to do when terminated.
	std::function<void()> m_termination;
	/// The task ID given by the scheduler.
	size_t m_taskId = 0;

	friend class Scheduler;
};

}// namespace owl::core::task
