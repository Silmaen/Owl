/**
 * @file SceneBody.h
 * @author Silmaen
 * @date 12/30/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene {
/**
 * @brief
 *  Class describing a physical body in the scene.
 */
class OWL_API SceneBody final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SceneBody();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~SceneBody() = default;

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	SceneBody(const SceneBody&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SceneBody(SceneBody&&) = default;

	/**
	 * @brief
	 *  Default copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const SceneBody&) -> SceneBody& = default;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(SceneBody&&) -> SceneBody& = default;

	/// The type of physical body
	enum struct BodyType : uint8_t {
		Static,///< Static but affected by physic.
		Dynamic,///< Position dynamically computed.
		Kinematic///< Kinematic position.
	};
	/// The type of body.
	BodyType type = BodyType::Dynamic;

	/// Where the body moves.
	enum struct Dimension : uint8_t {
		Plane,///< 2D: locked to the XY plane, rotating around Z.
		Space///< 3D: free in space.
	};
	/// Where the body moves.
	Dimension dimension = Dimension::Plane;

	/// Shape of the collider.
	enum struct Shape : uint8_t {
		Box,///< Box of `colliderSize`.
		Sphere,///< Sphere (a disc in 2D) of diameter `colliderSize.x`.
		Capsule,///< Capsule along Y, of diameter `colliderSize.x` and full height `colliderSize.y`.
		Cylinder,///< Cylinder along Y, of diameter `colliderSize.x` and height `colliderSize.y` (a rectangle in 2D).
		ConvexHull,///< 3D: the convex hull of the mesh of the entity's `MeshRenderer`, scaled like the entity.
		Mesh///< 3D, static or kinematic: the triangles of the mesh of the entity's `MeshRenderer`.
	};
	/// Shape of the collider.
	Shape shape = Shape::Box;

	/// Mask of a body colliding with every layer.
	static constexpr uint8_t allLayers = 0xff;
	/// Number of collision layers.
	static constexpr uint8_t layerCount = 8;

	/// If the body can rotate.
	bool fixedRotation = false;
	/// A sensor detects the bodies entering it (`on_trigger_enter`, `on_trigger_exit`) without stopping them.
	bool sensor = false;
	/// Collision layer of the body, 0 to `layerCount - 1`.
	uint8_t layer = 0;
	/// Layers the body collides with, one bit per layer; two bodies collide when each one's mask has the other's layer.
	uint8_t mask = allLayers;
	/**
	 * A part: the collider merges into the body of the nearest ancestor with a `PhysicBody` (one rigid assembly, mass
	 * and centre of mass summed) instead of making a body of its own; its own type is ignored.
	 */
	bool part = false;
	/// Internal body index.
	uint64_t bodyId = 0;
	/// The size of the collider (see `Shape`), multiplied by the entity scale; a 2D box ignores Z (unit depth).
	math::vec3f colliderSize{1, 1, 1};
	/// The density of the material.
	float density{1.f};
	/// The restitution coefficient in case of contact.
	float restitution{0};
	/// The friction coefficient for contact.
	float friction{0.5f};
	/// Share of the gravity the body feels, the world gravity and the `GravitySource` fields: 0 for none.
	float gravityScale{1.f};
	/// The body feels the `GravitySource` fields; off leaves it to the world gravity only.
	bool gravityFields = true;
};

}// namespace owl::scene
