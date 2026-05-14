/**
 * @file Scheduler.h
 * @author Silmaen
 * @date 01/12/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Task.h"
#include "Timer.h"

/**
 * @brief
 *  Namespace for task management.
 */
namespace owl::core::task {

struct SchedulerImpl;

/**
 * @brief
 *  Class that manage the tasks.
 */
class OWL_API Scheduler final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	Scheduler();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~Scheduler();

	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	Scheduler(const Scheduler&) = delete;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	Scheduler(Scheduler&&) = delete;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 */
	auto operator=(const Scheduler&) -> Scheduler& = delete;

	/**
	 * @brief
	 *  Deleted move affectation operator.
	 */
	auto operator=(Scheduler&&) -> Scheduler& = delete;

	/**
	 * @brief
	 *  Insert Task to the queue.
	 * @param iTask Task to push.
	 * @return The task ID for external follow.
	 */
	auto pushTask(Task&& iTask) -> size_t;

	/**
	 * @brief
	 *  Add a timer Task.
	 * @param iTimerParam The timer task.
	 * @return The weak.
	 */
	auto pushTimer(const TimerParam& iTimerParam) -> weak<Timer>;

	/**
	 * @brief
	 *  Execute each frame.
	 */
	void frame(const Timestep& iTimestep);

	/**
	 * @brief
	 *  Wait for all running tasks to finish.
	 * @note The tasks in queue will not be launched.
	 */
	void waitRunning();

	/**
	 * @brief
	 *  Wait for all known tasks to run and finish.
	 */
	void waitEmptyQueue();

	/**
	 * @brief
	 *  Check if the task with given ID is finished.
	 * @param[in] iTaskId The task's ID.
	 * @return true if finished.
	 */
	[[nodiscard]] auto isTaskFinished(size_t iTaskId) const -> bool;

	/**
	 * @brief
	 *  Check if the task with given ID is finished.
	 * @param[in] iTaskId The task's ID.
	 * @return true if finished.
	 */
	[[nodiscard]] auto isTaskRunning(size_t iTaskId) const -> bool;

	/**
	 * @brief
	 *  Check if the task with given ID is in queue.
	 * @param[in] iTaskId The task's ID.
	 * @return true if finished.
	 */
	[[nodiscard]] auto isTaskInQueue(size_t iTaskId) const -> bool;

	/**
	 * @brief
	 *  Clear the tasks queue.
	 */
	void clearQueue();

	/**
	 * @brief
	 *  Clear the Timer list.
	 */
	void clearTimers();

	/**
	 * @brief
	 *  Engine-internal accessor for the Taskflow-backed implementation.
	 *
	 * Returns a reference to the private `SchedulerImpl` struct so engine
	 * code (which includes the private header) can reach the `tf::Executor`
	 * for `parallelForEach` / `parallelForIndex`. External callers can hold
	 * the reference but cannot use it without the private header — this is
	 * intentional: the Taskflow dependency stays PRIVATE.
	 * @return Reference to the private implementation.
	 */
	[[nodiscard]] auto getImpl() const -> SchedulerImpl&;

private:
	/// Private implementation hiding Taskflow internals.
	uniq<SchedulerImpl> mp_impl;
};

}// namespace owl::core::task
