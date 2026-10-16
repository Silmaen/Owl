/**
 * @file PlayOverlay.cpp
 * @author Silmaen
 * @date 10/16/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PlayOverlay.h"

#include <algorithm>

namespace owl::nest::panel {

void PlayBanner::update(const PlayPhase iPhase, const float iDeltaSeconds) {
	if (iPhase != m_phase) {
		m_phase = iPhase;
		m_elapsed = iPhase == PlayPhase::Edit ? holdSeconds + fadeSeconds : 0.f;
		return;
	}
	m_elapsed += std::max(0.f, iDeltaSeconds);
}

auto PlayBanner::alpha() const -> float {
	if (m_phase == PlayPhase::Edit || m_elapsed >= holdSeconds + fadeSeconds)
		return 0.f;
	if (m_elapsed <= holdSeconds)
		return 1.f;
	return 1.f - (m_elapsed - holdSeconds) / fadeSeconds;
}

auto PlayInputGate::filter(const bool iDown, const bool iInViewport, const bool iOverOverlay) -> bool {
	if (!iDown) {
		m_wasDown = false;
		m_latched = false;
		return false;
	}
	if (!m_wasDown)
		m_latched = iInViewport && !iOverOverlay;
	m_wasDown = true;
	return m_latched;
}

}// namespace owl::nest::panel
