/**
 * @file MotorRate.cpp
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "MotorRate.h"

namespace drone::panels::gauge {

constexpr float maxRate = 6000;
constexpr float maxAngle = 67.5;

MotorRate::MotorRate() {
	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.addFromStandardPath("textures/GaugeMOTBack");
	textureLib.addFromStandardPath("textures/GaugeMOTCursor1");
	textureLib.addFromStandardPath("textures/GaugeMOTCursor2");
	textureLib.addFromStandardPath("textures/GaugeMOTCursor3");
	textureLib.addFromStandardPath("textures/GaugeMOTCursor4");
	background = textureLib.get("textures/GaugeMOTBack");
	cursor1 = textureLib.get("textures/GaugeMOTCursor1");
	cursor2 = textureLib.get("textures/GaugeMOTCursor2");
	cursor3 = textureLib.get("textures/GaugeMOTCursor3");
	cursor4 = textureLib.get("textures/GaugeMOTCursor4");
}

MotorRate::~MotorRate() = default;

void MotorRate::drawBack() {
	owl::renderer::utils::PRS tran{.position = getTransform().position, .rotation = 0, .size = getTransform().size};
	tran.position.z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}

void MotorRate::drawCursors() {
	owl::renderer::utils::PRS tran{.position = getTransform().position,
								   .rotation = -rateToAngle(0),
								   .size = getTransform().size};
	tran.position.z() = -0.08f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor1});
	tran.rotation = rateToAngle(1);
	tran.position.z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor2});
	tran.rotation = rateToAngle(2);
	tran.position.z() = -0.06f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor3});
	tran.rotation = -rateToAngle(3);
	tran.position.z() = -0.05f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor4});
}

float MotorRate::rateToAngle(uint8_t idx) const { return -std::clamp(rates[idx], 0.f, maxRate) / maxRate * maxAngle; }


}// namespace drone::panels::gauge
