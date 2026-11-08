/**
 * @file GravitySource.h
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {

/**
 * @brief
 *  A gravity field centred on the entity, pulling the dynamic physics bodies of the scene: a planet, a moon, a black
 *  hole, a gravity zone.
 *
 * Every field reaching a body is summed with the world gravity of the scene (`PhysicsSettings::gravity`), scaled by the
 * gravity scale of the body; a body with `PhysicBody` `gravityFields` off feels the world gravity only. A 2D body takes
 * the X and Y of the pull. The fields are read at every fixed step, from the world position of the entity.
 */
struct OWL_API GravitySource {
	/// How the pull varies around the entity.
	enum struct Type : uint8_t {
		Uniform,///< The same pull along `direction` everywhere in reach: a gravity zone, a wind tunnel.
		Point,///< Toward the entity, `strength` at `radius` falling with the square of the distance, capped inside.
		Spherical///< As `Point` outside `radius`; inside, a solid ball pulling less toward its centre.
	};
	/// How the pull varies around the entity.
	Type type = Type::Spherical;
	/// Whether the field pulls; off leaves the bodies to the other fields.
	bool enabled = true;
	/// Pull in world units per second squared: everywhere for `Uniform`, at `radius` otherwise.
	float strength = 9.81f;
	/// Distance from the entity where the pull is `strength` (the surface of a planet), in world units.
	float radius = 10.f;
	/// Distance beyond which the field pulls nothing, in world units; 0 reaches the whole scene.
	float range = 0.f;
	/// Direction of a `Uniform` pull, in world space (normalised when read).
	math::vec3f direction{0.f, -1.f, 0.f};

	/// Lowest accepted `radius`.
	static constexpr float minRadius = 0.001f;

	/**
	 * @brief
	 *  Gravitational parameter of a `Point` or `Spherical` source (the mass times the gravitational constant).
	 * @return `strength` times the square of `radius`, in cubed world units per second squared.
	 */
	[[nodiscard]] auto getGravitationalParameter() const -> double;

	/**
	 * @brief
	 *  Pull of the field at an offset from the entity.
	 * @param[in] iOffset Position of the point minus the position of the entity, in world units.
	 * @return The acceleration, in world units per second squared; zero when off or out of `range`.
	 */
	[[nodiscard]] auto getAcceleration(const math::vec3f& iOffset) const -> math::vec3f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Gravity Source"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "GravitySource"; }

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
