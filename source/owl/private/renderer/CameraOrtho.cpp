/**
 * @file CameraOrtho.cpp
 * @author Silmaen
 * @date 10/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/CameraOrtho.h"
#include "renderer/RenderCommand.h"

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
	if (RenderCommand::getApi() == RenderAPI::Type::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		m_projection = biasMatrix * m_projection;
		m_projection(1, 1) *= -1.f;
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
