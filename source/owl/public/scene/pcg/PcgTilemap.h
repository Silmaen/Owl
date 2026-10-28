/**
 * @file PcgTilemap.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "scene/TilemapAsset.h"
#include "scene/pcg/PcgGraph.h"

#include <cstdint>

namespace owl::scene::pcg {

/**
 * @brief
 *  Region of a tilemap a generator writes: its size, clamped to the grid (a zero side reaches the grid end).
 * @param[in] iAsset The tilemap.
 * @param[in] iGenerator The settings.
 * @return The region and the seed of the settings, an empty size when the origin is outside the grid.
 */
[[nodiscard]] OWL_API auto getRegion(const TilemapAsset& iAsset, const TilemapGenerator& iGenerator) -> Region;

/**
 * @brief
 *  Write the tiles of a result on a tilemap layer, the region's top-left cell on `iGenerator.origin`.
 * @param[in] iResult The result of `evaluate`.
 * @param[in,out] ioAsset The tilemap.
 * @param[in] iGenerator Layer and origin.
 * @return The number of cells changed.
 */
OWL_API auto applyToTilemap(const Result& iResult, TilemapAsset& ioAsset, const TilemapGenerator& iGenerator)
		-> uint32_t;

/**
 * @brief
 *  Evaluate a graph on the region of a tilemap and write its tiles.
 * @param[in] iGraph The graph.
 * @param[in,out] ioAsset The tilemap.
 * @param[in] iGenerator Seed, layer and region.
 * @return The number of cells changed, or why nothing was generated.
 */
OWL_API auto generateTilemap(const Graph& iGraph, TilemapAsset& ioAsset, const TilemapGenerator& iGenerator)
		-> expected<uint32_t, PcgError>;

}// namespace owl::scene::pcg
