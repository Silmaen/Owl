/**
 * @file SolverTaskPool.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <box2d/box2d.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace tf {
class Executor;
}// namespace tf

namespace owl::physics {

/**
 * @brief
 *  Box2D task callbacks (`enqueueTask` / `finishTask`) backed by a Taskflow executor.
 *
 * The executor is dedicated to the solver instead of shared with `core::task::Scheduler`: within a
 * step, Box2D's solver tasks spin-wait on each other, so all of them must run at once, and a long
 * Scheduler job holding a worker would stall the physics step until it ends.
 *
 * Each `enqueueTask` call is split into at most `workerCount` ranges of at least `minRange` items,
 * each run by one executor worker. The worker index handed to Box2D is the range index, not the
 * Taskflow thread id: Box2D keeps per-worker state (island-split candidate, bit sets) whose merge
 * depends on which items each index saw, so a thread id would make results vary from run to run.
 * Ranges of one task get distinct indices; the tasks Box2D runs concurrently with an indexed task
 * (tree rebuild, island split, solver stages) ignore the index. Results are thus reproducible for a
 * given worker count, and differ slightly between worker counts. Tasks are enqueued and finished
 * from the thread that calls `b2World_Step`.
 */
class SolverTaskPool final {
public:
	SolverTaskPool(const SolverTaskPool&) = delete;

	SolverTaskPool(SolverTaskPool&&) = delete;

	auto operator=(const SolverTaskPool&) -> SolverTaskPool& = delete;

	auto operator=(SolverTaskPool&&) -> SolverTaskPool& = delete;

	/**
	 * @brief
	 *  Start the worker threads.
	 * @param[in] iWorkerCount Number of worker threads, at least 2.
	 */
	explicit SolverTaskPool(uint32_t iWorkerCount);

	/**
	 * @brief
	 *  Join the worker threads.
	 */
	~SolverTaskPool();

	/**
	 * @brief
	 *  Number of worker threads.
	 * @return The worker count.
	 */
	[[nodiscard]] auto getWorkerCount() const -> uint32_t { return m_workerCount; }

	/**
	 * @brief
	 *  Route the world's tasks to this pool.
	 * @param[in,out] ioDef The world definition to fill (`workerCount`, task callbacks, user context).
	 */
	void configure(b2WorldDef& ioDef);

	/**
	 * @brief
	 *  Recycle the task records; call after each `b2World_Step`, when every task has finished.
	 */
	void recycle() { m_usedGroups = 0; }

private:
	/**
	 * @brief
	 *  Completion counter of one enqueued Box2D task.
	 */
	struct TaskGroup {
		/// Ranges of the task still running.
		std::atomic<int> pending{0};
	};

	/**
	 * @brief
	 *  Box2D `enqueueTask` callback: split the items into ranges and run them on the workers.
	 * @param[in] iTask The Box2D task function.
	 * @param[in] iItemCount Number of items to process.
	 * @param[in] iMinRange Minimum number of items per range.
	 * @param[in] iTaskContext Box2D context passed back to the task.
	 * @param[in] iUserContext The pool.
	 * @return The task record, waited on by `finish`.
	 */
	static auto enqueue(b2TaskCallback* iTask, int iItemCount, int iMinRange, void* iTaskContext, void* iUserContext)
			-> void*;

	/**
	 * @brief
	 *  Box2D `finishTask` callback: wait until every range of the task has run.
	 * @param[in] iUserTask The task record returned by `enqueue`.
	 * @param[in] iUserContext The pool.
	 */
	static void finish(void* iUserTask, void* iUserContext);

	/**
	 * @brief
	 *  Take a free task record, growing the list when all are in use.
	 * @return The task record.
	 */
	auto acquireGroup() -> TaskGroup&;

	/// Number of worker threads.
	uint32_t m_workerCount;
	/// Taskflow executor running the ranges.
	uniq<tf::Executor> mp_executor;
	/// Task records; a deque keeps their addresses stable when it grows.
	std::deque<TaskGroup> m_groups;
	/// Records handed out since the last `recycle()`.
	size_t m_usedGroups = 0;
};

}// namespace owl::physics
