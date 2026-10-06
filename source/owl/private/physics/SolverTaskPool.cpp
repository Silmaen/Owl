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
#include <cstdint>

namespace owl::physics {

SolverTaskPool::SolverTaskPool(const uint32_t iWorkerCount)
	: m_workerCount{std::max(iWorkerCount, 2U)}, mp_executor{mkUniq<tf::Executor>(m_workerCount)} {}

SolverTaskPool::~SolverTaskPool() = default;

void SolverTaskPool::configure(b2WorldDef& ioDef) {
	ioDef.workerCount = static_cast<int>(m_workerCount);
	ioDef.enqueueTask = &SolverTaskPool::enqueue;
	ioDef.finishTask = &SolverTaskPool::finish;
	ioDef.userTaskContext = this;
}

auto SolverTaskPool::acquireGroup() -> TaskGroup& {
	if (m_usedGroups == m_groups.size())
		m_groups.emplace_back();
	return m_groups[m_usedGroups++];
}

auto SolverTaskPool::enqueue(b2TaskCallback* iTask, const int iItemCount, const int iMinRange, void* iTaskContext,
							 void* iUserContext) -> void* {
	auto& pool = *static_cast<SolverTaskPool*>(iUserContext);
	auto& group = pool.acquireGroup();
	if (iItemCount <= 0)
		return &group;
	const int rangeCount = std::clamp(iItemCount / std::max(iMinRange, 1), 1, static_cast<int>(pool.m_workerCount));
	group.pending.store(rangeCount, std::memory_order_relaxed);
	const int baseSize = iItemCount / rangeCount;
	const int remainder = iItemCount % rangeCount;
	auto* executor = pool.mp_executor.get();
	int start = 0;
	for (int range = 0; range < rangeCount; ++range) {
		const int end = start + baseSize + (range < remainder ? 1 : 0);
		executor->silent_async([iTask, start, end, range, iTaskContext, &group]() -> void {
			iTask(start, end, static_cast<uint32_t>(range), iTaskContext);
			if (group.pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
				group.pending.notify_all();
		});
		start = end;
	}
	return &group;
}

void SolverTaskPool::finish(void* iUserTask, [[maybe_unused]] void* iUserContext) {
	auto& group = *static_cast<TaskGroup*>(iUserTask);
	for (int pending = group.pending.load(std::memory_order_acquire); pending != 0;
		 pending = group.pending.load(std::memory_order_acquire))
		group.pending.wait(pending, std::memory_order_acquire);
}

}// namespace owl::physics
