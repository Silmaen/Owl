/**
 * @file CameraOrtho.cpp
 * @author Silmaen
 * @date 10/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/CameraOrtho.h"
#include "renderer/gpu/RenderCommand.h"

namespace owl::renderer {

CameraOrtho::~CameraOrtho() = default;

CameraOrtho::CameraOrtho(const float iLeft, const float iRight, const float iBottom, const float iTop) {
	OWL_PROFILE_FUNCTION()

	m_view = math::identity<float, 4>();
	setProjection(iLeft, iRight, iBottom, iTop, -1.0f, 1.0f);
	m_viewProjection = m_projection * m_view;
}

void CameraOrtho::setProjection(const float iLeft, const float iRight, const float iBottom, const float iTop,
								const float iNear, const float iFar) {
	OWL_PROFILE_FUNCTION()

	m_projection = math::ortho(iLeft, iRight, iBottom, iTop, iNear, iFar);
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		m_projection = biasMatrix * m_projection;
		// Vulkan's NDC has Y pointing down. To keep "world Y up = screen up"
		// for our caller side, negate the whole Y row of the projection —
		// both the scale `(1,1)` and the offset `(1,3)`. The previous fix only
		// flipped `(1,1)`, which is enough for symmetric ortho (where
		// `(1,3) == 0`) but breaks for asymmetric pixel-space ortho such as
		// `(0, vw, 0, vh)` used by the raycaster, ScreenTransition, and the
		// screen-overlay `Renderer2DLayer`: with the offset left at `-1`,
		// `world y = 0` maps to `NDC y = -1` and the rest of the viewport
		// gets clipped (out of `[-1, 1]`), pinning every quad to the screen
		// edge.
		m_projection(1, 1) *= -1.f;
		m_projection(1, 3) *= -1.f;
	}
	m_viewProjection = m_projection * m_view;
}

void CameraOrtho::recalculateViewMatrix() {
	OWL_PROFILE_FUNCTION()

	const math::mat4 transform =
			math::translate(math::identity<float, 4>(), m_position) *
			math::rotate(math::identity<float, 4>(), math::radians(-m_rotation), math::vec3{0, 0, 1});
	m_view = math::inverse(transform);
	m_viewProjection = m_projection * m_view;
}

}// namespace owl::renderer
