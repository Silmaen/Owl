/**
 * @file AirSpeed.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "AirSpeed.h"

namespace drone::panels::gauge {

constexpr float g_MaxVelocity = 65;
constexpr float g_MaxAngle = 275;

AirSpeed::AirSpeed() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeSPDBack");
	textureLib.load("textures/GaugeSPDCursor");
	m_background = textureLib.get("textures/GaugeSPDBack");
	m_cursor = textureLib.get("textures/GaugeSPDCursor");
}

void AirSpeed::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_background});
}
void AirSpeed::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, velocityToAngle()}, getScale()};
	tran.translation().z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor});
}

auto AirSpeed::velocityToAngle() const -> float {
	return -std::clamp(m_velocity, 0.f, g_MaxVelocity) / g_MaxVelocity * g_MaxAngle;
}

AirSpeed::~AirSpeed() = default;

}// namespace drone::panels::gauge
