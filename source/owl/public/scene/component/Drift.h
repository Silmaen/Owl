/**
 * @file Drift.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief
 *  Makes an entity drift at a constant velocity, wrapping around a range, optionally following the camera: a cloud, a
 *  floating mote, a parallax layer.
 *
 * The `owl.drift` system places the entity at `cameraFollow` times the position of the primary camera, plus the
 * rest position (the `Transform` of the scene), plus the drift. The drift grows by `velocity` each second and an axis
 * with a `range` jumps from `+range` back to `-range`.
 */
struct OWL_API Drift {
	/// Whether the entity drifts; off leaves it at its rest position.
	bool enabled = true;
	/// Drift velocity, in world units per second.
	math::vec3 velocity{0.3f, 0.f, 0.f};
	/// Half extent of the drift along each axis, in world units; 0 for no wrap.
	math::vec3 range{20.f, 0.f, 0.f};
	/// Share of the camera movement the entity follows: 0 pins it to the world, 1 to the screen.
	float cameraFollow = 0.f;

	/// Runtime: the rest position was read.
	bool m_captured = false;
	/// Runtime: rest position.
	math::vec3 m_basePosition{0.f, 0.f, 0.f};
	/// Runtime: drift done.
	math::vec3 m_drift{0.f, 0.f, 0.f};

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Drift"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Drift"; }

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
