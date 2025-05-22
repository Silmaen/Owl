/**
 * @file Timer.cpp
 * @author Silmaen
 * @date 01/12/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/task/Scheduler.h"
#include "core/task/Timer.h"

namespace owl::core::task {

Timer::Timer(const TimerParam& iParam) : m_param{iParam} { m_lastCall = Timestep::clock::now(); }

Timer::~Timer() = default;

void Timer::frame(const Timestep& iTimestep, Scheduler* iScheduler) {
	if (m_state == State::Started) {
		m_state = State::Running;
		execute(iScheduler, iTimestep.getTimePoint());
		return;
	}
	if (m_state != State::Running)
		return;
	if (iTimestep.getTimePoint() - m_lastCall > m_param.frequency) {
		execute(iScheduler, iTimestep.getTimePoint());
	}
}

void Timer::execute(Scheduler* iScheduler, const Timestep::time_point& iCurrentTime) {
	m_lastCall = iCurrentTime;
	m_counter++;
	if (m_param.async && iScheduler != nullptr) {
		iScheduler->pushTask(Task{m_param.exec});
	} else {
		m_param.exec();
	}
	if (m_param.iteration > 0 && m_counter >= m_param.iteration) {
		m_state = State::Expired;
	}
}

void Timer::pause() {
	if (m_state == State::Running) {
		m_state = State::Paused;
	}
}

void Timer::resume() {
	if (m_state == State::Paused) {
		m_state = State::Running;
	}
}

void Timer::setPaused(const bool iPaused) {
	if (iPaused)
		pause();
	else
		resume();
}

void Timer::togglePaused() {
	if (m_state == State::Running) {
		m_state = State::Paused;
	} else if (m_state == State::Paused) {
		m_state = State::Running;
	}
}

}// namespace owl::core::task
