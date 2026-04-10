/**
 * @file ScreenTransition.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ScreenTransition.h"

#include "renderer/CameraOrtho.h"
#include "renderer/Renderer2D.h"

#include <algorithm>

namespace owl::scene {

ScreenTransition::Type ScreenTransition::s_type = Type::None;
float ScreenTransition::s_duration = 0.f;
float ScreenTransition::s_elapsed = 0.f;

void ScreenTransition::start(const Type iType, const float iDuration) {
	s_type = iType;
	s_duration = std::max(iDuration, 0.001f);
	s_elapsed = 0.f;
}

void ScreenTransition::update(const float iDeltaTime) {
	if (s_type == Type::None)
		return;
	s_elapsed += iDeltaTime;
	if (s_elapsed >= s_duration)
		s_type = Type::None;
}

void ScreenTransition::render(const float iViewportWidth, const float iViewportHeight) {
	if (s_type == Type::None)
		return;

	const float progress = std::clamp(s_elapsed / s_duration, 0.f, 1.f);
	float alpha = 0.f;
	switch (s_type) {
		case Type::FadeIn:
			alpha = 1.f - progress;// opaque → transparent
			break;
		case Type::FadeOut:
			alpha = progress;// transparent → opaque
			break;
		case Type::None:
			return;
	}

	const renderer::CameraOrtho cam(0.f, iViewportWidth, 0.f, iViewportHeight);
	renderer::Renderer2D::beginScene(cam);

	math::Transform fullscreen;
	fullscreen.translation() = {iViewportWidth * 0.5f, iViewportHeight * 0.5f, 0.9f};
	fullscreen.scale() = {iViewportWidth, iViewportHeight, 1.f};
	renderer::Renderer2D::drawQuad({.transform = fullscreen, .color = {0.f, 0.f, 0.f, alpha}});

	renderer::Renderer2D::endScene();
}

auto ScreenTransition::isActive() -> bool { return s_type != Type::None; }

auto ScreenTransition::getProgress() -> float {
	if (s_type == Type::None || s_duration <= 0.f)
		return 1.f;
	return std::clamp(s_elapsed / s_duration, 0.f, 1.f);
}

void ScreenTransition::reset() {
	s_type = Type::None;
	s_duration = 0.f;
	s_elapsed = 0.f;
}

}// namespace owl::scene
