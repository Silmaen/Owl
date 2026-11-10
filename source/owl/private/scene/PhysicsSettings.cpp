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

auto PhysicsSettings::clamped() const -> PhysicsSettings {
	PhysicsSettings out = *this;
	out.solverSubSteps = std::clamp(solverSubSteps, 1U, maxSolverSubSteps);
	out.workerCount = std::min(workerCount, maxWorkerCount);
	out.velocitySteps = std::clamp(velocitySteps, 1U, maxSolverIterations);
	out.positionSteps = std::clamp(positionSteps, 1U, maxSolverIterations);
	for (size_t axis = 0; axis < 3; ++axis)
		out.gravity[axis] = std::isfinite(gravity[axis]) ? std::clamp(gravity[axis], -maxGravity, maxGravity) : 0.f;
	out.railsTimeScale = std::isfinite(railsTimeScale)
								 ? std::clamp(railsTimeScale, minRailsTimeScale, maxRailsTimeScale)
								 : defaultRailsTimeScale;
	return out;
}

auto PhysicsSettings::getEffectiveWorkerCount(const uint32_t iDynamicBodies) const -> uint32_t {
	if (workerCount != 0)
		return std::min(workerCount, maxWorkerCount);
	if (iDynamicBodies < autoThreadingMinBodies)
		return 1;
	return std::clamp(std::thread::hardware_concurrency() / 2, 1U, maxAutoWorkerCount);
}

auto PhysicsSettings::operator==(const PhysicsSettings& iOther) const -> bool {
	const auto same = [](const float iLeft, const float iRight) -> bool { return iLeft >= iRight && iLeft <= iRight; };
	return solverSubSteps == iOther.solverSubSteps && interpolate == iOther.interpolate &&
		   workerCount == iOther.workerCount && same(gravity.x(), iOther.gravity.x()) &&
		   same(gravity.y(), iOther.gravity.y()) && same(gravity.z(), iOther.gravity.z()) &&
		   same(railsTimeScale, iOther.railsTimeScale) && velocitySteps == iOther.velocitySteps &&
		   positionSteps == iOther.positionSteps;
}

}// namespace owl::scene
