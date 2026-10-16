/**
 * @file PathBench.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <scene/TilemapAsset.h>
#include <scene/nav/NavMap.h>
#include <scene/nav/NavProfile.h>
#include <scene/nav/PathQueue.h>
#include <scene/nav/Pathfinder.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <tuple>

namespace owl::bench {

namespace {

using namespace scene::nav;

constexpr int32_t roadTile = 1;
constexpr uint32_t mapSize = 1024;

auto makeProfile() -> NavProfile {
	NavProfile profile;
	profile.rules.push_back({.tile = roadTile});
	return profile;
}

// Every cell of the overlay is a road: the worst case for the flat search (open ground, many equal paths).
auto makeOpenMap() -> scene::TilemapAsset {
	scene::TilemapAsset asset;
	asset.resize(mapSize, mapSize);
	auto& layer = asset.addLayer("roads");
	for (uint32_t y = 0; y < mapSize; ++y)
		for (uint32_t x = 0; x < mapSize; ++x)
			std::ignore = layer.setCell(x, y, {.terrain = -1, .overlay = roadTile, .flags = 0});
	return asset;
}

// A street grid: a road every 8 cells, where a town of the size of the map would put them.
auto makeStreetMap() -> scene::TilemapAsset {
	scene::TilemapAsset asset;
	asset.resize(mapSize, mapSize);
	auto& layer = asset.addLayer("roads");
	for (uint32_t y = 0; y < mapSize; ++y) {
		for (uint32_t x = 0; x < mapSize; ++x) {
			if (x % 8 == 0 || y % 8 == 0)
				std::ignore = layer.setCell(x, y, {.terrain = -1, .overlay = roadTile, .flags = 0});
		}
	}
	return asset;
}

// Open ground cut by a wall every 64 columns, each with one gap at the top then at the bottom.
auto makeWalledMap() -> scene::TilemapAsset {
	auto asset = makeOpenMap();
	auto& layer = asset.layers[0];
	for (uint32_t wall = 0; wall * 64 + 63 < mapSize; ++wall) {
		const uint32_t x = wall * 64 + 63;
		const uint32_t gap = wall % 2 == 0 ? mapSize - 2 : 1;
		for (uint32_t y = 0; y < mapSize; ++y) {
			if (y != gap)
				std::ignore = layer.setCell(x, y, {.terrain = -1, .overlay = -1, .flags = 0});
		}
	}
	return asset;
}

// Streets every 8 rows, joined by one avenue at the west edge.
auto makeCombMap() -> scene::TilemapAsset {
	scene::TilemapAsset asset;
	asset.resize(mapSize, mapSize);
	auto& layer = asset.addLayer("roads");
	for (uint32_t y = 0; y < mapSize; ++y) {
		for (uint32_t x = 0; x < mapSize; ++x) {
			if (x == 0 || y % 8 == 0)
				std::ignore = layer.setCell(x, y, {.terrain = -1, .overlay = roadTile, .flags = 0});
		}
	}
	return asset;
}

void measureQueries(Runner& ioRunner, const std::string& iName, const NavMap& iMap, const math::vec2ui& iFrom,
					const math::vec2ui& iTo, const PathMode iMode) {
	if (!ioRunner.wants(iName))
		return;
	Pathfinder finder;
	PathResult path;
	std::ignore = finder.find(iMap, iFrom, iTo, iMode, path);
	ioRunner.metric(iName + "_expansions", static_cast<double>(finder.getWorkDone()), "nodes");
	ioRunner.metric(iName + "_path_cells", static_cast<double>(path.cells.size()), "cells");
	ioRunner.measure(iName, 1, [&]() -> void {
		std::ignore = finder.find(iMap, iFrom, iTo, iMode, path);
		doNotOptimize(path.cost);
	});
	if (iMode == PathMode::Hierarchical) {
		ioRunner.measureWithSetup(
				iName + "_cold", 1, [&iMap]() -> void { iMap.releaseSearchCaches(); },
				[&]() -> void {
					std::ignore = finder.find(iMap, iFrom, iTo, iMode, path);
					doNotOptimize(path.cost);
				});
	}
}

}// namespace

void runPathBenches(Runner& ioRunner) {
	const auto profile = makeProfile();
	const math::vec2ui corner{0, 0};
	const math::vec2ui far{mapSize - 1, mapSize - 1};
	const math::vec2ui farStreet{mapSize - 8, mapSize - 8};
	constexpr std::array<const char*, 13> names{"path/build/1024x1024_open_ms",
												"path/long_query/flat/1024x1024_open",
												"path/long_query/hierarchical/1024x1024_open",
												"path/long_query/flat/1024x1024_walls",
												"path/long_query/hierarchical/1024x1024_walls",
												"path/long_query/flat/1024x1024_comb",
												"path/long_query/hierarchical/1024x1024_comb",
												"path/long_query/flat/1024x1024_streets",
												"path/long_query/hierarchical/1024x1024_streets",
												"path/short_query/auto/1024x1024_streets",
												"path/edit/sync_one_cell",
												"path/edit/sync_and_reroute",
												"path/queue/update_4000_nodes"};
	if (std::ranges::any_of(names, [&ioRunner](const char* iName) -> bool { return ioRunner.wants(iName); })) {
		const auto open = makeOpenMap();
		NavMap openMap;
		openMap.setProfile(profile);
		const auto start = std::chrono::steady_clock::now();
		std::ignore = openMap.sync(open);
		ioRunner.metric("path/build/1024x1024_open_ms",
						std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
						"ms");
		measureQueries(ioRunner, "path/long_query/flat/1024x1024_open", openMap, corner, far, PathMode::Flat);
		measureQueries(ioRunner, "path/long_query/hierarchical/1024x1024_open", openMap, corner, far,
					   PathMode::Hierarchical);

		const auto walled = makeWalledMap();
		NavMap walledMap;
		walledMap.setProfile(profile);
		std::ignore = walledMap.sync(walled);
		const math::vec2ui walledFar{mapSize - 1, 1};
		measureQueries(ioRunner, "path/long_query/flat/1024x1024_walls", walledMap, {0, 1}, walledFar, PathMode::Flat);
		measureQueries(ioRunner, "path/long_query/hierarchical/1024x1024_walls", walledMap, {0, 1}, walledFar,
					   PathMode::Hierarchical);

		const auto comb = makeCombMap();
		NavMap combMap;
		combMap.setProfile(profile);
		std::ignore = combMap.sync(comb);
		measureQueries(ioRunner, "path/long_query/flat/1024x1024_comb", combMap, {mapSize - 1, 0}, {mapSize - 1, 1016},
					   PathMode::Flat);
		measureQueries(ioRunner, "path/long_query/hierarchical/1024x1024_comb", combMap, {mapSize - 1, 0},
					   {mapSize - 1, 1016}, PathMode::Hierarchical);

		const auto streets = makeStreetMap();
		NavMap streetMap;
		streetMap.setProfile(profile);
		std::ignore = streetMap.sync(streets);
		measureQueries(ioRunner, "path/long_query/flat/1024x1024_streets", streetMap, corner, farStreet,
					   PathMode::Flat);
		measureQueries(ioRunner, "path/long_query/hierarchical/1024x1024_streets", streetMap, corner, farStreet,
					   PathMode::Hierarchical);
		measureQueries(ioRunner, "path/short_query/auto/1024x1024_streets", streetMap, {400, 400}, {440, 416},
					   PathMode::Auto);

		auto edited = makeStreetMap();
		NavMap editedMap;
		editedMap.setProfile(profile);
		std::ignore = editedMap.sync(edited);
		uint32_t step = 0;
		if (ioRunner.wants("path/edit/sync_one_cell")) {
			ioRunner.measure("path/edit/sync_one_cell", 1, [&]() -> void {
				const uint32_t x = 8 * (1 + step++ % 100);
				auto& layer = edited.layers[0];
				const bool built = layer.getCell(x, 4).overlay == roadTile;
				std::ignore = layer.setCell(x, 4, {.terrain = -1, .overlay = built ? -1 : roadTile, .flags = 0});
				doNotOptimize(editedMap.sync(edited));
			});
		}
		if (ioRunner.wants("path/edit/sync_and_reroute")) {
			Pathfinder finder;
			PathResult path;
			ioRunner.measure("path/edit/sync_and_reroute", 1, [&]() -> void {
				const uint32_t x = 8 * (1 + step++ % 100);
				auto& layer = edited.layers[0];
				const bool built = layer.getCell(x, 400).overlay == roadTile;
				std::ignore = layer.setCell(x, 400, {.terrain = -1, .overlay = built ? -1 : roadTile, .flags = 0});
				std::ignore = editedMap.sync(edited);
				std::ignore = finder.find(editedMap, corner, farStreet, PathMode::Hierarchical, path);
				doNotOptimize(path.cost);
			});
		}
		if (ioRunner.wants("path/queue/update_4000_nodes")) {
			PathQueue queue;
			ioRunner.measure("path/queue/update_4000_nodes", 4000, [&]() -> void {
				const auto id = queue.request(corner, far, PathMode::Flat);
				std::ignore = queue.update(openMap, 4000);
				queue.release(id);
			});
		}
	}
}

}// namespace owl::bench
