/**
 * @file CameraEditor.cpp
 * @author Silmaen
 * @date 30/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "CameraEditor.h"

#include "RenderAPI.h"
#include "RenderCommand.h"
#include "input/Input.h"

namespace owl::renderer {

Camera::~Camera() = default;

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
			[&](auto&& TZ1) { return onMouseScroll(std::forward<decltype(TZ1)>(TZ1)); });
}

math::vec3 CameraEditor::getUpDirection() const {
	return math::rotate(getOrientation(), math::vec3({0.0f, 1.0f, 0.0f}));
}

math::vec3 CameraEditor::getRightDirection() const {
	return math::rotate(getOrientation(), math::vec3({1.0f, 0.0f, 0.0f}));
}

math::vec3 CameraEditor::getForwardDirection() const {
	return math::rotate(getOrientation(), math::vec3({0.0f, 0.0f, -1.0f}));
}

math::quat CameraEditor::getOrientation() const { return {1.0, -m_pitch, -m_yaw, 0.0f}; }

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
}

void CameraEditor::updateView() {
	// yaw = pitch = 0.0f; // Lock the camera's rotation
	m_position = calculatePosition();

	const math::quat orientation = getOrientation();
	m_viewMatrix = math::translate(math::identity<float, 4>(), m_position) * math::toMat4(orientation);
	m_viewMatrix = math::inverse(m_viewMatrix);
}

bool CameraEditor::onMouseScroll(const event::MouseScrolledEvent& iEvent) {
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

math::vec3 CameraEditor::calculatePosition() const { return m_focalPoint - getForwardDirection() * m_distance; }

std::pair<float, float> CameraEditor::panSpeed() const {
	const float x = std::min(static_cast<float>(m_viewportSize.getWidth()) / 1000.0f, 2.4f);// max = 2.4f
	const float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

	const float y = std::min(static_cast<float>(m_viewportSize.getHeight()) / 1000.0f, 2.4f);// max = 2.4f
	const float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

	return {xFactor, yFactor};
}
// NOLINTBEGIN(readability-convert-member-functions-to-static)
float CameraEditor::rotationSpeed() const { return 0.8f; }
// NOLINTEND(readability-convert-member-functions-to-static)

float CameraEditor::zoomSpeed() const {
	const float distance = std::max(m_distance * 0.2f, 0.0f);
	return std::min(distance * distance, 100.0f);// max speed = 100
}

}// namespace owl::renderer
