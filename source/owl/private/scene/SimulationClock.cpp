/**
 * @file SimulationClock.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "scene/SimulationClock.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace owl::scene {

auto SimulationSettings::getStepSeconds() const -> double { return 1.0 / static_cast<double>(clamped().tickRate); }

auto SimulationSettings::clamped() const -> SimulationSettings {
	SimulationSettings out = *this;
	out.tickRate = std::isfinite(tickRate) ? std::clamp(tickRate, minTickRate, maxTickRate) : defaultTickRate;
	out.maxTicksPerFrame = std::clamp(maxTicksPerFrame, 1U, maxTicksLimit);
	return out;
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto SimulationSettings::operator==(const SimulationSettings& iOther) const -> bool = default;
OWL_DIAG_POP

SimulationClock::SimulationClock() : m_stepSeconds{m_settings.getStepSeconds()} { setSettings(m_settings); }

void SimulationClock::setSettings(const SimulationSettings& iSettings) {
	m_settings = iSettings.clamped();
	m_stepSeconds = m_settings.getStepSeconds();
	m_stepTimestep.forceUpdate(
			std::chrono::duration_cast<core::Timestep::duration>(std::chrono::duration<double>{m_stepSeconds}));
}

void SimulationClock::reset() {
	m_accumulator = 0.0;
	m_frameTicks = 0;
	m_totalTicks = 0;
	m_alpha = 0.f;
}

auto SimulationClock::advance(const core::Timestep& iFrame) -> uint32_t {
	const double step = m_stepSeconds;
	m_accumulator += std::max(0.0, static_cast<double>(iFrame.getMilliseconds()) / 1000.0);
	// The tolerance absorbs the rounding of frame durations that are whole multiples of the step.
	auto ticks = static_cast<uint32_t>(std::floor((m_accumulator + step * 1e-4) / step));
	m_accumulator = std::max(0.0, m_accumulator - static_cast<double>(ticks) * step);
	if (ticks > m_settings.maxTicksPerFrame) {
		ticks = m_settings.maxTicksPerFrame;
		m_accumulator = std::fmod(m_accumulator, step);
	}
	m_frameTicks = ticks;
	m_totalTicks += ticks;
	m_alpha = static_cast<float>(std::min(m_accumulator / step, 1.0));
	return ticks;
}

}// namespace owl::scene
