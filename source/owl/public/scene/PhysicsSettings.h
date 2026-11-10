/**
 * @file PhysicsSettings.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene {

/**
 * @brief
 *  Per-scene physics simulation settings, saved in the scene file under `Physics:`.
 *
 * The world advances one step per tick of the scene's fixed simulation clock (`Scene::getSimulationClock()`, rate
 * set by the project). Rendered transforms are blended between the last two steps when `interpolate` is set.
 */
struct OWL_API PhysicsSettings {
	/// Highest accepted collision step count.
	static constexpr uint32_t maxSolverSubSteps = 16;
	/// Highest accepted solver thread count.
	static constexpr uint32_t maxWorkerCount = 32;
	/// Upper bound of the automatic solver thread count.
	static constexpr uint32_t maxAutoWorkerCount = 4;
	/// Dynamic bodies below which the automatic count stays single-threaded (task dispatch costs more than it saves).
	static constexpr uint32_t autoThreadingMinBodies = 2000;
	/// Default time scale above which the bodies go on rails: the default bound of the ticks of a frame.
	static constexpr float defaultRailsTimeScale = 8.f;
	/// Lowest accepted `railsTimeScale`.
	static constexpr float minRailsTimeScale = 1.f;
	/// Highest accepted `railsTimeScale`.
	static constexpr float maxRailsTimeScale = 64.f;
	/// Largest accepted component of `gravity`.
	static constexpr float maxGravity = 1000.f;
	/// Highest accepted solver iteration count (velocity or position).
	static constexpr uint32_t maxSolverIterations = 64;

	/// Jolt collision steps inside one fixed step.
	uint32_t solverSubSteps = 2;
	/// Blend the rendered transforms between the last two fixed steps.
	bool interpolate = true;
	/// Solver threads: 0 picks a count from the hardware, 1 runs the solver on the calling thread.
	uint32_t workerCount = 0;
	/// Acceleration of every dynamic body, summed with the `GravitySource` fields, in world units per second squared.
	math::vec3f gravity{0.f, -9.81f, 0.f};
	/// Scene time scale above which the moving bodies leave the solver for their conic arcs (time warp).
	float railsTimeScale = defaultRailsTimeScale;
	/// Velocity iterations of the solver in each collision step: more keep long chains of joints stiff.
	uint32_t velocitySteps = 10;
	/// Position iterations of the solver in each collision step, against the drift of the joints.
	uint32_t positionSteps = 2;

	/**
	 * @brief
	 *  Copy of these settings with every field brought into its accepted range.
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto clamped() const -> PhysicsSettings;

	/**
	 * @brief
	 *  Solver thread count the world will use.
	 * @param[in] iDynamicBodies Number of dynamic bodies in the world, used by the automatic count.
	 * @return `workerCount` when set; otherwise 1 below `autoThreadingMinBodies` dynamic bodies, else half the
	 * hardware threads, at most `maxAutoWorkerCount`. Always at least 1.
	 */
	[[nodiscard]] auto getEffectiveWorkerCount(uint32_t iDynamicBodies) const -> uint32_t;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The settings to compare with.
	 * @return True when all fields are equal.
	 */
	auto operator==(const PhysicsSettings& iOther) const -> bool;
};

}// namespace owl::scene
