/**
 * @file SceneCamera.cpp
 * @author Silmaen
 * @date 23/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/SceneCamera.h"

#include "renderer/ClipSpace.h"

#include <cmath>

namespace owl::scene {

SceneCamera::SceneCamera() { recalculateProjection(); }

void SceneCamera::setOrthographic(const float iSize, const float iNearClip, const float iFarClip) {
	m_projectionType = ProjectionType::Orthographic;
	m_orthographicSize = iSize;
	m_orthographicNear = iNearClip;
	m_orthographicFar = iFarClip;
	recalculateProjection();
}

void SceneCamera::setPerspective(const float iVerticalFov, const float iNearClip, const float iFarClip) {
	m_projectionType = ProjectionType::Perspective;
	m_perspectiveFov = iVerticalFov;
	m_perspectiveNear = iNearClip;
	m_perspectiveFar = iFarClip;
	recalculateProjection();
}

void SceneCamera::setViewportSize(const math::vec2ui& iSize) {
	OWL_CORE_ASSERT(iSize.surface() > 0, "Null viewport size")
	m_aspectRatio = iSize.ratio();
	recalculateProjection();
}

auto SceneCamera::getHeightScale() const -> float {
	if (m_maxAspectRatio <= 0.f || m_aspectRatio <= m_maxAspectRatio)
		return 1.f;
	return m_maxAspectRatio / m_aspectRatio;
}

auto SceneCamera::getEffectiveVerticalFov() const -> float {
	return 2.f * std::atan(std::tan(m_perspectiveFov * 0.5f) * getHeightScale());
}

auto SceneCamera::getEffectiveHorizontalFov() const -> float {
	return 2.f * std::atan(std::tan(getEffectiveVerticalFov() * 0.5f) * m_aspectRatio);
}

auto SceneCamera::getEffectiveOrthographicSize() const -> float { return m_orthographicSize * getHeightScale(); }

void SceneCamera::recalculateProjection() {
	if (m_projectionType == ProjectionType::Perspective) {
		m_projection = math::perspectiveReversedZ(getEffectiveVerticalFov(), m_aspectRatio, m_perspectiveNear,
												  m_perspectiveFar);
	} else {
		const float size = getEffectiveOrthographicSize();
		const float orthoLeft = -size * m_aspectRatio * 0.5f;
		const float orthoRight = size * m_aspectRatio * 0.5f;
		const float orthoBottom = -size * 0.5f;
		const float orthoTop = size * 0.5f;
		m_projection = math::orthoReversedZ(orthoLeft, orthoRight, orthoBottom, orthoTop, m_orthographicNear,
											m_orthographicFar);
	}
	m_projection = renderer::toActiveClipSpace(m_projection);
}

SceneCamera::~SceneCamera() = default;


}// namespace owl::scene
