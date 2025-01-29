/**
 * @file CameraEditor.cpp
 * @author Silmaen
 * @date 30/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/CameraEditor.h"

#include "renderer/RenderAPI.h"
#include "input/Input.h"
#include "renderer/RenderCommand.h"

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
	if (input::Input::isKeyPressed(input::key::LeftAlt)) {
		const math::vec2 mouse{input::Input::getMouseX(), input::Input::getMouseY()};
		const math::vec2 delta = (mouse - m_initialMousePosition) * 0.003f;
		m_initialMousePosition = mouse;
		if (input::Input::isMouseButtonPressed(input::mouse::ButtonMiddle))
			mousePan(delta);
		else if (input::Input::isMouseButtonPressed(input::mouse::ButtonLeft))
			mouseRotate(delta);
		else if (input::Input::isMouseButtonPressed(input::mouse::ButtonRight))
			mouseZoom(delta.y());
	}
	updateView();
}

void CameraEditor::onEvent(event::Event& ioEvent) {
	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseScrolledEvent>(
			[&]<typename T0>(T0&& ioTz1) { return onMouseScroll(std::forward<T0>(ioTz1)); });
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

auto CameraEditor::getOrientation() const -> math::quat { return {1.0, -m_pitch, -m_yaw, 0.0f}; }

void CameraEditor::updateProjection() {
	m_aspectRatio = m_viewportSize.ratio();
	m_projection = math::perspective(math::radians(m_fov), m_aspectRatio, m_nearClip, m_farClip);
	if (RenderCommand::getApi() == RenderAPI::Type::Vulkan) {
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
	return false;
}

void CameraEditor::mousePan(const math::vec2& iDelta) {
	auto [xSpeed, ySpeed] = panSpeed();
	m_focalPoint += -getRightDirection() * iDelta.x() * xSpeed * m_distance;
	m_focalPoint += getUpDirection() * iDelta.y() * ySpeed * m_distance;
}

void CameraEditor::mouseRotate(const math::vec2& iDelta) {
	const float yawSign = getUpDirection().y() < 0 ? -1.0f : 1.0f;
	m_yaw += yawSign * iDelta.x() * rotationSpeed();
	m_pitch += iDelta.y() * rotationSpeed();
}

void CameraEditor::mouseZoom(const float iDelta) {
	m_distance -= iDelta * zoomSpeed();
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
// NOLINTBEGIN(readability-convert-member-functions-to-static)
auto CameraEditor::rotationSpeed() const -> float { return 0.8f; }
// NOLINTEND(readability-convert-member-functions-to-static)

auto CameraEditor::zoomSpeed() const -> float {
	const float distance = std::max(m_distance * 0.2f, 0.0f);
	return std::min(distance * distance, 100.0f);// max speed = 100
}

}// namespace owl::renderer
