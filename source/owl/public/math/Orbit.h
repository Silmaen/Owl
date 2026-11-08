/**
 * @file Orbit.h
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

/**
 * @brief
 *  Namespace for mathematical functions.
 */
namespace owl::math {

/**
 * @brief
 *  Position and velocity of a body relative to the centre it falls toward.
 */
struct OWL_API OrbitState {
	/// Position, in world units.
	vec3d position{0.0, 0.0, 0.0};
	/// Velocity, in world units per second.
	vec3d velocity{0.0, 0.0, 0.0};
};

/**
 * @brief
 *  Carry a body along its conic arc (ellipse, parabola or hyperbola) around a point mass, in closed form.
 *
 * Kepler's problem in universal variables: the result does not drift with the duration, a body on an ellipse comes back
 * to the same point every period. The maths are the deterministic `math::sin`, `cos` and `exp`: the same bits on every
 * target.
 * @param[in] iState The state relative to the centre.
 * @param[in] iGravitationalParameter Mass of the centre times the gravitational constant (`GravitySource`
 *  `getGravitationalParameter()`); 0 or less moves the body in a straight line.
 * @param[in] iSeconds Duration, in seconds; negative goes back in time.
 * @return The state after the duration; the straight line when the arc cannot be computed (body at the centre, an
 *  overflowing hyperbola).
 */
[[nodiscard]] OWL_API auto propagateKepler(const OrbitState& iState, double iGravitationalParameter, double iSeconds)
		-> OrbitState;

/**
 * @brief
 *  Carry a body along its parabola under a constant acceleration, in closed form.
 * @param[in] iState The state.
 * @param[in] iAcceleration The acceleration, in world units per second squared.
 * @param[in] iSeconds Duration, in seconds.
 * @return The state after the duration.
 */
[[nodiscard]] OWL_API auto propagateUniform(const OrbitState& iState, const vec3d& iAcceleration, double iSeconds)
		-> OrbitState;

}// namespace owl::math
