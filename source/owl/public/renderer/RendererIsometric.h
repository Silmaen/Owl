/**
 * @file RendererIsometric.h
 * @author Silmaen
 * @date 27/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/matrices.h"
#include "math/vectors.h"

namespace owl::renderer {

/**
 * @brief
 *  Per-layer configuration for the isometric renderer.
 *
 * Describes a fixed 2:1 dimetric projection in the Transport Tycoon Deluxe
 * tradition: there is no free-look camera. World axes map to screen pixels as
 * `screen.x = origin.x + (worldX - worldY) * tileWidth / 2` and
 * `screen.y = origin.y + (worldX + worldY) * tileHeight / 2 - worldZ * zStep`,
 * with screen Y growing downward. Tile sprites are 64x32 px by default.
 */
struct OWL_API IsometricConfig {
	/// Tile sprite footprint in pixels; the X:Y ratio sets the dimetric angle (default 64x32 = 2:1).
	math::vec2ui tileSize{64u, 32u};
	/// Vertical pixels added per world Z unit (elevation step for stacked tiles / buildings).
	float zStep = 16.f;
	/// Screen-space pixel offset where world origin (0, 0, 0) projects.
	math::vec2 origin{0.f, 0.f};
};

/**
 * @brief
 *  Project a world-space position to dimetric screen-space pixels.
 *
 * Screen Y grows downward (standard screen convention), so a larger
 * `worldX + worldY` lands lower on screen and a larger `worldZ` lifts the
 * point upward.
 * @param[in] iWorld The world-space position.
 * @param[in] iConfig The isometric projection configuration.
 * @return The screen-space pixel position.
 */
[[nodiscard]] OWL_API auto worldToScreen(const math::vec3& iWorld, const IsometricConfig& iConfig) -> math::vec2;

/**
 * @brief
 *  Inverse of `worldToScreen` for a known elevation.
 *
 * The dimetric projection collapses one degree of freedom, so the caller must
 * supply the world Z (elevation) of the plane being picked; the X/Y world
 * coordinates are then recovered exactly.
 * @param[in] iScreen The screen-space pixel position.
 * @param[in] iWorldZ The world elevation of the picked plane.
 * @param[in] iConfig The isometric projection configuration.
 * @return The world-space position on the `iWorldZ` plane.
 */
[[nodiscard]] OWL_API auto screenToWorld(const math::vec2& iScreen, float iWorldZ, const IsometricConfig& iConfig)
		-> math::vec3;

/**
 * @brief
 *  Build the view-projection matrix encoding the dimetric projection.
 *
 * The returned matrix maps world-space directly to normalized device
 * coordinates (no perspective divide), consistent with `worldToScreen` mapped
 * through the given viewport. Bound by the layer so `Renderer2D`, the editor
 * gizmos and picking all share the same isometric basis.
 * @param[in] iViewport The active viewport size in pixels.
 * @param[in] iConfig The isometric projection configuration.
 * @return The world-to-NDC view-projection matrix.
 */
[[nodiscard]] OWL_API auto dimetricViewProjection(const math::vec2ui& iViewport, const IsometricConfig& iConfig)
		-> math::mat4;

}// namespace owl::renderer
