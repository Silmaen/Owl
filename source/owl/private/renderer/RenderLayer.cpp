/**
 * @file RenderLayer.cpp
 * @author Silmaen
 * @date 30/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RenderLayer.h"

namespace owl::renderer {

auto projectToViewport(const math::mat4& iViewProjection, const math::vec2ui& iViewport, const math::vec3& iWorld)
		-> ViewportPoint {
	const math::vec4 clip = iViewProjection * math::vec4{iWorld.x(), iWorld.y(), iWorld.z(), 1.f};
	constexpr float epsilon = 1e-5f;
	// Dividing by |w| keeps a point behind the camera on the side it lies on, where an edge indicator must point.
	const float w = std::max(std::abs(clip.w()), epsilon);
	const math::vec2 ndc{clip.x() / w, clip.y() / w};
	return {.position = {(ndc.x() + 1.f) * 0.5f * static_cast<float>(iViewport.x()),
						 (1.f - ndc.y()) * 0.5f * static_cast<float>(iViewport.y())},
			.ahead = clip.w() > epsilon};
}

RenderLayer::~RenderLayer() = default;

}// namespace owl::renderer
