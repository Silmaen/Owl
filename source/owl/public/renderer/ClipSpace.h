/**
 * @file ClipSpace.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/matrices.h"

#include <cstdint>

namespace owl::renderer {

/**
 * @brief
 *  Clip-space convention of a projection matrix.
 * @details
 *  Both backends use reversed-Z depth in [0, 1] (1 on the near plane, compare `greater`, cleared to 0, a float depth
 *  buffer); they differ only in the direction of the NDC Y axis.
 */
enum struct ClipSpace : uint8_t {
	/// OpenGL (`glClipControl` lower left, zero to one): NDC Y up.
	OpenGl,
	/// Vulkan: NDC Y down (projection Y flipped).
	Vulkan,
};

/// Depth value of the near plane in the engine clip space.
constexpr float g_nearDepth = 1.f;
/// Depth value of the far plane (and of a cleared depth buffer) in the engine clip space.
constexpr float g_farDepth = 0.f;

/**
 * @brief
 *  Clip-space convention of the active render backend.
 * @return `Vulkan` on the Vulkan backend, `OpenGl` otherwise.
 */
[[nodiscard]] OWL_API auto getActiveClipSpace() -> ClipSpace;

/**
 * @brief
 *  Adapt a reversed-Z projection (`math::perspectiveReversedZ`, `math::orthoReversedZ`, NDC Y up) to a clip space.
 * @param[in] iProjection The projection, NDC Y up.
 * @param[in] iClipSpace The convention of the target backend.
 * @return The projection, Y row negated for Vulkan.
 */
[[nodiscard]] OWL_API auto toClipSpace(const math::mat4& iProjection, ClipSpace iClipSpace) -> math::mat4;

/**
 * @brief
 *  Adapt a reversed-Z projection to the clip space of the active backend.
 * @param[in] iProjection The projection, NDC Y up.
 * @return The projection for the active backend.
 */
[[nodiscard]] OWL_API auto toActiveClipSpace(const math::mat4& iProjection) -> math::mat4;

/**
 * @brief
 *  The view matrix with its translation removed: the view of the same camera placed at the origin.
 * @details
 *  For an affine view `V`, `V * translate(eye)` keeps the linear part of `V` and maps the eye to the origin, so its
 *  translation is exactly zero. Geometry given relative to the eye keeps its float precision at any distance.
 * @param[in] iView The view matrix.
 * @return The camera-relative view matrix.
 */
[[nodiscard]] OWL_API auto cameraRelativeView(const math::mat4& iView) -> math::mat4;

}// namespace owl::renderer
