/**
 * @file SteeringAgent.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Macros.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"
#include "scene/ai/Steering.h"

#include <string>
#include <string_view>

namespace owl::scene::component {

/**
 * @brief
 *  Weight of each steering behaviour of an agent; 0 leaves a behaviour out.
 */
struct OWL_API SteeringWeights {
	/// Go straight to the target.
	float seek = 0.f;
	/// Run away from the target while closer than the panic distance.
	float flee = 0.f;
	/// Go to the target and stop on it.
	float arrive = 0.f;
	/// Chase the point the target will reach.
	float pursue = 0.f;
	/// Run away from the point the target will reach.
	float evade = 0.f;
	/// Stroll at random.
	float wander = 0.f;
	/// Keep away from the members of the flock.
	float separation = 0.f;
	/// Head the way the members of the flock head.
	float alignment = 0.f;
	/// Stay with the members of the flock.
	float cohesion = 0.f;
	/// Steer around the other agents and the `NavObstacle`s.
	float avoidance = 1.f;
	/// Hold the slot of the formation around the leader.
	float formation = 0.f;

	/**
	 * @brief
	 *  The weight of a behaviour by its name (`seek`, `flee`, `arrive`, `pursue`, `evade`, `wander`, `separation`,
	 *  `alignment`, `cohesion`, `avoidance`, `formation`).
	 * @param[in] iName The name of the behaviour.
	 * @return The weight, null for an unknown name.
	 */
	[[nodiscard]] auto find(std::string_view iName) -> float*;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Compare two sets of weights.
	 * @param[in] iOther The other set.
	 * @return True when every weight is the same.
	 */
	auto operator==(const SteeringWeights& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  Moves an entity by steering behaviours: seek, flee, arrive, pursue, evade, wander, flocking, avoidance of the moving
 *  volumes and formations, on the 2D plane, on the ground or in open 3D space.
 *
 * The `owl.steering` system (`PrePhysics`, fixed tick only) adds the weighted behaviours, keeps the sum under
 * `maxAcceleration`, the velocity under `maxSpeed`, and moves the `Transform`. The target is the `target` entity, else
 * `targetPoint`. The flock of an agent is the agents of the same `flock` closer than `neighbourRadius`. A behaviour
 * tree or Lua (`steer.*`) changes the weights and the target while the game runs.
 */
struct OWL_API SteeringAgent {
	/// Whether the agent moves; off leaves it where it is.
	bool enabled = true;
	/// Space the agent moves in.
	ai::SteeringPlane plane = ai::SteeringPlane::Ground;
	/// Highest speed, world units per second.
	float maxSpeed = 4.f;
	/// Largest change of velocity per second.
	float maxAcceleration = 12.f;
	/// Radius of the agent, kept clear of the others.
	float radius = 0.5f;
	/// Turn the entity the way it moves (its front is +x in 2D, +z on the ground and in space).
	bool turn = true;
	/// Entity sought, fled, pursued or evaded; none: `targetPoint`.
	core::UUID target{0};
	/// World point sought, fled or reached when no `target` is set.
	math::vec3 targetPoint{0.f, 0.f, 0.f};
	/// Weight of each behaviour.
	SteeringWeights weights;
	/// Distance from the target at which `arrive` starts to slow down.
	float slowRadius = 2.f;
	/// Distance under which `flee` and `evade` run; 0 for always.
	float panicDistance = 8.f;
	/// Longest look ahead of `pursue` and `evade`, in seconds.
	float prediction = 1.f;
	/// Tuning of `wander`.
	ai::WanderSettings wanderSettings;
	/// Name of the flock: the agents of the same flock flock together.
	std::string flock;
	/// Distance under which the members of the flock are neighbours.
	float neighbourRadius = 4.f;
	/// Gap between two members under which `separation` pushes them apart.
	float separationDistance = 1.f;
	/// Time ahead `avoidance` looks for a collision, in seconds.
	float lookAhead = 1.5f;
	/// Entity leading the formation; none: no formation.
	core::UUID leader{0};
	/// Slot of the agent in the formation, in the frame of the leader (its front is +z, +x in 2D).
	math::vec3 slot{0.f, 0.f, -2.f};

	/// Runtime: velocity of the last tick.
	math::vec3 m_velocity{0.f, 0.f, 0.f};
	/// Runtime: wander point.
	ai::WanderState m_wander;
	/// Runtime: last position of the target, to read its velocity.
	math::vec3 m_targetLast{0.f, 0.f, 0.f};
	/// Runtime: entity whose position `m_targetLast` holds.
	core::UUID m_targetTracked{0};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Steering Agent"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SteeringAgent"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
