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
	textureLib.addFromStandardPath("textures/GaugeAltBack");
	textureLib.addFromStandardPath("textures/GaugeAltCursor");
	textureLib.addFromStandardPath("textures/GaugeAltCursor10");
	textureLib.addFromStandardPath("textures/GaugeAltCursor100");
	background = textureLib.get("textures/GaugeAltBack");
	cursor = textureLib.get("textures/GaugeAltCursor");
	cursor10 = textureLib.get("textures/GaugeAltCursor10");
	cursor100 = textureLib.get("textures/GaugeAltCursor100");
}

void Altitude::drawBack() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};
	tran.position.z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}
void Altitude::drawCursors() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};

	tran.rotation = altitudeToAngle(100);
	tran.position.z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor100});
	tran.rotation = altitudeToAngle(10);
	tran.position.z() = -0.06f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor10});
	tran.rotation = altitudeToAngle(1);
	tran.position.z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

float Altitude::altitudeToAngle(float divider) const { return -std::max(altitude / divider, 0.f) / 10 * 360; }

Altitude::~Altitude() = default;

}// namespace drone::panels::gauge
