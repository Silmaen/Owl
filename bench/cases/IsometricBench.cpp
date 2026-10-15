/**
 * @file IsometricBench.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <math/vectors.h>
#include <renderer/RendererIsometric.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapTerrain.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace owl::bench {

namespace {

// Rolling hills: a raised corner every 16 cells, so the corner heights reach 8 steps and slopes fill the map.
auto makeHills(const uint32_t iSize) -> scene::TilemapAsset {
	scene::TilemapAsset map;
	map.width = iSize;
	map.height = iSize;
	map.addLayer("ground").fill(0);
	for (uint32_t y = 8; y < iSize; y += 16) {
		for (uint32_t x = 8; x < iSize; x += 16) std::ignore = map.setCornerHeight(x, y, 8);
	}
	return map;
}

}// namespace

void runIsometricBenches(Runner& ioRunner) {
	if (!ioRunner.wants("iso"))
		return;
	constexpr uint32_t size = 256;
	const auto map = makeHills(size);
	renderer::IsometricCamera camera;
	camera.setViewportSize({1920U, 1080U});
	camera.setTarget({static_cast<float>(size) * 0.5f, static_cast<float>(size) * 0.5f, 0.f});
	const math::vec3 origin{0.f, 0.f, 0.f};
	std::array<math::vec2, 16> pixels{};
	for (size_t i = 0; i < pixels.size(); ++i)
		pixels[i] = {120.f + static_cast<float>(i % 4) * 560.f, 90.f + static_cast<float>(i / 4) * 280.f};
	size_t cursor = 0;
	uint32_t hits = 0;
	ioRunner.measure("iso/pick/256x256_hills", 1, [&]() -> void {
		cursor = (cursor + 1) % pixels.size();
		hits += scene::terrain::pickTerrain(map, camera, pixels[cursor], origin).has_value() ? 1U : 0U;
	});
	ioRunner.metric("iso/pick/256x256_hills_hits", hits, "picks that found the terrain (budget: 20 us each)");
}

}// namespace owl::bench
