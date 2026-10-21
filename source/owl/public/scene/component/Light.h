/**
 * @file Light.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>
#include <numbers>

namespace owl::scene::component {

/**
 * @brief
 *  A light of the 3D layers (meshes and voxel worlds). It shines along the -Z axis of its entity's world transform;
 *  a point light ignores the rotation, a directional light the position.
 */
struct OWL_API Light {
	/**
	 * @brief
	 *  Kind of light.
	 */
	enum struct Type : uint8_t {
		Directional,///< Parallel rays from infinitely far (the sun); the only kind casting shadows.
		Point,///< Rays from a point in every direction, fading to zero at `range`.
		Spot,///< A point light restricted to a cone, softened between `innerAngle` and `outerAngle`.
	};

	/// Kind of light.
	Type type = Type::Directional;
	/// Colour, sRGB as authored.
	math::vec3 color{1.f, 1.f, 1.f};
	/// Multiplier of the colour.
	float intensity = 1.f;
	/// Distance where a point or spot light reaches zero, in world units.
	float range = 10.f;
	/// Half angle of the full-intensity cone of a spot light, in radians.
	float innerAngle = 20.f * std::numbers::pi_v<float> / 180.f;
	/// Half angle where a spot light reaches zero, in radians.
	float outerAngle = 30.f * std::numbers::pi_v<float> / 180.f;
	/// Whether a directional light casts shadows (the first one of the scene does).
	bool castShadows = true;
	/// Distance from the camera the shadows cover, in world units.
	float shadowDistance = 60.f;
	/// Number of shadow cascades, from 1 to 4.
	uint32_t cascadeCount = 4;
	/// Size of each cascade's shadow map, in texels.
	uint32_t shadowResolution = 1024;
	/// Depth bias against self-shadowing, in shadow-map texels along the surface normal.
	float shadowBias = 1.5f;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Light"; }

	/**
	 * @brief
	 *  Key of the component in the scene files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "Light"; }

	/**
	 * @brief
	 *  Write the component.
	 * @param[in] iOut Serializer to write into.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read the component; missing keys keep their defaults.
	 * @param[in] iNode Serializer to read from.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
