/**
 * @file Light2D.h
 * @author Silmaen
 * @date 22/10/2026
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
 *  A light of the 2D layers: sprites and tilemaps of the scene are lit by the sum of the ambient lights and of the
 *  point and spot lights reaching them.
 * @details
 *  The values are artistic, not physical: a colour times an intensity. A scene without any `Light2D` draws its sprites
 *  unlit; a scene with lights but no `Ambient` one keeps a white ambient, so lights only add. A point or spot light
 *  sits at the world position of its entity; a spot points along the local +X axis of the entity (its Z rotation).
 */
struct OWL_API Light2D {
	/**
	 * @brief
	 *  Kind of 2D light.
	 */
	enum struct Type : uint8_t {
		Ambient,///< The same light on every lit pixel of the scene: the day and night tint.
		Point,///< Light from a point in every direction, fading to zero at `radius`.
		Spot,///< A point light restricted to a cone, softened between `innerAngle` and `outerAngle`.
	};

	/// Kind of light.
	Type type = Type::Point;
	/// Whether the light shines; a light switched off keeps its settings.
	bool enabled = true;
	/// Colour, sRGB as authored.
	math::vec3 color{1.f, 1.f, 1.f};
	/// Multiplier of the colour (1 for the colour as is).
	float intensity = 1.f;
	/// Distance where a point or spot light reaches zero, in world units.
	float radius = 5.f;
	/// Exponent of the fade `(1 - distance / radius)`: 1 linear, 2 brighter near the light and softer at the edge.
	float falloff = 2.f;
	/// Height of the light above the sprite plane, in world units: the slant of the light on normal-mapped sprites.
	float height = 1.f;
	/// Half angle of the full-intensity cone of a spot light, in radians.
	float innerAngle = 25.f * std::numbers::pi_v<float> / 180.f;
	/// Half angle where a spot light reaches zero, in radians.
	float outerAngle = 40.f * std::numbers::pi_v<float> / 180.f;
	/// Whether the sprites flagged as shadow casters block a point or spot light.
	bool castShadows = false;
	/// Width of the shadow penumbra, in bins of the light's shadow map (0 hard, 1 to 3 soft).
	float shadowSoftness = 1.f;

	/**
	 * @brief
	 *  Name of the component.
	 * @return The name.
	 */
	static auto name() -> const char* { return "Light 2D"; }

	/**
	 * @brief
	 *  Key of the component in the scene files.
	 * @return The key.
	 */
	static auto key() -> const char* { return "Light2D"; }

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
