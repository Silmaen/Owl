/**
 * @file ShadowCascades.h
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <array>
#include <cstdint>

namespace owl::renderer {

/// Most cascades of a directional shadow.
constexpr uint32_t g_maxShadowCascades = 4;

/**
 * @brief
 *  How a directional light splits the view of a camera into shadow cascades.
 */
struct ShadowCascadeSettings {
	/// Direction the light travels, world space (normalized by the computation).
	math::vec3 lightDirection{0.f, -1.f, 0.f};
	/// Distance from the camera the cascades cover, clamped to the camera far plane.
	float shadowDistance = 60.f;
	/// Number of cascades, from 1 to `g_maxShadowCascades`.
	uint32_t cascadeCount = g_maxShadowCascades;
	/// Size of each cascade's shadow map, in texels.
	uint32_t resolution = 1024;
	/// Blend between uniform (0) and logarithmic (1) split distances.
	float splitBlend = 0.75f;
};

/**
 * @brief
 *  One cascade: the light camera rendering its shadow map and the view depth it covers.
 */
struct ShadowCascade {
	/// Orthographic light camera, bounding the cascade's slice of the view frustum.
	Camera camera;
	/// View-space depth (distance along the view axis) where the cascade ends.
	float splitDepth = 0.f;
	/// World size of one shadow-map texel (normal-offset bias unit).
	float texelWorldSize = 0.f;
};

/**
 * @brief
 *  The cascades of a camera view.
 */
struct ShadowCascades {
	/// The cascades, nearest first; the first `count` are valid.
	std::array<ShadowCascade, g_maxShadowCascades> cascades{};
	/// Number of valid cascades.
	uint32_t count = 0;
};

/**
 * @brief
 *  Split the view of a camera into shadow cascades for a directional light. Each cascade bounds a sphere around its
 *  slice of the frustum, its centre snapped to the shadow-map texel grid, so the shadows do not shimmer while the
 *  camera turns or moves; the light camera is pulled back so casters between the light and the slice still render.
 * @param[in] iCamera The camera of the view (perspective or orthographic).
 * @param[in] iSettings The light and cascade settings.
 * @return The cascades, none when the direction is null.
 */
[[nodiscard]] OWL_API auto computeShadowCascades(const Camera& iCamera, const ShadowCascadeSettings& iSettings)
		-> ShadowCascades;

}// namespace owl::renderer
