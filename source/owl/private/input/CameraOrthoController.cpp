/**
 * @file CameraOrthoController.cpp
 * @author Silmaen
 * @date 17/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "input/CameraOrthoController.h"
#include "input/Input.h"

namespace owl::input {

namespace {

constexpr float g_HalfTurn{180.f};
constexpr float g_FullTurn{360.f};
constexpr float g_ZoomScroll{0.25f};

}// namespace

CameraOrthoController::CameraOrthoController(const float iAspectRatio, const bool iRotation)
	: m_aspectRatio{iAspectRatio},
	  m_camera(-m_aspectRatio * m_zoomLevel, m_aspectRatio * m_zoomLevel, -m_zoomLevel, m_zoomLevel),
	  m_rotation{iRotation} {}

void CameraOrthoController::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()
	const auto delta = iTimeStep.getSeconds();
	const auto angle = static_cast<double>(math::radians(m_cameraRotation));
	if (Input::isKeyPressed(key::A)) {
		m_cameraPosition.x() -= static_cast<float>(cos(angle)) * m_cameraTranslationSpeed * delta;
		m_cameraPosition.y() -= static_cast<float>(sin(angle)) * m_cameraTranslationSpeed * delta;
	} else if (Input::isKeyPressed(key::D)) {
		m_cameraPosition.x() += static_cast<float>(cos(angle)) * m_cameraTranslationSpeed * delta;
		m_cameraPosition.y() += static_cast<float>(sin(angle)) * m_cameraTranslationSpeed * delta;
	}
	if (Input::isKeyPressed(key::W)) {
		m_cameraPosition.x() += static_cast<float>(sin(angle)) * m_cameraTranslationSpeed * delta;
		m_cameraPosition.y() += static_cast<float>(cos(angle)) * m_cameraTranslationSpeed * delta;
	} else if (Input::isKeyPressed(key::S)) {
		m_cameraPosition.x() -= static_cast<float>(sin(angle)) * m_cameraTranslationSpeed * delta;
		m_cameraPosition.y() -= static_cast<float>(cos(angle)) * m_cameraTranslationSpeed * delta;
	}
	if (m_rotation) {
		if (Input::isKeyPressed(key::Q))
			m_cameraRotation += m_cameraRotationSpeed * delta;
		if (Input::isKeyPressed(key::E))
			m_cameraRotation -= m_cameraRotationSpeed * delta;
		if (m_cameraRotation > g_HalfTurn)
			m_cameraRotation -= g_FullTurn;
		else if (m_cameraRotation <= -g_HalfTurn)
			m_cameraRotation += g_FullTurn;
		m_camera.setRotation(m_cameraRotation);
	}
	m_camera.setPosition(m_cameraPosition);
	m_cameraTranslationSpeed = m_zoomLevel;
}

void CameraOrthoController::onEvent(event::Event& ioEvent) {
	OWL_PROFILE_FUNCTION()

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseScrolledEvent>(
			[this]<typename T0>(T0&& ioArgs) -> bool { return onMouseScrolled(std::forward<T0>(ioArgs)); });
	dispatcher.dispatch<event::WindowResizeEvent>(
			[this]<typename T0>(T0&& ioArgs) -> bool { return onWindowResized(std::forward<T0>(ioArgs)); });
}

auto CameraOrthoController::onMouseScrolled(const event::MouseScrolledEvent& iEvent) -> bool {
	OWL_PROFILE_FUNCTION()

	m_zoomLevel -= iEvent.getYOff() * g_ZoomScroll;
	m_zoomLevel = std::max(m_zoomLevel, g_ZoomScroll);
	m_camera.setProjection(-m_aspectRatio * m_zoomLevel, m_aspectRatio * m_zoomLevel, -m_zoomLevel, m_zoomLevel);
	return false;
}

auto CameraOrthoController::onWindowResized(const event::WindowResizeEvent& iEvent) -> bool {
	OWL_PROFILE_FUNCTION()

	onResize(iEvent.getSize());
	return false;
}

void CameraOrthoController::onResize(const math::vec2ui& iSize) {
	m_aspectRatio = iSize.ratio();
	m_camera.setProjection(-m_aspectRatio * m_zoomLevel, m_aspectRatio * m_zoomLevel, -m_zoomLevel, m_zoomLevel);
}

}// namespace owl::input
