/**
 * @file Camera3DController.cpp
 * @author Silmaen
 * @date 06/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "input/Input.h"
#include "renderer/Camera3DController.h"

namespace owl::renderer {

void Camera3DController::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	const float delta = iTimeStep.getSeconds();

	if (input::Input::isKeyPressed(input::key::Left))
		addYaw(m_lookSpeed * delta);
	if (input::Input::isKeyPressed(input::key::Right))
		addYaw(-m_lookSpeed * delta);
	if (input::Input::isKeyPressed(input::key::Up))
		addPitch(m_lookSpeed * delta);
	if (input::Input::isKeyPressed(input::key::Down))
		addPitch(-m_lookSpeed * delta);

	math::vec3 axes{0.0f, 0.0f, 0.0f};
	if (input::Input::isKeyPressed(input::key::W))
		axes.z() += 1.0f;
	if (input::Input::isKeyPressed(input::key::S))
		axes.z() -= 1.0f;
	if (input::Input::isKeyPressed(input::key::D))
		axes.x() += 1.0f;
	if (input::Input::isKeyPressed(input::key::A))
		axes.x() -= 1.0f;
	if (input::Input::isKeyPressed(input::key::Space) || input::Input::isKeyPressed(input::key::E))
		axes.y() += 1.0f;
	if (input::Input::isKeyPressed(input::key::LeftShift) || input::Input::isKeyPressed(input::key::Q))
		axes.y() -= 1.0f;

	moveLocal(axes, m_moveSpeed * delta);
}

void Camera3DController::moveLocal(const math::vec3& iAxes, const float iAmount) {
	if (iAxes.x() == 0.0f && iAxes.y() == 0.0f && iAxes.z() == 0.0f)
		return;
	const math::vec3 worldUp{0.0f, 1.0f, 0.0f};
	const math::vec3 direction =
			iAxes.z() * getForwardDirection() + iAxes.x() * getRightDirection() + iAxes.y() * worldUp;
	m_position += iAmount * direction;
}

void Camera3DController::addYaw(const float iDelta) { m_yaw += iDelta; }

void Camera3DController::addPitch(const float iDelta) { setPitch(m_pitch + iDelta); }

void Camera3DController::setPitch(const float iPitch) { m_pitch = std::clamp(iPitch, -m_pitchLimit, m_pitchLimit); }

void Camera3DController::setEulerRotation(const math::vec3& iEuler) {
	m_yaw = iEuler.y();
	setPitch(iEuler.x());
}

auto Camera3DController::getForwardDirection() const -> math::vec3 {
	const float cp = std::cos(m_pitch);
	return {-cp * std::sin(m_yaw), std::sin(m_pitch), -cp * std::cos(m_yaw)};
}

auto Camera3DController::getRightDirection() const -> math::vec3 { return {std::cos(m_yaw), 0.0f, -std::sin(m_yaw)}; }

auto Camera3DController::getUpDirection() const -> math::vec3 {
	const float sp = std::sin(m_pitch);
	return {std::sin(m_yaw) * sp, std::cos(m_pitch), std::cos(m_yaw) * sp};
}

auto Camera3DController::getTransform() const -> math::Transform { return {m_position, getEulerRotation()}; }

auto Camera3DController::getViewMatrix() const -> math::mat4 { return inverse(getTransform()()); }

}// namespace owl::renderer
