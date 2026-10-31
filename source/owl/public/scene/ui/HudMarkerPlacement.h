/**
 * @file HudMarkerPlacement.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderLayer.h"

namespace owl::scene::ui {

/**
 * @brief
 *  Where the HUD pass draws a `HudMarker`.
 */
struct OWL_API HudMarkerPlacement {
	/// Centre of the icon, in viewport pixels from the top-left corner, Y down.
	math::vec2 position{0.f, 0.f};
	/// Whether the entity is in view; false puts the marker on the screen edge.
	bool onScreen = true;
	/// Direction from the screen centre towards the entity, in radians (0 = right, positive = down).
	float angle = 0.f;
};

/**
 * @brief
 *  Place a marker: on its projected point when it is in view, else on the screen edge towards it.
 *
 * An off-screen point slides along the line from the screen centre until it lies `iMargin` pixels inside the
 * border; a point behind the camera goes to the edge as well, on the side the camera has to turn to.
 * @param[in] iPoint The projected point of the entity.
 * @param[in] iViewport The viewport size in pixels.
 * @param[in] iMargin Distance kept between an edge marker and the border, in pixels.
 * @return The placement.
 */
[[nodiscard]] OWL_API auto placeHudMarker(const renderer::ViewportPoint& iPoint, const math::vec2& iViewport,
										  float iMargin) -> HudMarkerPlacement;

}// namespace owl::scene::ui
