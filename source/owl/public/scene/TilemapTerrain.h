/**
 * @file TilemapTerrain.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/RendererIsometric.h"
#include "scene/TilemapAsset.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Slopes and ray picking of the isometric terrain (corner heights of a `TilemapAsset`).
 */
namespace owl::scene::terrain {

/// Number of slopes: 15 (flat, 4 corners up, 4 corners down, 4 edges up, 2 saddles) and 4 steep ones.
inline constexpr uint32_t slopeCount = 19;

/// Slope of a cell whose four corners have the same height.
inline constexpr uint8_t flatSlope = 0;

/// First slope with a corner two steps above the lowest one.
inline constexpr uint8_t firstSteepSlope = 15;

/// Cell flag bit asking the renderer for the foundation of the terrain tile instead of its slope.
inline constexpr uint8_t foundationFlag = 0x01;

/// World height of one corner step, in tiles (8 pixels with the default 2:1 view).
inline constexpr float heightStep = 0.5f;

/// Four corner values of a cell: back `(x, y)`, right `(x + 1, y)`, front `(x + 1, y + 1)`, left `(x, y + 1)`.
using CornerSet = std::array<uint8_t, 4>;

/**
 * @brief
 *  Corner heights of a slope, relative to its lowest corner.
 * @param[in] iSlope The slope, `[0, slopeCount)`.
 * @return The heights (the flat profile for an out-of-range slope).
 */
[[nodiscard]] OWL_API auto getSlopeProfile(uint8_t iSlope) -> const CornerSet&;

/**
 * @brief
 *  Slope of four corner heights.
 *
 * The heights are taken relative to the lowest one. Corners next to each other must differ by at most one step.
 * @param[in] iCorners Absolute corner heights.
 * @return The slope, or nothing when the corners match none of the 19.
 */
[[nodiscard]] OWL_API auto deriveSlope(const CornerSet& iCorners) -> std::optional<uint8_t>;

/**
 * @brief
 *  Name of a slope.
 * @param[in] iSlope The slope.
 * @return A short name (`"Flat"`, `"Corner back"`, ...), `"Invalid"` when out of range.
 */
[[nodiscard]] OWL_API auto getSlopeName(uint8_t iSlope) -> std::string_view;

/**
 * @brief
 *  Whether a slope is one of the four steep ones.
 * @param[in] iSlope The slope.
 * @return True for a corner two steps above the lowest.
 */
[[nodiscard]] OWL_API auto isSteepSlope(uint8_t iSlope) -> bool;

/// Result of a terrain pick.
struct OWL_API TerrainHit {
	/// Cell under the ray.
	math::vec2ui cell{0, 0};
	/// Corner of the cell the hit point is the closest to.
	math::vec2ui corner{0, 0};
	/// World position of the hit.
	math::vec3 point{0.f, 0.f, 0.f};
};

/**
 * @brief
 *  First point where a ray meets the terrain surface.
 *
 * Walks the cells under the ray and tests it against the two triangles of each candidate cell; the diagonal is the
 * one joining the corners whose heights are the closest.
 * @param[in] iAsset The tilemap asset holding the corner heights.
 * @param[in] iMapOrigin World position of the back corner of the map (the entity translation).
 * @param[in] iRayOrigin Start of the ray (above the terrain for a camera ray).
 * @param[in] iRayDirection Direction of the ray (any length, must go down).
 * @return The hit, or nothing when the ray misses the map.
 */
[[nodiscard]] OWL_API auto pickTerrain(const TilemapAsset& iAsset, const math::vec3& iMapOrigin,
									   const math::vec3& iRayOrigin, const math::vec3& iRayDirection)
		-> std::optional<TerrainHit>;

/**
 * @brief
 *  Merge the changes of several terraforming calls: one entry per corner, with its first `before` and its last
 *  `after`; corners that ended where they started are dropped.
 * @param[in,out] ioChanges The changes, sorted by corner index on return.
 */
OWL_API void mergeCornerChanges(std::vector<CornerHeightChange>& ioChanges);

/**
 * @brief
 *  Terrain cell under a viewport pixel of an isometric camera.
 * @param[in] iAsset The tilemap asset holding the corner heights.
 * @param[in] iCamera The camera of the viewport.
 * @param[in] iPixel Viewport pixel, origin at the top left, Y down.
 * @param[in] iMapOrigin World position of the back corner of the map (the entity translation).
 * @return The hit, or nothing when the pixel is off the map.
 */
[[nodiscard]] OWL_API auto pickTerrain(const TilemapAsset& iAsset, const renderer::IsometricCamera& iCamera,
									   const math::vec2& iPixel, const math::vec3& iMapOrigin)
		-> std::optional<TerrainHit>;

}// namespace owl::scene::terrain
