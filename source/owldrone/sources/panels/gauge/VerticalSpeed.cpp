/**
 * @file VerticalSpeed.cpp
 * @author Silmaen
 * @date 23/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "VerticalSpeed.h"

namespace drone::panels::gauge {

constexpr float g_MaxVelocity = 10;
constexpr float g_MaxAngle = 157;

VerticalSpeed::VerticalSpeed() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeVSIBack");
	textureLib.load("textures/GaugeVSICursor");
	m_background = textureLib.get("textures/GaugeVSIBack");
	m_cursor = textureLib.get("textures/GaugeVSICursor");
}

VerticalSpeed::~VerticalSpeed() = default;

void VerticalSpeed::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_background});
}

void VerticalSpeed::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, velocityToAngle()}, getScale()};
	tran.translation().z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor});
}

auto VerticalSpeed::velocityToAngle() const -> float {
	return -std::clamp(m_verticalVelocity, -g_MaxVelocity, g_MaxVelocity) / g_MaxVelocity * g_MaxAngle;
}

}// namespace drone::panels::gauge
