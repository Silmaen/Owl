/**
 * @file Compas.cpp
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Compas.h"

namespace drone::panels::gauge {

Compas::Compas() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeHDGBack");
	textureLib.load("textures/GaugeHDGCursor");
	background = textureLib.get("textures/GaugeHDGBack");
	cursor = textureLib.get("textures/GaugeHDGCursor");
}

Compas::~Compas() = default;

void Compas::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, headingToAngle()}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}

void Compas::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, 0}, getScale()};
	tran.translation().z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

auto Compas::headingToAngle() -> float {
	while (m_heading > 360) m_heading -= 360;
	while (m_heading < 0) m_heading += 360;
	return m_heading;
}

}// namespace drone::panels::gauge
