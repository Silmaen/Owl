/**
 * @file ArtificialHorizon.cpp
 * @author Silmaen
 * @date 24/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "ArtificialHorizon.h"
#include <cmath>
#include <numbers>

namespace drone::panels::gauge {

constexpr float g_Deg2Rad = std::numbers::pi_v<float> / 180.f;

ArtificialHorizon::ArtificialHorizon() {

	auto textureLib = owl::renderer::Renderer::getTextureLibrary();
	textureLib.load("textures/GaugeHORBack");
	textureLib.load("textures/GaugeHORCursor");
	textureLib.load("textures/GaugeHOROlivePitch");
	textureLib.load("textures/GaugeHORRollRing");
	m_background = textureLib.get("textures/GaugeHORBack");
	m_cursor = textureLib.get("textures/GaugeHORCursor");
	m_olivePitch = textureLib.get("textures/GaugeHOROlivePitch");
	m_rollRing = textureLib.get("textures/GaugeHORRollRing");
}

ArtificialHorizon::~ArtificialHorizon() = default;

void ArtificialHorizon::drawBack() {
	owl::math::Transform tran{getPosition(), {0, 0, -m_roll}, getScale()};
	tran.translation().z() = -0.1f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_background});
}

void ArtificialHorizon::drawCursors() {
	owl::math::Transform tran{getPosition(), {0, 0, -m_roll}, getScale()};
	tran.translation().z() = -0.08f;
	tran.translation().x() =
			tran.translation().x() - std::sin(m_roll * g_Deg2Rad) * std::clamp(m_pitch, -30.f, 30.f) / 30.f * 0.086f;
	tran.translation().y() =
			tran.translation().y() - std::cos(m_roll * g_Deg2Rad) * std::clamp(m_pitch, -30.f, 30.f) / 30.f * 0.086f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_olivePitch});
	tran.translation() = getPosition();
	tran.translation().z() = -0.07f;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_rollRing});
	tran.translation().z() = -0.06f;
	tran.rotation().z() = 0;
	owl::renderer::Renderer2D::drawQuad({.transform = tran, .texture = m_cursor});
}

}// namespace drone::panels::gauge
