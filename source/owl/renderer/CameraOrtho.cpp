/**
 * @file CameraOrtho.cpp
 * @author Silmaen
 * @date 10/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "CameraOrtho.h"
#include "RenderCommand.h"

namespace owl::renderer {

CameraOrtho::CameraOrtho(const float iLeft, const float iRight, const float iBottom, const float iTop) {
	OWL_PROFILE_FUNCTION()
	m_viewMatrix = math::identity<float, 4>();
	setProjection(iLeft, iRight, iBottom, iTop, -1.0f, 1.0f);
	m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

void CameraOrtho::setProjection(const float iLeft, const float iRight, const float iBottom, const float iTop,
								const float iNear, const float iFar) {
	OWL_PROFILE_FUNCTION()

	m_projectionMatrix = math::ortho(iLeft, iRight, iBottom, iTop, iNear, iFar);
	if (RenderCommand::getApi() == RenderAPI::Type::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		m_projectionMatrix = biasMatrix * m_projectionMatrix;
		m_projectionMatrix(1, 1) *= -1.f;
	}
	m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

void CameraOrtho::recalculateViewMatrix() {
	OWL_PROFILE_FUNCTION()
	const math::mat4 transform =
			math::translate(math::identity<float, 4>(), m_position) *
			math::rotate(math::identity<float, 4>(), math::radians(-m_rotation), math::vec3{0, 0, 1});
	m_viewMatrix = math::inverse(transform);
	m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

}// namespace owl::renderer
