/**
 * @file Scheduler.h
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "Task.h"
#include "Timer.h"
#include <queue>

/**
 * @brief Namespace for task management.
 */
namespace owl::core::task {

/**
 * @brief Class that manage the tasks.
 */
class OWL_API Scheduler final {
public:
	/**
	 * @brief Default constructor.
	 */
	Scheduler();
	/**
	 * @brief Default destructor.
	 */
	~Scheduler();
	/**
	 * @brief Default copy constructor.
	 */
	Scheduler(const Scheduler&) = default;
	/**
	 * @brief Default move constructor.
	 */
	Scheduler(Scheduler&&) = default;
	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const Scheduler&) -> Scheduler& = default;
	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(Scheduler&&) -> Scheduler& = default;

	/**
	 * @brief Insert Task to the queue.
	 * @param iTask Task to push.
	 * @return The task ID for external follow.
	 */
	auto pushTask(Task&& iTask) -> size_t;

	/**
	 * @brief Add a timer Task.
	 * @param iTimerParam The timer task.
	 */
	auto pushTimer(const TimerParam& iTimerParam) -> weak<Timer>;

	/**
	 * @brief Execute each frame.
	 */
	void frame(const Timestep& iTimestep);

	/**
	 * @brief Wait for all running tasks to finish.
	 * @note The tasks in queue will  ot be launched.
	 */
	void waitRunning();

	/**
	 * @brief Wait for all known tasks to run and finish.
	 */
	void waitEmptyQueue();

	/**
	 * @brief Check if the task with given ID is finished.
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskFinished(const size_t& iTaskId) -> bool;

	/**
	 * @brief Check if the task with given ID is finished.
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskRunning(const size_t& iTaskId) -> bool;

	/**
	 * @brief Check if the task with given ID is in queue.
	 * @param iTaskId The task's ID.
	 * @return true if finished.
	 */
	auto isTaskInQueue(const size_t& iTaskId) -> bool;

	/**
	 * @brief Clear the tasks queue.
	 */
	void clearQueue();

	/**
	 * @brief Clear the Timer list.
	 */
	void clearTimers();

private:
	void frameInternal(bool iTreatQueue = true);
	/// The tasks in queue.
	std::queue<shared<Task>> m_tasksQueue;
	/// The Running tasks.
	std::vector<shared<Task>> m_runningTasks;
	/// The maximum number of running tasks.
	size_t m_maxRunningTasks = 5;
	/// The next task ID.
	size_t m_nextTaskId = 1;
	/// List of timers.
	std::vector<shared<Timer>> m_timers;
};

}// namespace owl::core::task
