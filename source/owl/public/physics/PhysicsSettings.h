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

namespace owl::physics {

/**
 * @brief
 *  Per-scene physics simulation settings, saved in the scene file under `Physics:`.
 *
 * The world advances at a fixed rate whatever the frame rate: each frame adds its duration to an
 * accumulator and runs as many steps of `1 / tickRate` seconds as it holds, at most
 * `maxStepsPerFrame`. Rendered transforms are blended between the last two steps when `interpolate`
 * is set.
 */
struct OWL_API PhysicsSettings {
	/// Default number of fixed steps per second.
	static constexpr float defaultTickRate = 60.f;
	/// Lowest accepted tick rate, in steps per second.
	static constexpr float minTickRate = 1.f;
	/// Highest accepted tick rate, in steps per second.
	static constexpr float maxTickRate = 1000.f;
	/// Highest accepted step count per frame.
	static constexpr uint32_t maxStepsLimit = 64;
	/// Highest accepted Box2D sub-step count.
	static constexpr uint32_t maxSolverSubSteps = 16;
	/// Highest accepted solver thread count.
	static constexpr uint32_t maxWorkerCount = 32;
	/// Upper bound of the automatic solver thread count.
	static constexpr uint32_t maxAutoWorkerCount = 4;
	/// Dynamic bodies below which the automatic count stays single-threaded (task dispatch costs more than it saves).
	static constexpr uint32_t autoThreadingMinBodies = 2000;

	/// Fixed steps per second.
	float tickRate = defaultTickRate;
	/// Maximum number of fixed steps per frame; the time beyond is dropped (no spiral of death).
	uint32_t maxStepsPerFrame = 8;
	/// Box2D sub-steps inside one fixed step.
	uint32_t solverSubSteps = 4;
	/// Blend the rendered transforms between the last two fixed steps.
	bool interpolate = true;
	/// Solver threads: 0 picks a count from the hardware, 1 runs the solver on the calling thread.
	uint32_t workerCount = 0;

	/**
	 * @brief
	 *  Duration of one fixed step.
	 * @return The step duration in seconds, from the clamped tick rate.
	 */
	[[nodiscard]] auto getStepSeconds() const -> double;

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

}// namespace owl::physics
