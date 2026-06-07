/**
 * @file FlyCamera.h
 * @author Silmaen
 * @date 06/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief
 *  Free-fly camera controller attached to a camera entity.
 *
 * When present on an entity that also carries a `Camera`, the scene drives the
 * entity transform from keyboard input each runtime frame via a
 * `renderer::Camera3DController`: WASD moves in the facing plane, Space/E rises
 * and Left-Shift/Q descends, and the arrow keys look around. It replaces the
 * ad-hoc fly-camera Lua script for exploring 3D scenes (voxel worlds) in Play.
 * Only the tunable speeds are authored; the orientation state lives in the
 * entity transform.
 */
struct OWL_API FlyCamera {
	/// Movement speed in world units per second.
	float moveSpeed = 8.0f;
	/// Look (rotation) speed in radians per second.
	float lookSpeed = 1.5f;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Fly Camera"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "FlyCamera"; }

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
