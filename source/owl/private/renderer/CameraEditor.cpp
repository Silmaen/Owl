/**
 * @file CameraEditor.cpp
 * @author Silmaen
 * @date 30/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/CameraEditor.h"

#include "input/Input.h"
#include "renderer/gpu/RenderAPI.h"
#include "renderer/gpu/RenderCommand.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace owl::renderer {

Camera::~Camera() = default;

void Camera::updateViewProjection() { m_viewProjection = m_projection * m_view; }

CameraEditor::CameraEditor(const float iFov, const float iAspectRatio, const float iNearClip, const float iFarClip)
	: Camera(math::perspective(math::radians(iFov), iAspectRatio, iNearClip, iFarClip)), m_fov(iFov),
	  m_aspectRatio(iAspectRatio), m_nearClip(iNearClip), m_farClip(iFarClip) {
	updateView();
}

CameraEditor::~CameraEditor() = default;

void CameraEditor::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) {
	const math::vec2 mouse{input::Input::getMouseX(), input::Input::getMouseY()};
	const math::vec2 pixels = mouse - m_initialMousePosition;
	const math::vec2 delta = pixels * 0.003f;
	// Track the cursor every frame so re-engaging a drag never produces a jump from a stale anchor.
	m_initialMousePosition = mouse;
	const bool alt = input::Input::isKeyPressed(input::key::LeftAlt);
	const bool ctrl = input::Input::isKeyPressed(input::key::LeftControl);
	const bool lmb = input::Input::isMouseButtonPressed(input::mouse::ButtonLeft);
	const bool rmb = input::Input::isMouseButtonPressed(input::mouse::ButtonRight);
	const bool mmb = input::Input::isMouseButtonPressed(input::mouse::ButtonMiddle);
	if (!(alt || ctrl) || !(lmb || rmb || mmb)) {
		m_dragTravel = 0.f;
		updateView();
		return;
	}
	// The drag moves nothing until the cursor has left the dead zone around where it started.
	const bool inDeadZone = m_dragTravel < m_navigation.deadZone;
	m_dragTravel += pixels.norm();
	if (inDeadZone) {
		updateView();
		return;
	}
	if (alt) {
		// Alt: camera-local navigation (the camera moves, its reference point follows).
		if (lmb)
			rotateInPlace(delta);
		else if (rmb)
			mousePan(delta);
		else if (mmb)
			mouseDolly(delta.y());
	} else if (ctrl) {
		// Ctrl: orbit navigation around the reference point.
		if (lmb)
			mouseRotate(delta);
		else if (rmb || mmb)
			mousePan(delta);
	}
	updateView();
}

void CameraEditor::onEvent(event::Event& ioEvent) {
	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseScrolledEvent>(
			[&]<typename T0>(T0&& ioTz1) -> bool { return onMouseScroll(std::forward<T0>(ioTz1)); });
}

auto CameraEditor::getUpDirection() const -> math::vec3 {
	return rotate(getOrientation(), math::vec3({0.0f, 1.0f, 0.0f}));
}

auto CameraEditor::getRightDirection() const -> math::vec3 {
	return rotate(getOrientation(), math::vec3({1.0f, 0.0f, 0.0f}));
}

auto CameraEditor::getForwardDirection() const -> math::vec3 {
	return rotate(getOrientation(), math::vec3({0.0f, 0.0f, -1.0f}));
}

auto CameraEditor::getOrientation() const -> math::quat {
	// Proper Euler->quat (yaw about world Y, pitch about local X): a real unit rotation, so a full 360deg turn works and the view never shears (the old `{1,-pitch,-yaw,0}` could not pass ~180deg, w fixed at 1).
	return math::quat{math::vec3{-m_pitch, -m_yaw, 0.0f}};
}

void CameraEditor::updateProjection() {
	m_aspectRatio = m_viewportSize.ratio();
	if (m_projectionMode == Projection::Orthographic) {
		// Same framing as the perspective view at the reference point.
		const float halfHeight = m_distance * std::tan(math::radians(m_fov) * 0.5f);
		const float halfWidth = halfHeight * m_aspectRatio;
		m_projection = math::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_nearClip, m_farClip);
	} else {
		m_projection = math::perspective(math::radians(m_fov), m_aspectRatio, m_nearClip, m_farClip);
	}
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		m_projection = biasMatrix * m_projection;
		m_projection(1, 1) *= -1.f;
	}
	updateViewProjection();
}

void CameraEditor::updateView() {
	// yaw = pitch = 0.0f; // Lock the camera's rotation
	m_position = calculatePosition();

	const math::quat orientation = getOrientation();
	m_view = translate(math::identity<float, 4>(), m_position) * toMat4(orientation);
	m_view = inverse(m_view);
	updateViewProjection();
}

auto CameraEditor::onMouseScroll(const event::MouseScrolledEvent& iEvent) -> bool {
	const float delta = iEvent.getYOff() * 0.1f;
	mouseZoom(delta);
	updateView();
	if (m_projectionMode == Projection::Orthographic)
		updateProjection();
	return false;
}

void CameraEditor::setProjection(const Projection iProjection) {
	m_projectionMode = iProjection;
	if (iProjection == Projection::Orthographic) {
		m_pitch = 0.f;
		m_yaw = 0.f;
	}
	updateView();
	updateProjection();
}

void CameraEditor::frame(const math::vec3& iCenter, const float iRadius) {
	m_focalPoint = iCenter;
	// Back off until the sphere fits the smaller side of the view, with a margin.
	const float halfFov = math::radians(m_fov) * 0.5f;
	const float fit = std::min(1.f, m_aspectRatio);
	m_distance = std::max(1.f, std::max(iRadius, 0.01f) * 1.25f / (std::tan(halfFov) * fit));
	updateView();
	updateProjection();
}

void CameraEditor::viewAlong(const ViewAxis iAxis) {
	constexpr float quarter = std::numbers::pi_v<float> * 0.5f;
	switch (iAxis) {
		case ViewAxis::Front:
			m_pitch = 0.f;
			m_yaw = 0.f;
			break;
		case ViewAxis::Back:
			m_pitch = 0.f;
			m_yaw = 2.f * quarter;
			break;
		case ViewAxis::Top:
			m_pitch = quarter;
			m_yaw = 0.f;
			break;
		case ViewAxis::Bottom:
			m_pitch = -quarter;
			m_yaw = 0.f;
			break;
		case ViewAxis::Right:
			m_pitch = 0.f;
			m_yaw = -quarter;
			break;
		case ViewAxis::Left:
			m_pitch = 0.f;
			m_yaw = quarter;
			break;
	}
	updateView();
}

void CameraEditor::resetView() {
	m_focalPoint = {0.f, 0.f, 0.f};
	m_distance = 10.f;
	m_pitch = 0.f;
	m_yaw = 0.f;
	updateView();
	updateProjection();
}

void CameraEditor::lookFrom(const math::vec3& iEye, const math::vec3& iForward) {
	if (const float length = iForward.norm(); length > 1e-6f) {
		// Inverse of `getForwardDirection()`: forward = (cos p sin y, -sin p, -cos p cos y).
		const math::vec3 forward = iForward / length;
		m_pitch = std::asin(std::clamp(-forward.y(), -1.f, 1.f));
		m_yaw = std::atan2(forward.x(), -forward.z());
	}
	m_focalPoint = iEye + getForwardDirection() * m_distance;
	updateView();
}

void CameraEditor::mousePan(const math::vec2& iDelta) {
	auto [xSpeed, ySpeed] = panSpeed();
	xSpeed *= m_navigation.pan;
	ySpeed *= m_navigation.pan;
	m_focalPoint += -getRightDirection() * iDelta.x() * xSpeed * m_distance;
	m_focalPoint += getUpDirection() * iDelta.y() * ySpeed * m_distance;
}

void CameraEditor::mouseRotate(const math::vec2& iDelta) {
	if (m_projectionMode == Projection::Orthographic)
		return;
	const float yawSign = getUpDirection().y() < 0 ? -1.0f : 1.0f;
	m_yaw += yawSign * iDelta.x() * rotationSpeed();
	m_pitch += pitchSign() * iDelta.y() * rotationSpeed();
}

void CameraEditor::rotateInPlace(const math::vec2& iDelta) {
	if (m_projectionMode == Projection::Orthographic)
		return;
	// Keep the eye fixed: rotate, then move the focal point so `position = focal - forward * distance` is unchanged.
	const math::vec3 eye = m_position;
	const float yawSign = getUpDirection().y() < 0 ? -1.0f : 1.0f;
	m_yaw += yawSign * iDelta.x() * rotationSpeed();
	m_pitch += pitchSign() * iDelta.y() * rotationSpeed();
	m_focalPoint = eye + getForwardDirection() * m_distance;
}

void CameraEditor::mouseDolly(const float iDelta) {
	m_focalPoint += getForwardDirection() * iDelta * m_navigation.zoom * m_distance;
}

void CameraEditor::mouseZoom(const float iDelta) {
	m_distance -= iDelta * m_navigation.zoom * zoomSpeed();
	if (m_distance < 1.0f) {
		m_focalPoint += getForwardDirection();
		m_distance = 1.0f;
	}
}

auto CameraEditor::calculatePosition() const -> math::vec3 { return m_focalPoint - getForwardDirection() * m_distance; }

auto CameraEditor::panSpeed() const -> std::pair<float, float> {
	const float x = std::min(static_cast<float>(m_viewportSize.x()) / 1000.0f, 2.4f);// max = 2.4f
	const float xFactor = (0.0366f * (x * x)) - (0.1778f * x) + 0.3021f;

	const float y = std::min(static_cast<float>(m_viewportSize.y()) / 1000.0f, 2.4f);// max = 2.4f
	const float yFactor = (0.0366f * (y * y)) - (0.1778f * y) + 0.3021f;

	return {xFactor, yFactor};
}

auto CameraEditor::rotationSpeed() const -> float { return 0.8f * m_navigation.rotate; }

auto CameraEditor::pitchSign() const -> float { return m_navigation.invertY ? -1.f : 1.f; }

auto CameraEditor::zoomSpeed() const -> float {
	const float distance = std::max(m_distance * 0.2f, 0.0f);
	return std::min(distance * distance, 100.0f);// max speed = 100
}

}// namespace owl::renderer
