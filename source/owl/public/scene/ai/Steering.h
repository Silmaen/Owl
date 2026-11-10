/**
 * @file Steering.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/RandomStream.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>

/**
 * @brief
 *  Steering behaviours: the accelerations that move an agent towards, away from or around other bodies.
 *
 * Every function returns a steering acceleration: the velocity the behaviour wants minus the current velocity. The
 * `owl.steering` system weighs them, keeps the sum under the highest acceleration of the agent and integrates it. The
 * functions only use `+ - * /` and `sqrt`, so the same input gives the same bits on every target.
 */
namespace owl::scene::ai {

/**
 * @brief
 *  The space an agent moves in.
 */
enum struct SteeringPlane : uint8_t {
	Plane2D,///< The plane (x, y) of the 2D scenes: z stays put.
	Ground,///< The ground (x, z) of the 3D scenes: y stays put.
	Space///< The three axes: a ship, a bird, a fish.
};

/**
 * @brief
 *  A moving sphere: an agent, a neighbour, a volume to avoid.
 */
struct OWL_API SteeringBody {
	/// Centre, world units.
	math::vec3 position;
	/// Velocity, world units per second.
	math::vec3 velocity;
	/// Radius of the sphere (of the disc on a plane).
	float radius = 0.5f;
};

/**
 * @brief
 *  Tuning of the wander behaviour: a point jittered on a circle (a sphere in space) ahead of the agent.
 */
struct OWL_API WanderSettings {
	/// Radius of the circle.
	float radius = 1.f;
	/// Distance of the circle ahead of the agent.
	float distance = 2.f;
	/// Largest change of the wander angle per second, in radians.
	float jitter = 2.f;
};

/**
 * @brief
 *  The wander point of an agent, kept from one tick to the next.
 */
struct OWL_API WanderState {
	/// Angle on the circle, radians.
	float heading = 0.f;
	/// Elevation on the sphere in space, radians.
	float pitch = 0.f;
};

/**
 * @brief
 *  Remove the part of a vector that leaves the plane of the agent.
 * @param[in] iVector The vector.
 * @param[in] iPlane The plane.
 * @return The vector on the plane (unchanged in space).
 */
[[nodiscard]] OWL_API auto projectOnPlane(const math::vec3& iVector, SteeringPlane iPlane) -> math::vec3;

/**
 * @brief
 *  Shorten a vector to a largest length.
 * @param[in] iVector The vector.
 * @param[in] iMax The largest length.
 * @return The vector, shortened when longer than `iMax`.
 */
[[nodiscard]] OWL_API auto truncate(const math::vec3& iVector, float iMax) -> math::vec3;

/**
 * @brief
 *  Go straight to a point at full speed.
 * @param[in] iAgent The agent.
 * @param[in] iTarget The point.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @return The steering acceleration.
 */
[[nodiscard]] OWL_API auto seek(const SteeringBody& iAgent, const math::vec3& iTarget, float iMaxSpeed) -> math::vec3;

/**
 * @brief
 *  Run straight away from a point while it is closer than the panic distance.
 * @param[in] iAgent The agent.
 * @param[in] iThreat The point.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @param[in] iPanicDistance Distance under which the agent runs; 0 for always.
 * @return The steering acceleration, zero beyond the panic distance.
 */
[[nodiscard]] OWL_API auto flee(const SteeringBody& iAgent, const math::vec3& iThreat, float iMaxSpeed,
								float iPanicDistance) -> math::vec3;

/**
 * @brief
 *  Go to a point and stop on it, slowing down inside the slowing radius.
 * @param[in] iAgent The agent.
 * @param[in] iTarget The point.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @param[in] iSlowRadius Distance from the point at which the agent starts to slow down.
 * @return The steering acceleration.
 */
[[nodiscard]] OWL_API auto arrive(const SteeringBody& iAgent, const math::vec3& iTarget, float iMaxSpeed,
								  float iSlowRadius) -> math::vec3;

/**
 * @brief
 *  Seek the point a moving body will reach: its position plus its velocity times the time the agent needs to get
 *  there, at most `iMaxPrediction`.
 * @param[in] iAgent The agent.
 * @param[in] iQuarry The body chased.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @param[in] iMaxPrediction Longest look ahead, in seconds.
 * @return The steering acceleration.
 */
[[nodiscard]] OWL_API auto pursue(const SteeringBody& iAgent, const SteeringBody& iQuarry, float iMaxSpeed,
								  float iMaxPrediction) -> math::vec3;

/**
 * @brief
 *  Flee the point a moving body will reach (the mirror of `pursue`).
 * @param[in] iAgent The agent.
 * @param[in] iThreat The body evaded.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @param[in] iMaxPrediction Longest look ahead, in seconds.
 * @param[in] iPanicDistance Distance under which the agent runs; 0 for always.
 * @return The steering acceleration.
 */
[[nodiscard]] OWL_API auto evade(const SteeringBody& iAgent, const SteeringBody& iThreat, float iMaxSpeed,
								 float iMaxPrediction, float iPanicDistance) -> math::vec3;

/**
 * @brief
 *  Stroll: seek a point that wanders on a circle ahead of the agent (a sphere in space).
 * @param[in] iAgent The agent.
 * @param[in,out] ioState The wander point, jittered by this call.
 * @param[in] iSettings Tuning.
 * @param[in] iPlane The space of the agent.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @param[in] iSeconds Duration of the tick.
 * @param[in,out] ioRandom Stream of the jitter.
 * @return The steering acceleration.
 */
[[nodiscard]] OWL_API auto wander(const SteeringBody& iAgent, WanderState& ioState, const WanderSettings& iSettings,
								  SteeringPlane iPlane, float iMaxSpeed, float iSeconds, math::RandomStream& ioRandom)
		-> math::vec3;

/**
 * @brief
 *  Keep away from the neighbours closer than a distance, harder the closer they are (flocking).
 * @param[in] iAgent The agent.
 * @param[in] iNeighbours The other members of the flock around it.
 * @param[in] iDistance Distance between the surfaces under which a neighbour pushes.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @return The steering acceleration, zero without a neighbour in range.
 */
[[nodiscard]] OWL_API auto separation(const SteeringBody& iAgent, std::span<const SteeringBody> iNeighbours,
									  float iDistance, float iMaxSpeed) -> math::vec3;

/**
 * @brief
 *  Match the mean heading of the neighbours (flocking).
 * @param[in] iAgent The agent.
 * @param[in] iNeighbours The other members of the flock around it.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @return The steering acceleration, zero without a neighbour.
 */
[[nodiscard]] OWL_API auto alignment(const SteeringBody& iAgent, std::span<const SteeringBody> iNeighbours,
									 float iMaxSpeed) -> math::vec3;

/**
 * @brief
 *  Seek the centre of the neighbours (flocking).
 * @param[in] iAgent The agent.
 * @param[in] iNeighbours The other members of the flock around it.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @return The steering acceleration, zero without a neighbour.
 */
[[nodiscard]] OWL_API auto cohesion(const SteeringBody& iAgent, std::span<const SteeringBody> iNeighbours,
									float iMaxSpeed) -> math::vec3;

/**
 * @brief
 *  Steer around the moving volumes the agent would hit within a time: the one met first, at the time of the closest
 *  approach of the two spheres, pushes the agent sideways out of its way.
 * @param[in] iAgent The agent.
 * @param[in] iVolumes The volumes around it (other agents, obstacles).
 * @param[in] iLookAhead Time ahead the agent looks, in seconds.
 * @param[in] iMaxSpeed Highest speed of the agent.
 * @return The steering acceleration, zero when nothing is in the way.
 */
[[nodiscard]] OWL_API auto avoidVolumes(const SteeringBody& iAgent, std::span<const SteeringBody> iVolumes,
										float iLookAhead, float iMaxSpeed) -> math::vec3;

}// namespace owl::scene::ai
