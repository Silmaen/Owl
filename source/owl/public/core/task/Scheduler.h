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

#include <cstddef>
#include <cstdint>
#include <functional>

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
	 * @param[in] iTimestep The time step of the frame.
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
	 *  Check if a task waits in the queue or runs (timers excluded).
	 * @return True while a task has not finished.
	 */
	[[nodiscard]] auto hasPendingTasks() const -> bool;

	/**
	 * @brief
	 *  Number of worker threads of the pool, for libraries that run their own workers (atlas generation).
	 * @return The worker count, at least 1.
	 */
	[[nodiscard]] auto getWorkerCount() const -> uint32_t;

	/**
	 * @brief
	 *  Run a body over `[0, iCount)` split into chunks, on the pool and on the calling thread, and wait for the end.
	 *
	 * The calling thread takes chunks too, so the call never waits for a worker that is busy elsewhere. The chunks
	 * are disjoint, contiguous ranges handed out in increasing order; the body must not throw and must only write
	 * data of its own range. Everything the body writes is visible to the caller when the call returns.
	 * @param[in] iCount Number of items.
	 * @param[in] iMinChunk Smallest worthwhile chunk size, at least 1.
	 * @param[in] iBody Called with the half-open range `[begin, end)` of one chunk.
	 */
	void parallelFor(size_t iCount, size_t iMinChunk, const std::function<void(size_t, size_t)>& iBody);

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
