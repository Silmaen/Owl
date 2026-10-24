/**
 * @file FullLoadProbe.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "FullLoadProbe.h"

#include <algorithm>

namespace owl::nest::panel {

void FullLoadProbe::start(const clock::time_point iNow) {
	m_running = true;
	m_measuring = false;
	m_start = iNow;
	m_frames = 0;
	m_workSum = 0.0;
	m_gpuFrames = 0;
	m_gpuSum = 0.0;
}

auto FullLoadProbe::addFrame(const double iWorkMs, const clock::time_point iNow) -> bool {
	if (!m_running)
		return false;
	if (!m_measuring) {
		if (iNow - m_start < warmup)
			return false;
		// The measure starts at the first frame after the warm-up.
		m_measuring = true;
		m_start = iNow;
		m_gpuFrames = 0;
		m_gpuSum = 0.0;
		return false;
	}
	++m_frames;
	m_workSum += std::max(0.0, iWorkMs);
	const auto elapsed = iNow - m_start;
	if (elapsed < measure)
		return false;
	m_running = false;
	m_result = Result{.fps = static_cast<double>(m_frames) / std::chrono::duration<double>(elapsed).count(),
					  .workMs = m_workSum / static_cast<double>(m_frames),
					  .gpuMs = m_gpuFrames > 0 ? std::optional{m_gpuSum / static_cast<double>(m_gpuFrames)}
											   : std::nullopt};
	return true;
}

void FullLoadProbe::addGpu(const double iBusyMs) {
	if (!m_measuring)
		return;
	++m_gpuFrames;
	m_gpuSum += std::max(0.0, iBusyMs);
}

}// namespace owl::nest::panel
