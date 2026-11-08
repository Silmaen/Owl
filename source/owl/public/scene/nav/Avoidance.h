/**
 * @file Avoidance.h
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <span>
#include <vector>

namespace owl::scene::nav {

/**
 * @brief
 *  A moving disc of the avoidance, on the plane (x, z) given as (x, y).
 */
struct OWL_API AvoidanceBody {
	/// Centre of the disc.
	math::vec2 position;
	/// Velocity of the last step.
	math::vec2 velocity;
	/// Velocity it wants: towards the next corner of its path.
	math::vec2 preferred;
	/// Radius of the disc.
	float radius = 0.5f;
	/// Highest speed.
	float maxSpeed = 2.f;
	/// Whether it steers away from the others; an obstacle does not, the others take the whole avoidance.
	bool responsive = true;
};

/**
 * @brief
 *  Tuning of the avoidance.
 */
struct OWL_API AvoidanceSettings {
	/// Time ahead the bodies look for a collision, in seconds: longer is smoother but shyer.
	float timeHorizon = 1.5f;
	/// Distance from a body under which the others are taken into account.
	float neighbourDistance = 6.f;
};

/**
 * @brief
 *  Compute the velocities that keep the bodies apart (optimal reciprocal collision avoidance, ORCA).
 *
 * Each responsive body takes half of the avoidance of every responsive neighbour and all of it against an obstacle,
 * and gets the velocity nearest to its preferred one that stays out of their velocity obstacles. The bodies are
 * treated in the order given, so the same input gives the same velocities on every target.
 * @param[in] iBodies The bodies.
 * @param[in] iSettings Tuning.
 * @param[in] iTimeStep Duration of the step, in seconds.
 * @return The new velocity of each body (an obstacle keeps its velocity).
 */
OWL_API auto computeAvoidance(std::span<const AvoidanceBody> iBodies, const AvoidanceSettings& iSettings,
							  float iTimeStep) -> std::vector<math::vec2>;

}// namespace owl::scene::nav
