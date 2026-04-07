/**
 * @file SchedulerImpl.h
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "core/external/taskflow.h"
#include "core/task/Scheduler.h"
#include <deque>

namespace owl::core::task {

/**
 * @brief Private implementation of the Scheduler, hiding Taskflow internals.
 */
struct SchedulerImpl {
	/// The Taskflow executor (thread pool) — declared first so it is destroyed last.
	tf::Executor executor{std::thread::hardware_concurrency()};
	/// Tasks waiting to be submitted.
	std::deque<shared<Task>> tasksQueue;
	/// Currently running tasks.
	std::vector<shared<Task>> runningTasks;
	/// Maximum number of concurrent tasks (adapts to hardware).
	size_t maxRunningTasks = std::thread::hardware_concurrency();
	/// Next task ID counter.
	size_t nextTaskId = 1;
	/// Active timers.
	std::vector<shared<Timer>> timers;

	/**
	 * @brief Internal frame processing: poll running tasks, clean finished, launch queued.
	 * @param[in] iTreatQueue Whether to dequeue and launch new tasks.
	 */
	void frameInternal(bool iTreatQueue = true);
};

}// namespace owl::core::task
