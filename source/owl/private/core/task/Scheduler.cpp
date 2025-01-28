/**
 * @file Scheduler.cpp
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/task/Scheduler.h"

namespace owl::core::task {

Scheduler::Scheduler() = default;

Scheduler::~Scheduler() {
	while (!m_tasksQueue.empty()) {
		// try pop
		m_tasksQueue.pop();
	}
	waitRunning();
}

auto Scheduler::pushTask(Task&& iTask) -> size_t {
	const size_t taskId = m_nextTaskId;
	iTask.m_taskId = taskId;
	m_tasksQueue.push(mkShared<Task>(std::move(iTask)));
	++m_nextTaskId;
	return taskId;
}

void Scheduler::frame(const Timestep& iTimestep) {
	// process asynchron tasks.
	frameInternal();

	// Process timers
	for (const auto& timer: m_timers) {
		// frame the timer
		timer->frame(iTimestep, this);
	}
	std::erase_if(m_timers, [](const shared<Timer>& iTimer) { return iTimer->getState() == Timer::State::Expired; });
}

void Scheduler::frameInternal(const bool iTreatQueue) {
	// Poll all running tasks.
	for (const auto& task: m_runningTasks) {
		// Polling Task!
		task->poll();
	}

	// Check finished tasks.
	std::erase_if(m_runningTasks,
				  [](const shared<Task>& iTask) { return iTask->getState() == Task::State::Terminated; });

	// push new tasks
	while (iTreatQueue && !m_tasksQueue.empty() && (m_runningTasks.size() < m_maxRunningTasks)) {
		m_tasksQueue.front()->run();
		m_runningTasks.push_back(std::move(m_tasksQueue.front()));
		m_tasksQueue.pop();
	}
}

void Scheduler::waitRunning() {
	while (!m_runningTasks.empty()) { frameInternal(false); }
}

void Scheduler::waitEmptyQueue() {
	while (!m_tasksQueue.empty() || !m_runningTasks.empty()) { frameInternal(); }
}

auto Scheduler::isTaskFinished(const size_t& iTaskId) -> bool {
	//     not known                not         (running      or      in queue )
	return iTaskId < m_nextTaskId && !(isTaskRunning(iTaskId) || isTaskInQueue(iTaskId));
}

auto Scheduler::isTaskRunning(const size_t& iTaskId) -> bool {
	return std::ranges::find_if(m_runningTasks, [&iTaskId](const shared<Task>& iTask) {
			   return iTask->m_taskId == iTaskId;
		   }) != m_runningTasks.end();
}
auto Scheduler::isTaskInQueue(const size_t& iTaskId) -> bool {
	bool found = false;
	std::queue<shared<Task>> tasks;
	while (!m_tasksQueue.empty()) {
		tasks.push(m_tasksQueue.front());
		if (m_tasksQueue.front()->m_taskId == iTaskId)
			found = true;
		m_tasksQueue.pop();
	}
	std::swap(tasks, m_tasksQueue);
	return found;
}

void Scheduler::clearQueue() {
	while (!m_tasksQueue.empty()) { m_tasksQueue.pop(); }
}

auto Scheduler::pushTimer(const TimerParam& iTimerParam) -> weak<Timer> {
	m_timers.push_back(mkShared<Timer>(iTimerParam));
	return m_timers.back();
}

void Scheduler::clearTimers() { m_timers.clear(); }

}// namespace owl::core::task
