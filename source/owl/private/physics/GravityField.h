/**
 * @file GravityField.h
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "math/Orbit.h"
#include "scene/component/GravitySource.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace owl::scene {
class Scene;
}// namespace owl::scene

namespace owl::physics {

/**
 * @brief
 *  A `GravitySource` of a scene, where it stands at the start of a step.
 */
struct FieldSource {
	/// UUID of the entity of the source.
	core::UUID owner;
	/// World position of the entity.
	math::vec3f position{0.f, 0.f, 0.f};
	/// Velocity of the entity's body, zero without one.
	math::vec3f velocity{0.f, 0.f, 0.f};
	/// The field.
	scene::component::GravitySource field;
};

/**
 * @brief
 *  The enabled gravity sources of a scene, in UUID order so every sum runs in the same order.
 * @param[in] iScene The scene.
 * @param[in] iVelocityOf Velocity of the body of an entity, for the sources that move; empty for none.
 * @param[out] oSources The sources, cleared first.
 */
void gatherFieldSources(const scene::Scene& iScene, const std::function<math::vec3f(core::UUID)>& iVelocityOf,
						std::vector<FieldSource>& oSources);

/**
 * @brief
 *  Sum of the fields at a point.
 * @param[in] iSources The sources.
 * @param[in] iPoint The point, in world units.
 * @param[in] iExclude Entity whose own field is left out (a planet does not pull itself).
 * @return The acceleration, in world units per second squared.
 */
[[nodiscard]] auto fieldAcceleration(std::span<const FieldSource> iSources, const math::vec3f& iPoint,
									 core::UUID iExclude) -> math::vec3f;

/**
 * @brief
 *  How a body moves on rails during a time warp: around the strongest point or spherical source, or under the uniform
 *  pull.
 */
struct RailsArc {
	/// Index of the source the body orbits; none for the uniform pull.
	std::optional<size_t> source;
	/// Uniform pull without a source: the world gravity and the uniform fields, scaled.
	math::vec3d acceleration{0.0, 0.0, 0.0};
	/// Gravitational parameter of the orbited source, scaled.
	double gravitationalParameter = 0.0;
};

/**
 * @brief
 *  What a body follows on rails: the source pulling it hardest, or the uniform pull when that pull is stronger.
 * @param[in] iSources The sources.
 * @param[in] iWorldGravity The world gravity of the scene.
 * @param[in] iPoint Where the body is.
 * @param[in] iExclude The entity of the body.
 * @param[in] iScale Gravity factor of the body.
 * @param[in] iFields The body feels the fields.
 * @return The arc.
 */
[[nodiscard]] auto railsArc(std::span<const FieldSource> iSources, const math::vec3f& iWorldGravity,
							const math::vec3f& iPoint, core::UUID iExclude, float iScale, bool iFields) -> RailsArc;

/**
 * @brief
 *  Carry a state relative to the orbited source along an arc; a body reaching the surface of the source stops on it.
 * @param[in] iArc The arc.
 * @param[in] iSources The sources of the arc.
 * @param[in] iRelative The state, relative to the source for an orbit, absolute for the uniform pull.
 * @param[in] iSeconds Duration.
 * @return The state after the duration, in the same frame.
 */
[[nodiscard]] auto followArc(const RailsArc& iArc, std::span<const FieldSource> iSources,
							 const math::OrbitState& iRelative, double iSeconds) -> math::OrbitState;

/**
 * @brief
 *  What a trajectory prediction integrates.
 */
struct PathSettings {
	/// The world gravity of the scene.
	math::vec3f worldGravity{0.f, -9.81f, 0.f};
	/// Gravity factor of the body.
	float gravityScale = 1.f;
	/// The body feels the fields.
	bool fields = true;
	/// A 2D body: the motion stays in its XY plane.
	bool plane = false;
	/// Duration of a fixed step.
	float stepSeconds = 1.f / 60.f;
	/// Collision steps inside a fixed step, as the solver integrates them.
	uint32_t subSteps = 1;
	/// Entity of the body, its own field left out.
	core::UUID self;
};

/**
 * @brief
 *  Future positions of a body under the fields, integrated like the solver does, collisions ignored.
 * @param[in] iSources The sources, considered still.
 * @param[in] iSettings What is integrated.
 * @param[in] iPosition Start position.
 * @param[in] iVelocity Start velocity.
 * @param[in] iSteps Number of fixed steps.
 * @return The position after each step.
 */
[[nodiscard]] auto predictPath(std::span<const FieldSource> iSources, const PathSettings& iSettings,
							   const math::vec3f& iPosition, const math::vec3f& iVelocity, uint32_t iSteps)
		-> std::vector<math::vec3f>;

}// namespace owl::physics
