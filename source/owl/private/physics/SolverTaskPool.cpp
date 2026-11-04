/**
 * @file SolverTaskPool.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "SolverTaskPool.h"

#include "core/external/taskflow.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace owl::physics {

SolverTaskPool::SolverTaskPool(const uint32_t iWorkerCount)
	: m_workerCount{std::max(iWorkerCount, 2U)}, mp_executor{mkUniq<tf::Executor>(m_workerCount)} {}

SolverTaskPool::~SolverTaskPool() = default;

void SolverTaskPool::runRanges(const size_t iCount, const size_t iMinRange, const RangeFn iBody, void* iContext) {
	const size_t rangeCount = std::clamp<size_t>(iCount / std::max<size_t>(iMinRange, 1), 1, m_workerCount);
	std::atomic<size_t> pending{rangeCount - 1};
	size_t start = 0;
	for (size_t range = 0; range + 1 < rangeCount; ++range) {
		const size_t end = iCount * (range + 1) / rangeCount;
		mp_executor->silent_async([iBody, iContext, start, end, &pending]() -> void {
			iBody(iContext, start, end);
			if (pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
				pending.notify_all();
		});
		start = end;
	}
	iBody(iContext, start, iCount);
	for (size_t left = pending.load(std::memory_order_acquire); left != 0;
		 left = pending.load(std::memory_order_acquire))
		pending.wait(left, std::memory_order_acquire);
}

}// namespace owl::physics
