/**
 * @file Task.cpp
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/task/Task.h"

namespace owl::core::task {

Task::Task(const std::function<void()>& iExec, const std::function<void()>& iEnds)
	: m_action{iExec}, m_termination{iEnds} {}

Task::Task(Task&& iOther) noexcept
	: m_state(iOther.m_state), m_future(std::move(iOther.m_future)), m_action(std::move(iOther.m_action)),
	  m_termination(std::move(iOther.m_termination)), m_taskId(iOther.m_taskId) {
	iOther.m_state = State::Waiting;
}

Task::~Task() {
	if (m_state == State::Running) {
		m_future.wait();
	}
}

void Task::run() {
	if (m_state == State::Waiting) {
		m_future = std::async(std::launch::async, m_action);
		m_state = State::Running;
	}
}

void Task::poll() {
	if (m_state == State::Running) {
		if (m_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			m_termination();
			m_state = State::Terminated;
		}
	}
}

}// namespace owl::core::task
