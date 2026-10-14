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

	m_projection = makeProjection(iLeft, iRight, iBottom, iTop, iNear, iFar, getActiveClipSpace());
	m_viewProjection = m_projection * m_view;
}

auto CameraOrtho::makeProjection(const float iLeft, const float iRight, const float iBottom, const float iTop,
								 const float iNear, const float iFar, const ClipSpace iClipSpace) -> math::mat4 {
	math::mat4 projection = math::ortho(iLeft, iRight, iBottom, iTop, iNear, iFar);
	if (iClipSpace == ClipSpace::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		projection = biasMatrix * projection;
		projection(1, 1) *= -1.f;
		projection(1, 3) *= -1.f;
	}
	return projection;
}

auto CameraOrtho::getActiveClipSpace() -> ClipSpace {
	return gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan ? ClipSpace::Vulkan : ClipSpace::OpenGl;
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
