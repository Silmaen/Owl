/**
 * @file FrameMeter.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FrameMeter.h"

#include <algorithm>

namespace owl::nest::panel {

void FrameMeter::addFrame(const double iWorkMs, const clock::time_point iNow) {
	if (!m_started) {
		m_started = true;
		m_windowStart = iNow;
	}
	++m_frames;
	m_workSum += std::max(0.0, iWorkMs);
	const auto elapsed = iNow - m_windowStart;
	if (elapsed < window)
		return;
	m_workMs = m_workSum / static_cast<double>(m_frames);
	m_displayFps = static_cast<double>(m_frames) / std::chrono::duration<double>(elapsed).count();
	if (m_gpuFrames > 0)
		m_gpuMs = m_gpuSum / static_cast<double>(m_gpuFrames);
	m_windowStart = iNow;
	m_frames = 0;
	m_workSum = 0.0;
	m_gpuFrames = 0;
	m_gpuSum = 0.0;
}

void FrameMeter::addGpu(const double iBusyMs) {
	++m_gpuFrames;
	m_gpuSum += std::max(0.0, iBusyMs);
}

void FrameMeter::reset() { *this = FrameMeter{}; }

auto FrameMeter::getPotentialFps() const -> double {
	const double cost = std::max(m_workMs, m_gpuMs.value_or(0.0));
	return cost > 0.0 ? 1000.0 / cost : 0.0;
}

}// namespace owl::nest::panel
