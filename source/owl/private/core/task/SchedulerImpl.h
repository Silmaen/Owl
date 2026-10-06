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
#include <cstddef>
#include <deque>
#include <exception>
#include <thread>
#include <vector>

namespace owl::core::task {
/**
 * @brief
 *  Taskflow worker hook naming each worker thread on the profiler timeline ("Worker N").
 */
class ProfiledWorker final : public tf::WorkerInterface {
public:
	ProfiledWorker() = default;

	ProfiledWorker(const ProfiledWorker&) = delete;

	ProfiledWorker(ProfiledWorker&&) = delete;

	auto operator=(const ProfiledWorker&) -> ProfiledWorker& = delete;

	auto operator=(ProfiledWorker&&) -> ProfiledWorker& = delete;

	~ProfiledWorker() override = default;

	/**
	 * @brief
	 *  Name the worker thread before it enters the scheduling loop.
	 * @param[in] ioWorker The Taskflow worker.
	 */
	void scheduler_prologue(tf::Worker& ioWorker) override;// NOLINT(readability-identifier-naming) Taskflow API

	/**
	 * @brief
	 *  Nothing to do when the worker leaves the scheduling loop.
	 * @param[in] ioWorker The Taskflow worker.
	 * @param[in] iException Exception raised by the worker, if any.
	 */
	void scheduler_epilogue(tf::Worker& ioWorker,// NOLINT(readability-identifier-naming) Taskflow API
							std::exception_ptr iException) override;
};

/**
 * @brief
 *  Private implementation of the Scheduler, hiding Taskflow internals.
 */
struct SchedulerImpl {
	/**
	 * @brief
	 *  The Taskflow executor (thread pool) — declared first so it is destroyed last.
	 */
	tf::Executor executor{std::thread::hardware_concurrency(), tf::make_worker_interface<ProfiledWorker>()};
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
	 * @brief
	 *  Internal frame processing: poll running tasks, clean finished, launch queued.
	 * @param[in] iTreatQueue Whether to dequeue and launch new tasks.
	 */
	void frameInternal(bool iTreatQueue = true);
};

}// namespace owl::core::task
