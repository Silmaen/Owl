/**
 * @file VerticalSpeed.cpp
 * @author Silmaen
 * @date 23/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "VerticalSpeed.h"

namespace drone::panels::gauge {

constexpr float maxVelocity = 10;
constexpr float maxAngle = 157;

VerticalSpeed::VerticalSpeed() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.addFromStandardPath("textures/GaugeVSIBack");
	textureLib.addFromStandardPath("textures/GaugeVSICursor");
	background = textureLib.get("textures/GaugeVSIBack");
	cursor = textureLib.get("textures/GaugeVSICursor");
}

VerticalSpeed::~VerticalSpeed() = default;

void VerticalSpeed::drawBack() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};
	tran.position.z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}

void VerticalSpeed::drawCursors() {
	owl::renderer::utils::PRS tran{.position = getTransform().position,
								   .rotation = velocityToAngle(),
								   .size = getTransform().size};
	tran.position.z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

float VerticalSpeed::velocityToAngle() const {
	return -std::clamp(verticalVelocity, -maxVelocity, maxVelocity) / maxVelocity * maxAngle;
}

}// namespace drone::panels::gauge
