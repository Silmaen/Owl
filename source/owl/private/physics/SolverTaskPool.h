/**
 * @file SolverTaskPool.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace tf {
class Executor;
}// namespace tf

namespace owl::physics {

/**
 * @brief
 *  Taskflow executor of one physics world: Jolt's jobs during a step, the pose copies between steps.
 *
 * The executor is dedicated to the solver instead of shared with `core::task::Scheduler`: a long Scheduler job
 * holding a worker would stall the physics step until it ends. Jolt's results do not depend on the worker count.
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
	 *  The executor running the tasks.
	 * @return The executor.
	 */
	[[nodiscard]] auto getExecutor() const -> tf::Executor& { return *mp_executor; }

	/**
	 * @brief
	 *  Run a function over `[0, iCount)` split into at most `workerCount` ranges, the last one on the calling
	 *  thread; return when every range has run. Call it between steps.
	 * @tparam Fn Callable taking the range bounds `(begin, end)`.
	 * @param[in] iCount Number of items.
	 * @param[in] iMinRange Minimum number of items per range.
	 * @param[in] iBody The function run on each range.
	 */
	template<typename Fn>
	void parallelFor(const size_t iCount, const size_t iMinRange, Fn&& iBody) {
		using Body = std::remove_reference_t<Fn>;
		runRanges(
				iCount, iMinRange,
				[](void* iContext, const size_t iBegin, const size_t iEnd) -> void {
					(*static_cast<Body*>(iContext))(iBegin, iEnd);
				},
				const_cast<void*>(static_cast<const void*>(&iBody)));
	}

private:
	/// Type-erased range function of `parallelFor`.
	using RangeFn = void (*)(void* iContext, size_t iBegin, size_t iEnd);

	/**
	 * @brief
	 *  Non-template body of `parallelFor`.
	 * @param[in] iCount Number of items.
	 * @param[in] iMinRange Minimum number of items per range.
	 * @param[in] iBody The range function.
	 * @param[in] iContext The context passed back to the range function.
	 */
	void runRanges(size_t iCount, size_t iMinRange, RangeFn iBody, void* iContext);

	/// Number of worker threads.
	uint32_t m_workerCount;
	/// Taskflow executor running the ranges.
	uniq<tf::Executor> mp_executor;
};

}// namespace owl::physics
