/**
 * @file CameraPilot.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CameraPilot.h"

#include <math/linAlgebra.h>
#include <math/matrixCreation.h>

#include <cmath>
#include <utility>

namespace owl::nest::panel {

namespace {
auto isSamePose(const CameraPilot::Pose& iA, const CameraPilot::Pose& iB) -> bool {
	constexpr float epsilon = 1e-6f;
	return (iA.position - iB.position).norm() < epsilon && std::abs(iA.pitch - iB.pitch) < epsilon &&
		   std::abs(iA.yaw - iB.yaw) < epsilon;
}
}// namespace

auto CameraPilot::getPose(const renderer::CameraEditor& iView) -> Pose {
	return {.position = iView.getPosition(), .pitch = iView.getPitch(), .yaw = iView.getYaw()};
}

void CameraPilot::start(const core::UUID iEntity, const renderer::CameraEditor& iSavedView,
						const renderer::CameraEditor& iView) {
	m_entity = iEntity;
	m_savedView = iSavedView;
	m_lastPose = getPose(iView);
	m_gesture = false;
}

auto CameraPilot::stop() -> std::optional<renderer::CameraEditor> {
	m_entity = core::UUID{0};
	m_gesture = false;
	return std::exchange(m_savedView, std::nullopt);
}

auto CameraPilot::shouldFollowEntity(const renderer::CameraEditor& iView) const -> bool {
	return isActive() && !m_gesture && isSamePose(getPose(iView), m_lastPose);
}

auto CameraPilot::track(const renderer::CameraEditor& iView, const bool iInputHeld) -> Step {
	if (!isActive())
		return Step::Idle;
	if (const Pose pose = getPose(iView); !isSamePose(pose, m_lastPose)) {
		m_lastPose = pose;
		if (m_gesture)
			return Step::Move;
		m_gesture = true;
		return Step::Begin;
	}
	if (m_gesture && !iInputHeld) {
		m_gesture = false;
		return Step::Commit;
	}
	return Step::Idle;
}

auto CameraPilot::getCameraWorld(const renderer::CameraEditor& iView, const math::Transform& iCurrent,
								 const bool iOrthographic) -> math::Transform {
	if (iOrthographic) {
		const auto& focal = iView.getFocalPoint();
		return {{focal.x(), focal.y(), iCurrent.translation().z()}, iCurrent.rotation(), iCurrent.scale()};
	}
	math::Transform world{translate(math::identity<float, 4>(), iView.getPosition()) * toMat4(iView.getOrientation())};
	world.scale() = iCurrent.scale();
	return world;
}

}// namespace owl::nest::panel
