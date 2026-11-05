/**
 * @file SceneJoint.h
 * @author Silmaen
 * @date 05/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene {

/**
 * @brief
 *  A joint tying the physics body of an entity to another body, or to the world.
 */
struct OWL_API SceneJoint {
	/// What the joint lets the two bodies do.
	enum struct Type : uint8_t {
		Fixed,///< Weld: the bodies move as one.
		Distance,///< The anchors keep their distance (a rod, or a spring with `springFrequency`).
		Hinge,///< The bodies turn around the anchor, along `axis` (around Z for a 2D body).
		Slider///< The bodies slide along `axis`, without turning.
	};
	/// What the joint lets the two bodies do.
	Type type = Type::Hinge;
	/// Entity whose body is joined (0: the world).
	core::UUID connected{0};
	/// Joint point, in the frame of the entity.
	math::vec3f anchor{0.f, 0.f, 0.f};
	/// Distance joint: the point on the connected body, in its frame (in the world without one).
	math::vec3f connectedAnchor{0.f, 0.f, 0.f};
	/// Hinge axis or slide direction, in the frame of the entity; a 2D hinge turns around Z.
	math::vec3f axis{0.f, 0.f, 1.f};
	/// Whether the motion is bounded by `lowerLimit` and `upperLimit`.
	bool limited = false;
	/// Smallest angle of a hinge (radians), position of a slider or length of a distance joint.
	float lowerLimit = 0.f;
	/// Largest angle of a hinge (radians), position of a slider or length of a distance joint.
	float upperLimit = 0.f;
	/// Whether a motor drives a hinge or a slider to `motorSpeed`.
	bool motor = false;
	/// Target speed of the motor, radians or units per second.
	float motorSpeed = 0.f;
	/// Largest torque or force of the motor.
	float motorForce = 100.f;
	/// Distance joint: frequency of the spring in Hz, 0 for a rigid rod.
	float springFrequency = 0.f;
	/// Distance joint: damping ratio of the spring, 0 to 1.
	float springDamping = 0.f;
	/// Runtime: the joint in the physics world, 0 when not built.
	uint64_t jointId = 0;
};

}// namespace owl::scene
