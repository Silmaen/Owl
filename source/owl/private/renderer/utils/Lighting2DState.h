/**
 * @file Lighting2DState.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/gpu/StorageBuffer.h"

#include <cstdint>

namespace owl::renderer::utils {

/**
 * @brief
 *  The 2D lighting `Renderer2D::setLighting` uploaded, shared with the other 2D renderers (tilemaps).
 */
struct Lighting2DState {
	/// Lights (`Light2D` of `light2d.slangh`), null before `Renderer2D::init`.
	shared<gpu::StorageBuffer> lights;
	/// Shadow rows of the shadow-casting lights.
	shared<gpu::StorageBuffer> shadows;
	/// Number of lights in `lights`.
	int32_t lightCount = 0;
	/// rgb: linear ambient; w: 1 when the 2D lighting is on.
	math::vec4 ambient{1.f, 1.f, 1.f, 0.f};
};

/**
 * @brief
 *  The current 2D lighting of `Renderer2D`.
 * @return The buffers and the parameters, lighting off when `Renderer2D` is not initialised.
 */
[[nodiscard]] auto getLighting2DState() -> Lighting2DState;

}// namespace owl::renderer::utils
