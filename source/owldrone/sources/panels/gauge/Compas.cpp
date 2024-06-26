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
	textureLib.addFromStandardPath("textures/GaugeHDGBack");
	textureLib.addFromStandardPath("textures/GaugeHDGCursor");
	background = textureLib.get("textures/GaugeHDGBack");
	cursor = textureLib.get("textures/GaugeHDGCursor");
}

Compas::~Compas() = default;

void Compas::drawBack() {
	owl::renderer::utils::PRS tran{.position = getTransform().position,
								   .rotation = headingToAngle(),
								   .size = getTransform().size};
	tran.position.z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}

void Compas::drawCursors() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};
	tran.position.z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

float Compas::headingToAngle() {
	while (heading > 360) heading -= 360;
	while (heading < 0) heading += 360;
	return heading;
}

}// namespace drone::panels::gauge
