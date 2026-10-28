/**
 * @file PcgTilemap.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/pcg/PcgTilemap.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace owl::scene::pcg {

auto getRegion(const TilemapAsset& iAsset, const TilemapGenerator& iGenerator) -> Region {
	Region region{.size = {0, 0}, .seed = iGenerator.seed};
	if (iGenerator.origin.x() >= iAsset.width || iGenerator.origin.y() >= iAsset.height)
		return region;
	const uint32_t spaceX = iAsset.width - iGenerator.origin.x();
	const uint32_t spaceY = iAsset.height - iGenerator.origin.y();
	region.size = {iGenerator.size.x() == 0 ? spaceX : std::min(iGenerator.size.x(), spaceX),
				   iGenerator.size.y() == 0 ? spaceY : std::min(iGenerator.size.y(), spaceY)};
	return region;
}

auto applyToTilemap(const Result& iResult, TilemapAsset& ioAsset, const TilemapGenerator& iGenerator) -> uint32_t {
	if (iGenerator.layer >= ioAsset.layers.size()) {
		OWL_CORE_WARN("PcgGraph: The tilemap has no layer {} to generate on.", iGenerator.layer)
		return 0;
	}
	uint32_t changed = 0;
	for (uint32_t y = 0; y < iResult.size.y(); ++y) {
		for (uint32_t x = 0; x < iResult.size.x(); ++x) {
			const auto tile = iResult.tiles[static_cast<size_t>(y) * iResult.size.x() + x];
			const uint32_t cellX = iGenerator.origin.x() + x;
			const uint32_t cellY = iGenerator.origin.y() + y;
			if (tile == g_UntouchedTile || cellX >= ioAsset.width || cellY >= ioAsset.height ||
				ioAsset.getTile(iGenerator.layer, cellX, cellY) == tile)
				continue;
			ioAsset.setTile(iGenerator.layer, cellX, cellY, tile);
			++changed;
		}
	}
	return changed;
}

auto generateTilemap(const Graph& iGraph, TilemapAsset& ioAsset, const TilemapGenerator& iGenerator)
		-> expected<uint32_t, PcgError> {
	OWL_PROFILE_FUNCTION()

	const auto result = evaluate(iGraph, getRegion(ioAsset, iGenerator));
	if (!result)
		return unexpected{result.error()};
	return applyToTilemap(*result, ioAsset, iGenerator);
}

}// namespace owl::scene::pcg
