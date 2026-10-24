/**
 * @file Scheduler.cpp
 * @author Silmaen
 * @date 01/12/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/task/Scheduler.h"
#include "core/task/SchedulerImpl.h"
#include "debug/Profiler.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <format>
#include <future>
#include <thread>

namespace owl::core::task {

namespace {
// Most workers woken by one `parallelFor`.
constexpr size_t g_MaxHelpers = 7;
}// namespace

void ProfiledWorker::scheduler_prologue(tf::Worker& ioWorker) {
	debug::setProfilerThreadName(std::format("Worker {}", ioWorker.id()).c_str());
}

void ProfiledWorker::scheduler_epilogue([[maybe_unused]] tf::Worker& ioWorker,
										[[maybe_unused]] std::exception_ptr iException) {}

Scheduler::Scheduler() : mp_impl{mkUniq<SchedulerImpl>()} {}

Scheduler::~Scheduler() {
	mp_impl->tasksQueue.clear();
	waitRunning();
}

auto Scheduler::pushTask(Task&& iTask) -> size_t {
	const size_t taskId = mp_impl->nextTaskId;
	iTask.m_taskId = taskId;
	mp_impl->tasksQueue.push_back(mkShared<Task>(std::move(iTask)));
	++mp_impl->nextTaskId;
	return taskId;
}

void Scheduler::frame(const Timestep& iTimestep) {
	// process asynchronous tasks.
	mp_impl->frameInternal();

	// Process timers
	for (const auto& timer: mp_impl->timers) { timer->frame(iTimestep, this); }
	std::erase_if(mp_impl->timers,
				  [](const shared<Timer>& iTimer) -> bool { return iTimer->getState() == Timer::State::Expired; });
}

void Scheduler::waitRunning() {
	while (!mp_impl->runningTasks.empty()) {
		mp_impl->frameInternal(false);
		if (!mp_impl->runningTasks.empty())
			std::this_thread::yield();
	}
}

void Scheduler::waitEmptyQueue() {
	while (!mp_impl->tasksQueue.empty() || !mp_impl->runningTasks.empty()) {
		mp_impl->frameInternal();
		if (!mp_impl->tasksQueue.empty() || !mp_impl->runningTasks.empty())
			std::this_thread::yield();
	}
}

auto Scheduler::isTaskFinished(const size_t iTaskId) const -> bool {
	return iTaskId < mp_impl->nextTaskId && !(isTaskRunning(iTaskId) || isTaskInQueue(iTaskId));
}

auto Scheduler::isTaskRunning(const size_t iTaskId) const -> bool {
	return std::ranges::find_if(mp_impl->runningTasks, [iTaskId](const shared<Task>& iTask) -> bool {
			   return iTask->m_taskId == iTaskId;
		   }) != mp_impl->runningTasks.end();
}

auto Scheduler::isTaskInQueue(const size_t iTaskId) const -> bool {
	return std::ranges::find_if(mp_impl->tasksQueue, [iTaskId](const shared<Task>& iTask) -> bool {
			   return iTask->m_taskId == iTaskId;
		   }) != mp_impl->tasksQueue.end();
}

auto Scheduler::hasPendingTasks() const -> bool {
	return !mp_impl->runningTasks.empty() || !mp_impl->tasksQueue.empty();
}

auto Scheduler::getWorkerCount() const -> uint32_t {
	return std::max(static_cast<uint32_t>(mp_impl->executor.num_workers()), 1u);
}

void Scheduler::parallelFor(const size_t iCount, const size_t iMinChunk,
							const std::function<void(size_t, size_t)>& iBody) {
	if (iCount == 0)
		return;
	// Waking a worker costs a few microseconds: past a handful of helpers the wake-ups outweigh the work shared.
	const size_t workers = std::clamp<size_t>(mp_impl->executor.num_workers(), 1, g_MaxHelpers);
	const size_t minChunk = std::max<size_t>(iMinChunk, 1);
	// Four chunks per thread balance uneven chunks; a chunk smaller than `minChunk` costs more than it saves.
	const size_t chunkCount = std::clamp<size_t>(iCount / minChunk, 1, (workers + 1) * 4);
	if (chunkCount == 1) {
		iBody(0, iCount);
		return;
	}
	// Helpers that start after the end find nothing to claim: they own the state, not the caller's stack.
	struct State {
		std::atomic<size_t> next{0};
		std::atomic<size_t> done{0};
		size_t count = 0;
		size_t chunks = 0;
		const std::function<void(size_t, size_t)>* body = nullptr;
	};
	auto state = mkShared<State>();
	state->count = iCount;
	state->chunks = chunkCount;
	state->body = &iBody;
	const auto drain = [](State& ioState) -> void {
		for (size_t chunk = ioState.next.fetch_add(1, std::memory_order_relaxed); chunk < ioState.chunks;
			 chunk = ioState.next.fetch_add(1, std::memory_order_relaxed)) {
			(*ioState.body)(chunk* ioState.count / ioState.chunks, (chunk + 1) * ioState.count / ioState.chunks);
			if (ioState.done.fetch_add(1, std::memory_order_acq_rel) + 1 == ioState.chunks)
				ioState.done.notify_all();
		}
	};
	for (size_t helper = 0; helper < std::min(workers, chunkCount - 1); ++helper)
		mp_impl->executor.silent_async([state, drain]() -> void { drain(*state); });
	drain(*state);
	for (size_t done = state->done.load(std::memory_order_acquire); done != chunkCount;
		 done = state->done.load(std::memory_order_acquire))
		state->done.wait(done, std::memory_order_acquire);
}

void Scheduler::clearQueue() { mp_impl->tasksQueue.clear(); }

auto Scheduler::pushTimer(const TimerParam& iTimerParam) -> weak<Timer> {
	mp_impl->timers.push_back(mkShared<Timer>(iTimerParam));
	return mp_impl->timers.back();
}

void Scheduler::clearTimers() { mp_impl->timers.clear(); }

void SchedulerImpl::frameInternal(const bool iTreatQueue) {
	// Poll all running tasks.
	for (const auto& task: runningTasks) { task->poll(); }

	// Check finished tasks.
	std::erase_if(runningTasks,
				  [](const shared<Task>& iTask) -> bool { return iTask->getState() == Task::State::Terminated; });

	// Launch new tasks from the queue using the Taskflow executor.
	while (iTreatQueue && !tasksQueue.empty() && (runningTasks.size() < maxRunningTasks)) {
		auto& task = tasksQueue.front();
		// Bridge std::promise to keep std::future<void> in the public Task header.
		auto promise = mkShared<std::promise<void>>();
		task->m_future = promise->get_future();
		executor.silent_async([action = task->m_action, promise]() -> void {
			action();
			promise->set_value();
		});
		task->m_state = Task::State::Running;
		runningTasks.push_back(std::move(task));
		tasksQueue.pop_front();
	}
}

}// namespace owl::core::task
