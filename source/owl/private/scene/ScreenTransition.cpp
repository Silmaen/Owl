/**
 * @file ScreenTransition.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ScreenTransition.h"

#include "core/Application.h"
#include "data/fonts/FontLibrary.h"
#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace owl::scene {

ScreenTransition::Type ScreenTransition::s_type = Type::None;
float ScreenTransition::s_duration = 0.f;
float ScreenTransition::s_elapsed = 0.f;
math::vec4 ScreenTransition::s_color{0.f, 0.f, 0.f, 1.f};
ScreenTransition::Phase ScreenTransition::s_phase = ScreenTransition::Phase::Idle;
std::optional<ScreenTransition::SceneLoadRequest> ScreenTransition::s_request;
bool ScreenTransition::s_loadDispatched = false;
float ScreenTransition::s_loadingHeld = 0.f;

namespace {

auto isWipe(const ScreenTransition::Type iType) -> bool {
	switch (iType) {
		case ScreenTransition::Type::WipeLeft:
		case ScreenTransition::Type::WipeRight:
		case ScreenTransition::Type::WipeUp:
		case ScreenTransition::Type::WipeDown:
			return true;
		case ScreenTransition::Type::None:
		case ScreenTransition::Type::FadeIn:
		case ScreenTransition::Type::FadeOut:
			return false;
	}
	return false;
}

void renderOverlay(const ScreenTransition::Type iType, const float iProgress, const math::vec4& iColor, const float iVw,
				   const float iVh) {
	if (isWipe(iType)) {
		math::Transform bar;
		bar.scale() = {iVw, iVh, 1.f};
		float cx = iVw * 0.5f;
		float cy = iVh * 0.5f;
		switch (iType) {
			case ScreenTransition::Type::WipeLeft:
				cx -= iProgress * iVw;
				break;
			case ScreenTransition::Type::WipeRight:
				cx += iProgress * iVw;
				break;
			case ScreenTransition::Type::WipeUp:
				cy += iProgress * iVh;
				break;
			case ScreenTransition::Type::WipeDown:
				cy -= iProgress * iVh;
				break;
			case ScreenTransition::Type::None:
			case ScreenTransition::Type::FadeIn:
			case ScreenTransition::Type::FadeOut:
				break;// `isWipe` already filtered these out.
		}
		bar.translation() = {cx, cy, 0.9f};
		renderer::Renderer2D::drawQuad({.transform = bar, .color = iColor});
		return;
	}
	float alpha = 1.f;
	switch (iType) {
		case ScreenTransition::Type::FadeIn:
			alpha = 1.f - iProgress;
			break;
		case ScreenTransition::Type::FadeOut:
			alpha = iProgress;
			break;
		case ScreenTransition::Type::None:
		case ScreenTransition::Type::WipeLeft:
		case ScreenTransition::Type::WipeRight:
		case ScreenTransition::Type::WipeUp:
		case ScreenTransition::Type::WipeDown:
			break;
	}
	math::Transform fullscreen;
	fullscreen.translation() = {iVw * 0.5f, iVh * 0.5f, 0.9f};
	fullscreen.scale() = {iVw, iVh, 1.f};
	const math::vec4 tint{iColor.r(), iColor.g(), iColor.b(), iColor.a() * alpha};
	renderer::Renderer2D::drawQuad({.transform = fullscreen, .color = tint});
}

auto contrastColor(const math::vec4& iCover) -> math::vec4 {
	const float luma = 0.299f * iCover.r() + 0.587f * iCover.g() + 0.114f * iCover.b();
	return luma < 0.5f ? math::vec4{0.95f, 0.95f, 0.95f, 1.f} : math::vec4{0.05f, 0.05f, 0.05f, 1.f};
}

void renderLoadingScreen(const math::vec4& iColor, const float iElapsed, const float iVw, const float iVh) {
	math::Transform fullscreen;
	fullscreen.translation() = {iVw * 0.5f, iVh * 0.5f, 0.9f};
	fullscreen.scale() = {iVw, iVh, 1.f};
	renderer::Renderer2D::drawQuad({.transform = fullscreen, .color = iColor});

	const auto fg = contrastColor(iColor);

	constexpr int kDotCount = 8;
	const float radius = std::min(iVw, iVh) * 0.05f;
	const float dotSize = radius * 0.35f;
	const float cx = iVw * 0.5f;
	const float cy = iVh * 0.5f + radius * 1.5f;
	const float headAngle = iElapsed * std::numbers::pi_v<float> * 2.f;
	for (int i = 0; i < kDotCount; ++i) {
		const float fraction = static_cast<float>(i) / static_cast<float>(kDotCount);
		const float angle = -fraction * 2.f * std::numbers::pi_v<float>;
		const float dotX = cx + radius * std::cos(angle);
		const float dotY = cy + radius * std::sin(angle);
		const float headOffset = std::fmod(static_cast<float>(i) - headAngle / (2.f * std::numbers::pi_v<float>),
										   static_cast<float>(kDotCount));
		const float wrapped = headOffset < 0.f ? headOffset + static_cast<float>(kDotCount) : headOffset;
		const float trailRatio = wrapped / static_cast<float>(kDotCount);
		const float alpha = 0.25f + 0.75f * (1.f - trailRatio);
		math::Transform dotTr;
		dotTr.translation() = {dotX, dotY, 0.91f};
		dotTr.scale() = {dotSize, dotSize, 1.f};
		const math::vec4 tint{fg.r(), fg.g(), fg.b(), fg.a() * alpha};
		renderer::Renderer2D::drawQuad({.transform = dotTr, .color = tint});
	}

	if (!core::Application::instanced())
		return;
	const auto font = core::Application::get().getFontLibrary().getDefaultFont();
	if (!font)
		return;
	math::Transform textTr;
	const float textHeight = std::min(iVw, iVh) * 0.04f;
	textTr.translation() = {cx, cy - radius * 2.5f - textHeight * 0.5f, 0.91f};
	textTr.scale() = {textHeight, textHeight, 1.f};
	renderer::Renderer2D::drawString(
			{.transform = textTr, .text = "Loading", .font = font, .color = fg, .entityId = -1});
}

}// namespace

void ScreenTransition::play(const Type iType, const float iDuration, const math::vec4& iColor) {
	s_type = iType;
	s_duration = std::max(iDuration, 0.001f);
	s_elapsed = 0.f;
	s_color = iColor;
	if (s_phase == Phase::Idle)
		s_phase = (iType == Type::None) ? Phase::Idle : Phase::OutAnim;
}

void ScreenTransition::start(const Type iType, const float iDuration) {
	play(iType, iDuration, math::vec4{0.f, 0.f, 0.f, 1.f});
}

void ScreenTransition::requestSceneLoad(const SceneLoadRequest& iRequest) {
	s_request = iRequest;
	s_loadDispatched = false;
	s_loadingHeld = 0.f;
	s_phase = Phase::OutAnim;
	s_type = iRequest.outType;
	s_duration = std::max(iRequest.outDuration, 0.001f);
	s_elapsed = 0.f;
	s_color = iRequest.color;
}

void ScreenTransition::update(const float iDeltaTime) {
	switch (s_phase) {
		case Phase::Idle:
			return;
		case Phase::OutAnim:
			{
				if (s_type == Type::None) {
					// Direct `play(None, …)` was used — nothing to animate, drop straight to Idle.
					s_phase = Phase::Idle;
					return;
				}
				s_elapsed += iDeltaTime;
				if (s_elapsed >= s_duration) {
					if (s_request.has_value()) {
						s_phase = Phase::Loading;
						s_loadingHeld = 0.f;
					} else {
						// Primitive overlay: no orchestrator follow-up.
						s_type = Type::None;
						s_phase = Phase::Idle;
					}
				}
				return;
			}
		case Phase::Loading:
			{
				s_loadingHeld += iDeltaTime;
				if (!s_request.has_value())
					return;// awaiting host call — keep showing the loading screen.
				if (s_loadDispatched && s_loadingHeld >= s_request->minHoldDuration) {
					s_phase = Phase::InAnim;
					s_type = s_request->inType;
					s_duration = std::max(s_request->inDuration, 0.001f);
					s_elapsed = 0.f;
				}
				return;
			}
		case Phase::InAnim:
			{
				s_elapsed += iDeltaTime;
				if (s_elapsed >= s_duration) {
					s_phase = Phase::Idle;
					s_type = Type::None;
					s_request.reset();
					s_loadDispatched = false;
					s_loadingHeld = 0.f;
				}
				return;
			}
	}
}

void ScreenTransition::render(const float iViewportWidth, const float iViewportHeight) {
	if (s_phase == Phase::Idle)
		return;

	const renderer::CameraOrtho cam(0.f, iViewportWidth, 0.f, iViewportHeight);
	renderer::Renderer2D::beginScene(cam);

	switch (s_phase) {
		case Phase::Idle:
			break;
		case Phase::OutAnim:
		case Phase::InAnim:
			{
				const float progress = std::clamp(s_elapsed / s_duration, 0.f, 1.f);
				renderOverlay(s_type, progress, s_color, iViewportWidth, iViewportHeight);
				break;
			}
		case Phase::Loading:
			renderLoadingScreen(s_color, s_loadingHeld, iViewportWidth, iViewportHeight);
			break;
	}

	renderer::Renderer2D::endScene();
}

auto ScreenTransition::isActive() -> bool { return s_phase != Phase::Idle; }

auto ScreenTransition::getProgress() -> float {
	switch (s_phase) {
		case Phase::Idle:
			return 1.f;
		case Phase::OutAnim:
		case Phase::InAnim:
			return s_duration <= 0.f ? 1.f : std::clamp(s_elapsed / s_duration, 0.f, 1.f);
		case Phase::Loading:
			if (s_request.has_value() && s_request->minHoldDuration > 0.f)
				return std::clamp(s_loadingHeld / s_request->minHoldDuration, 0.f, 1.f);
			return 1.f;
	}
	return 1.f;
}

auto ScreenTransition::getType() -> Type { return s_type; }

auto ScreenTransition::getColor() -> const math::vec4& { return s_color; }

auto ScreenTransition::getPhase() -> Phase { return s_phase; }

auto ScreenTransition::pendingLoadPath() -> std::optional<std::string> {
	if (s_phase != Phase::Loading || !s_request.has_value() || s_loadDispatched)
		return std::nullopt;
	s_loadDispatched = true;
	return s_request->scenePath;
}

void ScreenTransition::reset() {
	s_type = Type::None;
	s_duration = 0.f;
	s_elapsed = 0.f;
	s_color = math::vec4{0.f, 0.f, 0.f, 1.f};
	s_phase = Phase::Idle;
	s_request.reset();
	s_loadDispatched = false;
	s_loadingHeld = 0.f;
}

}// namespace owl::scene
