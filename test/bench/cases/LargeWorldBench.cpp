/**
 * @file LargeWorldBench.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/PlanetTerrain.h>
#include <scene/Scene.h>
#include <scene/WorldStreamer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

auto benchSurface() -> shared<const renderer::PlanetSurface> {
	renderer::PlanetSettings settings;
	settings.radius = 6.4e6;
	settings.heightScale = 8000.f;
	settings.maxLevel = 14;
	settings.chunkResolution = 16;
	renderer::PlanetHeightMap heights{.size = {256, 256}, .values = {}};
	heights.values.reserve(size_t{256} * 256);
	for (uint32_t y = 0; y < 256; ++y)
		for (uint32_t x = 0; x < 256; ++x)
			heights.values.push_back(static_cast<float>((x * 31 + y * 17) % 256) / 255.f);
	return mkShared<const renderer::PlanetSurface>(settings, std::move(heights), 11);
}

}// namespace

void runLargeWorldBenches(Runner& ioRunner) {
	const auto surface = benchSurface();
	const math::vec3d nearGround{0.0, 6.4e6 + 1500.0, 0.0};
	std::vector<renderer::PlanetChunkKey> leaves;
	renderer::selectPlanetChunks(surface->getSettings(), nearGround, leaves);
	ioRunner.measure("planet/select/near_ground", leaves.size(), [&]() -> void {
		renderer::selectPlanetChunks(surface->getSettings(), nearGround, leaves);
		doNotOptimize(leaves.size());
	});
	const renderer::PlanetChunkKey key{.face = 2, .level = 10, .x = 512, .y = 512};
	ioRunner.measure("planet/build_chunk/16", 1, [&]() -> void {
		const auto mesh = renderer::buildPlanetChunk(*surface, key);
		doNotOptimize(mesh.vertices.size());
	});
	// The frame of a planet whose chunks are all built: selection, draw list, cache upkeep.
	renderer::PlanetTerrain terrain;
	terrain.setSurface(surface);
	for (int frame = 0; frame < 200; ++frame) terrain.update(nearGround, false, nullptr);
	ioRunner.measure("planet/update/steady", terrain.getStats().selected, [&]() -> void {
		terrain.update(nearGround, false, nullptr);
		doNotOptimize(terrain.getDrawList().size());
	});
	// The frame of a world partition of 4096 declared cells around one source, nothing to load or unload.
	scene::Scene world;
	auto& partition = world.getWorldPartitionSettings();
	partition.enabled = true;
	partition.cellSize = 100.0;
	partition.loadRadius = 0.0;
	partition.unloadRadius = 0.0;
	for (int32_t x = 0; x < 64; ++x)
		for (int32_t z = 0; z < 64; ++z)
			partition.cells.push_back({.coords = {x, 0, z}, .scene = std::format("cells/{}_{}.owl", x, z)});
	partition.cells.push_back({.coords = {-10, 0, -10}, .scene = std::string{}});
	scene::WorldStreamer streamer;
	const std::array sources{scene::StreamPoint{.position = {-950.0, 50.0, -950.0}, .radiusScale = 1.0}};
	streamer.update(world, sources, nullptr);
	ioRunner.measure("world_streaming/update/4096_cells", partition.cells.size(), [&]() -> void {
		streamer.update(world, sources, nullptr);
		doNotOptimize(streamer.getStats().loaded);
	});
}

}// namespace owl::bench
