/**
 * @file FramePacer.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/FramePacer.h"
#include "debug/Profiler.h"

#include <algorithm>
#include <cstdint>
#include <thread>

namespace owl::core {

void FramePacer::setPeriod(const clock::duration iPeriod, const clock::time_point iNow) {
	m_period = std::max(iPeriod, clock::duration::zero());
	m_deadline = iNow;
}

void FramePacer::setRate(const uint32_t iPerSecond, const clock::time_point iNow) {
	setPeriod(iPerSecond == 0 ? clock::duration::zero()
							  : std::chrono::duration_cast<clock::duration>(
										std::chrono::nanoseconds{1'000'000'000LL / static_cast<int64_t>(iPerSecond)}),
			  iNow);
}

auto FramePacer::next(const clock::time_point iNow) -> clock::time_point {
	if (!isActive())
		return iNow;
	m_deadline += m_period;
	if (m_deadline < iNow)
		m_deadline = iNow;
	return m_deadline;
}

void FramePacer::wait() {
	if (!isActive())
		return;
	OWL_PROFILE_FUNCTION()

	const auto deadline = next(clock::now());
	// sleep_until may wake a little early (Windows rounds the wait to its timer): sleep again until the deadline.
	while (clock::now() < deadline) std::this_thread::sleep_until(deadline);
}

}// namespace owl::core
