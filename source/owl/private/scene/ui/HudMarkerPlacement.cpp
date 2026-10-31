/**
 * @file HudMarkerPlacement.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ui/HudMarkerPlacement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace owl::scene::ui {

auto placeHudMarker(const renderer::ViewportPoint& iPoint, const math::vec2& iViewport, const float iMargin)
		-> HudMarkerPlacement {
	const math::vec2 centre = iViewport * 0.5f;
	const math::vec2 delta = iPoint.position - centre;
	const float angle = std::atan2(delta.y(), delta.x());
	const bool inside = iPoint.position.x() >= 0.f && iPoint.position.x() <= iViewport.x() &&
						iPoint.position.y() >= 0.f && iPoint.position.y() <= iViewport.y();
	if (iPoint.ahead && inside)
		return {.position = iPoint.position, .onScreen = true, .angle = angle};
	const math::vec2 half{std::max(0.f, centre.x() - iMargin), std::max(0.f, centre.y() - iMargin)};
	constexpr float epsilon = 1e-6f;
	// Straight behind the camera the point sits on the centre: send it down, where a turn-around arrow reads best.
	if (std::abs(delta.x()) < epsilon && std::abs(delta.y()) < epsilon)
		return {.position = {centre.x(), centre.y() + half.y()},
				.onScreen = false,
				.angle = std::numbers::pi_v<float> / 2.f};
	const float scaleX =
			std::abs(delta.x()) > epsilon ? half.x() / std::abs(delta.x()) : std::numeric_limits<float>::max();
	const float scaleY =
			std::abs(delta.y()) > epsilon ? half.y() / std::abs(delta.y()) : std::numeric_limits<float>::max();
	return {.position = centre + delta * std::min(scaleX, scaleY), .onScreen = false, .angle = angle};
}

}// namespace owl::scene::ui
