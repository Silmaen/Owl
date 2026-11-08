/**
 * @file NavBench.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <math/matrixCreation.h>
#include <scene/nav/Avoidance.h>
#include <scene/nav/NavMesh.h>

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

namespace owl::bench {

namespace {

using namespace scene::nav;

constexpr uint32_t crateRows = 6;
constexpr uint32_t bodyRows = 8;

// A room of 24 x 24 with 36 crates in rows: many holes, many regions.
auto makeRoom() -> NavGeometry {
	NavGeometry geometry;
	geometry.addBox({24.f, 0.2f, 24.f}, math::translate(math::identity<float, 4>(), math::vec3{0.f, -0.1f, 0.f}));
	for (uint32_t row = 0; row < crateRows; ++row) {
		for (uint32_t column = 0; column < crateRows; ++column) {
			const math::vec3 centre{-9.f + 3.6f * static_cast<float>(column), 0.5f,
									-9.f + 3.6f * static_cast<float>(row)};
			geometry.addBox({1.f, 1.f, 1.f}, math::translate(math::identity<float, 4>(), centre));
		}
	}
	return geometry;
}

auto settings() -> NavMeshSettings {
	NavMeshSettings result;
	result.cellSize = 0.1f;
	result.cellHeight = 0.05f;
	return result;
}

}// namespace

void runNavBenches(Runner& ioRunner) {
	const auto geometry = makeRoom();
	const math::vec3 boundsMin{-12.5f, -1.f, -12.5f};
	const math::vec3 boundsMax{12.5f, 3.f, 12.5f};
	NavMesh mesh;
	ioRunner.measure("nav/bake/room_24x24_36_crates", 1,
					 [&]() -> void { std::ignore = mesh.build(geometry, settings(), boundsMin, boundsMax); });
	std::vector<NavPathPoint> path;
	ioRunner.measure("nav/find_path/room_24x24_36_crates", 1,
					 [&]() -> void { std::ignore = mesh.findPath({-11.f, 0.f, -11.f}, {11.f, 0.f, 11.f}, path); });
	std::vector<AvoidanceBody> bodies;
	for (uint32_t row = 0; row < bodyRows; ++row) {
		for (uint32_t column = 0; column < bodyRows; ++column) {
			const math::vec2 position{static_cast<float>(column) * 1.2f, static_cast<float>(row) * 1.2f};
			bodies.push_back({.position = position,
							  .velocity = {0.5f, 0.f},
							  .preferred = math::vec2{4.f, 4.f} - position,
							  .radius = 0.4f,
							  .maxSpeed = 2.f,
							  .responsive = true});
		}
	}
	ioRunner.measure("nav/avoidance/64_bodies", bodies.size(),
					 [&]() -> void { std::ignore = computeAvoidance(bodies, {}, 1.f / 60.f); });
}

}// namespace owl::bench
