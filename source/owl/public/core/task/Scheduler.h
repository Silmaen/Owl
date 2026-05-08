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
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskFinished(const size_t& iTaskId) -> bool;

	/**
	 * @brief
	 *  Check if the task with given ID is finished.
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskRunning(const size_t& iTaskId) -> bool;

	/**
	 * @brief
	 *  Check if the task with given ID is in queue.
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskInQueue(const size_t& iTaskId) -> bool;

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

private:
	/// Private implementation hiding Taskflow internals.
	uniq<SchedulerImpl> mp_impl;
};

}// namespace owl::core::task
