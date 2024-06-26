/**
 * @file AirSpeed.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "AirSpeed.h"

namespace drone::panels::gauge {

constexpr float maxVelocity = 65;
constexpr float maxAngle = 275;

AirSpeed::AirSpeed() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.addFromStandardPath("textures/GaugeSPDBack");
	textureLib.addFromStandardPath("textures/GaugeSPDCursor");
	background = textureLib.get("textures/GaugeSPDBack");
	cursor = textureLib.get("textures/GaugeSPDCursor");
}

void AirSpeed::drawBack() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};
	tran.position.z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}
void AirSpeed::drawCursors() {
	owl::renderer::utils::PRS tran{.position = getTransform().position,
								   .rotation = velocityToAngle(),
								   .size = getTransform().size};
	tran.position.z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

float AirSpeed::velocityToAngle() const { return -std::clamp(velocity, 0.f, maxVelocity) / maxVelocity * maxAngle; }

AirSpeed::~AirSpeed() = default;

}// namespace drone::panels::gauge
