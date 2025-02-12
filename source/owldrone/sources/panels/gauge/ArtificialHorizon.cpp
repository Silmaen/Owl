/**
 * @file ArtificialHorizon.cpp
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "ArtificialHorizon.h"
#include <cmath>

namespace drone::panels::gauge {

constexpr float deg2rad = std::numbers::pi_v<float> / 180.f;

ArtificialHorizon::ArtificialHorizon() {

	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeHORBack");
	textureLib.load("textures/GaugeHORCursor");
	textureLib.load("textures/GaugeHOROlivePitch");
	textureLib.load("textures/GaugeHORRollRing");
	background = textureLib.get("textures/GaugeHORBack");
	cursor = textureLib.get("textures/GaugeHORCursor");
	olivePitch = textureLib.get("textures/GaugeHOROlivePitch");
	rollRing = textureLib.get("textures/GaugeHORRollRing");
}

ArtificialHorizon::~ArtificialHorizon() = default;

void ArtificialHorizon::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, -roll}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = background});
}

void ArtificialHorizon::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, -roll}, getScale()};
	tran.translation().z() = -0.08f;
	tran.translation().x() =
			tran.translation().x() - std::sin(roll * deg2rad) * std::clamp(pitch, -30.f, 30.f) / 30.f * 0.086f;
	tran.translation().y() =
			tran.translation().y() - std::cos(roll * deg2rad) * std::clamp(pitch, -30.f, 30.f) / 30.f * 0.086f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = olivePitch});
	tran.translation() = getPosition();
	tran.translation().z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = rollRing});
	tran.translation().z() = -0.06f;
	tran.rotation().z() = 0;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = cursor});
}

}// namespace drone::panels::gauge
