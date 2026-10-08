/**
 * @file PhysicsSettings.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/PhysicsSettings.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <thread>

namespace owl::scene {

auto PhysicsSettings::getStepSeconds() const -> double { return 1.0 / static_cast<double>(clamped().tickRate); }

auto PhysicsSettings::clamped() const -> PhysicsSettings {
	PhysicsSettings out = *this;
	out.tickRate = std::isfinite(tickRate) ? std::clamp(tickRate, minTickRate, maxTickRate) : defaultTickRate;
	out.maxStepsPerFrame = std::clamp(maxStepsPerFrame, 1U, maxStepsLimit);
	out.solverSubSteps = std::clamp(solverSubSteps, 1U, maxSolverSubSteps);
	out.workerCount = std::min(workerCount, maxWorkerCount);
	return out;
}

auto PhysicsSettings::getEffectiveWorkerCount(const uint32_t iDynamicBodies) const -> uint32_t {
	if (workerCount != 0)
		return std::min(workerCount, maxWorkerCount);
	if (iDynamicBodies < autoThreadingMinBodies)
		return 1;
	return std::clamp(std::thread::hardware_concurrency() / 2, 1U, maxAutoWorkerCount);
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto PhysicsSettings::operator==(const PhysicsSettings& iOther) const -> bool = default;
OWL_DIAG_POP

}// namespace owl::scene
