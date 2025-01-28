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

constexpr float sHalfTurn{180.f};
constexpr float sFullTurn{360.f};
constexpr float sZoomScroll{0.25f};

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
		if (m_cameraRotation > sHalfTurn)
			m_cameraRotation -= sFullTurn;
		else if (m_cameraRotation <= -sHalfTurn)
			m_cameraRotation += sFullTurn;
		m_camera.setRotation(m_cameraRotation);
	}
	m_camera.setPosition(m_cameraPosition);
	m_cameraTranslationSpeed = m_zoomLevel;
}

void CameraOrthoController::onEvent(event::Event& ioEvent) {
	OWL_PROFILE_FUNCTION()

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::MouseScrolledEvent>(
			[this](auto&& PH1) -> bool { return onMouseScrolled(std::forward<decltype(PH1)>(PH1)); });
	dispatcher.dispatch<event::WindowResizeEvent>(
			[this](auto&& PH1) -> bool { return onWindowResized(std::forward<decltype(PH1)>(PH1)); });
}

auto CameraOrthoController::onMouseScrolled(const event::MouseScrolledEvent& iEvent) -> bool {
	OWL_PROFILE_FUNCTION()

	m_zoomLevel -= iEvent.getYOff() * sZoomScroll;
	m_zoomLevel = std::max(m_zoomLevel, sZoomScroll);
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
