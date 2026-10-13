/**
 * @file PhysicsSettings.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

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
	/// Highest accepted Box2D sub-step count.
	static constexpr uint32_t maxSolverSubSteps = 16;
	/// Highest accepted solver thread count.
	static constexpr uint32_t maxWorkerCount = 32;
	/// Upper bound of the automatic solver thread count.
	static constexpr uint32_t maxAutoWorkerCount = 4;
	/// Dynamic bodies below which the automatic count stays single-threaded (task dispatch costs more than it saves).
	static constexpr uint32_t autoThreadingMinBodies = 2000;

	/// Box2D sub-steps inside one fixed step.
	uint32_t solverSubSteps = 4;
	/// Blend the rendered transforms between the last two fixed steps.
	bool interpolate = true;
	/// Solver threads: 0 picks a count from the hardware, 1 runs the solver on the calling thread.
	uint32_t workerCount = 0;

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
