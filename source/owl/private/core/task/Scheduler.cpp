/**
 * @file Scheduler.cpp
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/task/Scheduler.h"
#include "core/task/SchedulerImpl.h"

namespace owl::core::task {

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
	// process asynchron tasks.
	mp_impl->frameInternal();

	// Process timers
	for (const auto& timer: mp_impl->timers) {
		timer->frame(iTimestep, this);
	}
	std::erase_if(mp_impl->timers,
				  [](const shared<Timer>& iTimer) { return iTimer->getState() == Timer::State::Expired; });
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

auto Scheduler::isTaskFinished(const size_t& iTaskId) -> bool {
	return iTaskId < mp_impl->nextTaskId && !(isTaskRunning(iTaskId) || isTaskInQueue(iTaskId));
}

auto Scheduler::isTaskRunning(const size_t& iTaskId) -> bool {
	return std::ranges::find_if(mp_impl->runningTasks, [&iTaskId](const shared<Task>& iTask) {
			   return iTask->m_taskId == iTaskId;
		   }) != mp_impl->runningTasks.end();
}

auto Scheduler::isTaskInQueue(const size_t& iTaskId) -> bool {
	return std::ranges::find_if(mp_impl->tasksQueue, [&iTaskId](const shared<Task>& iTask) {
			   return iTask->m_taskId == iTaskId;
		   }) != mp_impl->tasksQueue.end();
}

void Scheduler::clearQueue() { mp_impl->tasksQueue.clear(); }

auto Scheduler::pushTimer(const TimerParam& iTimerParam) -> weak<Timer> {
	mp_impl->timers.push_back(mkShared<Timer>(iTimerParam));
	return mp_impl->timers.back();
}

void Scheduler::clearTimers() { mp_impl->timers.clear(); }

void SchedulerImpl::frameInternal(const bool iTreatQueue) {
	// Poll all running tasks.
	for (const auto& task: runningTasks) {
		task->poll();
	}

	// Check finished tasks.
	std::erase_if(runningTasks,
				  [](const shared<Task>& iTask) { return iTask->getState() == Task::State::Terminated; });

	// Launch new tasks from the queue using the Taskflow executor.
	while (iTreatQueue && !tasksQueue.empty() && (runningTasks.size() < maxRunningTasks)) {
		auto& task = tasksQueue.front();
		// Bridge std::promise to keep std::future<void> in the public Task header.
		auto promise = std::make_shared<std::promise<void>>();
		task->m_future = promise->get_future();
		executor.silent_async([action = task->m_action, promise]() {
			action();
			promise->set_value();
		});
		task->m_state = Task::State::Running;
		runningTasks.push_back(std::move(task));
		tasksQueue.pop_front();
	}
}

}// namespace owl::core::task
