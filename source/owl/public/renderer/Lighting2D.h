/**
 * @file Lighting2D.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

/// Most point and spot lights the 2D renderers take per scene.
constexpr uint32_t g_maxLights2D = 64;
/// Angular bins of the polar shadow map of a 2D light.
constexpr uint32_t g_shadow2DBins = 512;

/**
 * @brief
 *  A point or spot light of the 2D renderers, in world units.
 */
struct OWL_API Light2DData {
	/// World position of the light in the sprite plane.
	math::vec2 position{0.f, 0.f};
	/// Height above the sprite plane: the slant of the light on normal-mapped sprites.
	float height = 1.f;
	/// Distance where the light reaches zero.
	float radius = 5.f;
	/// Linear colour times the intensity.
	math::vec3 color{1.f, 1.f, 1.f};
	/// Exponent of `(1 - distance / radius)`.
	float falloff = 2.f;
	/// Unit direction of a spot light.
	math::vec2 direction{1.f, 0.f};
	/// Cosine of the inner half angle of a spot light.
	float cosInner = 1.f;
	/// Cosine of the outer half angle of a spot light; below -1 for a point light.
	float cosOuter = -2.f;
	/// Whether the occluders block the light.
	bool castShadows = false;
	/// Width of the penumbra, in shadow-map bins.
	float shadowSoftness = 1.f;
};

/**
 * @brief
 *  A segment blocking the 2D lights casting shadows (an edge of a shadow-casting sprite).
 */
struct OWL_API Occluder2D {
	/// First end, world position.
	math::vec2 start{0.f, 0.f};
	/// Second end, world position.
	math::vec2 end{0.f, 0.f};
};

/**
 * @brief
 *  The lighting of the 2D layers of a scene for one frame.
 */
struct OWL_API Lighting2D {
	/// Linear ambient light added to every lit pixel.
	math::vec3 ambient{1.f, 1.f, 1.f};
	/// Point and spot lights, at most `g_maxLights2D` are used.
	std::vector<Light2DData> lights;
	/// Segments the shadow-casting lights stop at.
	std::vector<Occluder2D> occluders;
	/**
	 * @brief
	 *  The lights and the sprites meet on the ground plane (x, z) of a Y-up world instead of the XY plane (billboard
	 *  layers): light positions are then world (x, z), the sprites are lit without normal map nor shadow.
	 */
	bool groundPlane = false;
	/**
	 * @brief
	 *  Factor on the sprites of a layer of a 3D scene (billboards, depth tested) that undoes the exposure compensation
	 *  of the camera: unlit sprites keep their colour, the 2D lights keep their strength; 1 elsewhere.
	 */
	float exposureScale = 1.f;
	/// True for a layer of a 3D scene: lit even without any `Light2D`, unlit sprites scaled by `exposureScale`.
	bool inScene3D = false;
};

/**
 * @brief
 *  Fill the polar shadow map of a light: for each angular bin, the distance from the light to the nearest occluder
 *  along the ray through the bin centre, or the radius when nothing blocks it.
 * @details
 *  Bin `k` covers the angles from `-pi + k * 2pi / bins` (the angle of `atan2(y, x)`).
 * @param[in] iLight Position of the light.
 * @param[in] iRadius Radius of the light: occluders beyond it are skipped.
 * @param[in] iOccluders The occluding segments.
 * @param[out] oRow The bins to fill.
 */
OWL_API void buildShadowRow2D(const math::vec2& iLight, float iRadius, std::span<const Occluder2D> iOccluders,
							  std::span<float> oRow);

/**
 * @brief
 *  Light reaching a point of the sprite plane from a light, as the shaders compute it, without shadows or normal map.
 * @param[in] iLight The light.
 * @param[in] iPoint The lit point.
 * @return The linear light: colour times the distance and cone attenuation.
 */
[[nodiscard]] OWL_API auto lightAt2D(const Light2DData& iLight, const math::vec2& iPoint) -> math::vec3;

}// namespace owl::renderer
