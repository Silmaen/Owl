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

/// Illuminance of a clear noon sun, in lux: the default of a directional light.
constexpr float g_defaultSunIlluminance = 100000.f;

/// Luminous power of a 60 W incandescent bulb, in lumens: the default of a point or spot light.
constexpr float g_defaultBulbPower = 800.f;

/// Smallest size of an area light, in world units.
constexpr float g_minAreaSize = 0.001f;

/// Largest apparent radius of a directional light, in radians (10 degrees).
constexpr float g_maxAngularRadius = 10.f * std::numbers::pi_v<float> / 180.f;

/**
 * @brief
 *  A light of the 3D layers (meshes and voxel worlds). It shines along the -Z axis of its entity's world transform;
 *  a point or sphere light ignores the rotation, a directional light the position. A rectangle spans the X and Y
 *  axes of the entity, a disc lies in its XY plane; both emit from their -Z face only.
 */
struct OWL_API Light {
	/**
	 * @brief
	 *  Kind of light.
	 */
	enum struct Type : uint8_t {
		Directional,///< Parallel rays from infinitely far (the sun).
		Point,///< Rays from a point in every direction, fading to zero at `range`.
		Spot,///< A point light restricted to a cone, softened between `innerAngle` and `outerAngle`.
		Rectangle,///< A rectangle of `size` emitting from one face, like a panel or a window.
		Disc,///< A disc of `radius` emitting from one face, like a ceiling lamp.
		Sphere,///< A sphere of `radius` emitting in every direction, like a bulb.
	};

	/// Kind of light.
	Type type = Type::Directional;
	/// Whether the light shines; a light switched off keeps its settings.
	bool enabled = true;
	/// Colour, sRGB as authored.
	math::vec3 color{1.f, 1.f, 1.f};
	/// Strength in physical units: illuminance in lux for a directional light, luminous power in lumens otherwise.
	float intensity = g_defaultSunIlluminance;
	/// Distance where a point, spot or area light reaches zero, in world units.
	float range = 10.f;
	/// Width (along X) and height (along Y) of a rectangle light, in world units.
	math::vec2 size{1.f, 1.f};
	/// Radius of a disc or a sphere light, in world units.
	float radius = 0.1f;
	/// Half angle of the full-intensity cone of a spot light, in radians.
	float innerAngle = 20.f * std::numbers::pi_v<float> / 180.f;
	/// Half angle where a spot light reaches zero, in radians.
	float outerAngle = 30.f * std::numbers::pi_v<float> / 180.f;
	/**
	 * @brief
	 *  Whether the light casts shadows: the first directional light that does shadows the scene through cascades,
	 *  the other lights nearest to the camera within the budget of the backend (`getLocalShadowBudget`), an area
	 *  light through a cube like a point light, softened by the size of its surface.
	 */
	bool castShadows = true;
	/// Distance from the camera the shadows of a directional light cover, in world units.
	float shadowDistance = 60.f;
	/// Number of shadow cascades of a directional light, from 1 to 4.
	uint32_t cascadeCount = 4;
	/// Size of each cascade's shadow map, of a spot shadow map or of each face of a point shadow cube, in texels.
	uint32_t shadowResolution = 1024;
	/// Depth bias against self-shadowing, in shadow-map texels along the surface normal.
	float shadowBias = 1.5f;
	/**
	 * @brief
	 *  Apparent radius of a directional light, in radians (the sun: about 0.0047): soft shadows growing with the
	 *  distance to their caster (PCSS) and a wider highlight; 0 keeps hard shadows and a point-like highlight.
	 */
	float angularRadius = 0.f;

	/**
	 * @brief
	 *  Strength of the lens flare of the light, seen by a camera with lens flares: 0 never flares, 1 flares with the
	 *  pre-exposed illuminance the light gives at the camera.
	 */
	float flare = 0.f;

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
