/**
 * @file MotorRate.cpp
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "MotorRate.h"

namespace drone::panels::gauge {

constexpr float g_MaxRate = 6000;
constexpr float g_MaxAngle = 67.5;

MotorRate::MotorRate() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeMOTBack");
	textureLib.load("textures/GaugeMOTCursor1");
	textureLib.load("textures/GaugeMOTCursor2");
	textureLib.load("textures/GaugeMOTCursor3");
	textureLib.load("textures/GaugeMOTCursor4");
	m_background = textureLib.get("textures/GaugeMOTBack");
	m_cursor1 = textureLib.get("textures/GaugeMOTCursor1");
	m_cursor2 = textureLib.get("textures/GaugeMOTCursor2");
	m_cursor3 = textureLib.get("textures/GaugeMOTCursor3");
	m_cursor4 = textureLib.get("textures/GaugeMOTCursor4");
}

MotorRate::~MotorRate() = default;

void MotorRate::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_background});
}

void MotorRate::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, -rateToAngle(0)}, getScale()};
	tran.translation().z() = -0.08f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor1});
	tran.rotation().z() = rateToAngle(1);
	tran.translation().z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor2});
	tran.rotation().z() = rateToAngle(2);
	tran.translation().z() = -0.06f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor3});
	tran.rotation().z() = -rateToAngle(3);
	tran.translation().z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor4});
}

auto MotorRate::rateToAngle(const uint8_t iRate) const -> float {
	return -std::clamp(m_rates[iRate], 0.f, g_MaxRate) / g_MaxRate * g_MaxAngle;
}


}// namespace drone::panels::gauge
