/**
 * @file Altitude.cpp
 * @author Silmaen
 * @date 23/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Altitude.h"

namespace drone::panels::gauge {

Altitude::Altitude() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeAltBack");
	textureLib.load("textures/GaugeAltCursor");
	textureLib.load("textures/GaugeAltCursor10");
	textureLib.load("textures/GaugeAltCursor100");
	m_background = textureLib.get("textures/GaugeAltBack");
	m_cursor = textureLib.get("textures/GaugeAltCursor");
	m_cursor10 = textureLib.get("textures/GaugeAltCursor10");
	m_cursor100 = textureLib.get("textures/GaugeAltCursor100");
}

void Altitude::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_background});
}
void Altitude::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};

	tran.rotation().z() = altitudeToAngle(100);
	tran.translation().z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor100});
	tran.rotation().z() = altitudeToAngle(10);
	tran.translation().z() = -0.06f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor10});
	tran.rotation().z() = altitudeToAngle(1);
	tran.translation().z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor});
}

auto Altitude::altitudeToAngle(float iDivider) const -> float { return -std::max(m_altitude / iDivider, 0.f) / 10 * 360; }

Altitude::~Altitude() = default;

}// namespace drone::panels::gauge
