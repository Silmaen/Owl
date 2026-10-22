/**
 * @file FollowTarget.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "core/UUID.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  Keeps an entity at a fixed offset from another one, turning with its heading or not: the camera of a minimap above
 *  the player, a light carried by a character, a marker over a vehicle.
 *
 * The `owl.follow_target` system places the entity at the world position of `target` plus `offset`. With `followYaw`,
 * the yaw of the entity (its rotation about Y) is the yaw of the target plus the yaw of its own `Transform` at the
 * start, its pitch and roll kept, so a camera looking down turns with the player without tilting with its view. The
 * entity is a root of the hierarchy: its `Transform` is written as a world transform.
 */
struct OWL_API FollowTarget {
	/// Whether the entity follows; off leaves it where it is.
	bool enabled = true;
	/// Entity followed (0: none).
	core::UUID target{0};
	/// Offset from the target, in world units, along the world axes.
	math::vec3 offset{0.f, 20.f, 0.f};
	/// Whether the entity turns with the heading of the target.
	bool followYaw = true;

	/// Runtime: the rest yaw was read.
	bool m_captured = false;
	/// Runtime: yaw of the entity at the start.
	float m_baseYaw = 0.f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Follow Target"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "FollowTarget"; }

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
