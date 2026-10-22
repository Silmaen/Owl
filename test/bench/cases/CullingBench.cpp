/**
 * @file CullingBench.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <data/geometry/LooseOctree.h>
#include <data/geometry/MeshLoader.h>
#include <math/Frustum.h>
#include <math/matrixCreation.h>
#include <math/trigonometry.h>
#include <renderer/CameraOrtho.h>
#include <renderer/MeshCache.h>
#include <renderer/RendererMesh.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <random>
#include <vector>

namespace owl::bench {

namespace {

// Boxes of 0.2 to 4 units spread over a cube of 400 units, as the props of an open level.
auto randomBoxes(const uint32_t iCount) -> std::vector<math::box3f> {
	std::mt19937 random{1234};
	std::uniform_real_distribution<float> position{-200.f, 200.f};
	std::uniform_real_distribution<float> half{0.1f, 2.f};
	std::vector<math::box3f> boxes;
	boxes.reserve(iCount);
	for (uint32_t index = 0; index < iCount; ++index) {
		const math::vec3 centre{position(random), position(random) * 0.1f, position(random)};
		const float extent = half(random);
		boxes.emplace_back(centre - math::vec3{extent, extent, extent}, centre + math::vec3{extent, extent, extent});
	}
	return boxes;
}

void runOctree(Runner& ioRunner) {
	for (const uint32_t count: {1000U, 10000U, 100000U}) {
		const auto boxes = randomBoxes(count);
		data::geometry::LooseOctree tree;
		ioRunner.measureWithSetup(
				std::format("culling/octree/insert/{}", count), count, [&]() -> void { tree.clear(); },
				[&]() -> void {
					for (uint32_t index = 0; index < count; ++index) tree.insert(index, boxes[index]);
				});
		for (uint32_t index = 0; index < count; ++index) tree.insert(index, boxes[index]);
		// A camera in the middle of the level, 60 degrees, 150 units deep: about a tenth of the boxes.
		auto view = math::identity<float, 4>();
		view(1, 3) = -2.f;
		const auto frustum = math::Frustum::fromViewProjection(
				math::perspectiveReversedZ(math::radians(60.f), 1.78f, 0.1f, 150.f) * view);
		std::vector<uint32_t> ids;
		ioRunner.measure(std::format("culling/octree/frustum_query/{}", count), count, [&]() -> void {
			ids.clear();
			tree.query(frustum, ids);
			doNotOptimize(ids.size());
		});
		ioRunner.measure(std::format("culling/octree/brute_force/{}", count), count, [&]() -> void {
			uint32_t hits = 0;
			for (const auto& box: boxes) hits += frustum.intersects(box) ? 1 : 0;
			doNotOptimize(hits);
		});
		std::mt19937 random{99};
		std::uniform_real_distribution<float> step{-0.5f, 0.5f};
		ioRunner.measure(std::format("culling/octree/move_tenth/{}", count), count / 10, [&]() -> void {
			for (uint32_t index = 0; index < count; index += 10) {
				auto box = tree.getBox(index);
				box.translate(math::vec3{step(random), 0.f, step(random)});
				tree.insert(index, box);
			}
		});
	}
}

void runMeshFlush(Runner& ioRunner) {
	const auto file =
			std::filesystem::path(OWL_BENCH_SOURCE_DIR) / "engine_demo" / "meshes" / "tavern" / "log_stack.glb";
	const auto loaded = data::geometry::MeshLoader::loadMesh(file);
	if (!loaded.mesh)
		return;
	renderer::MeshResource resource{file, loaded};
	const renderer::CameraOrtho camera{-200.f, 200.f, -200.f, 200.f};
	for (const uint32_t count: {1000U, 10000U}) {
		std::vector<math::mat4> models;
		std::mt19937 random{7};
		std::uniform_real_distribution<float> position{-250.f, 250.f};
		for (uint32_t index = 0; index < count; ++index) {
			auto model = math::identity<float, 4>();
			model(0, 3) = position(random);
			model(1, 3) = position(random);
			models.push_back(model);
		}
		ioRunner.measure(std::format("culling/mesh_flush/{}_instances", count), count, [&]() -> void {
			renderer::RendererMesh::beginScene(camera, {});
			for (uint32_t index = 0; index < count; ++index)
				renderer::RendererMesh::drawMesh(resource, models[index], nullptr,
												 {.entityId = static_cast<int32_t>(index)}, false);
			renderer::RendererMesh::endScene();
		});
	}
}

}// namespace

void runCullingBenches(Runner& ioRunner) {
	if (ioRunner.wants("culling/octree"))
		runOctree(ioRunner);
	if (ioRunner.wants("culling/mesh_flush"))
		runMeshFlush(ioRunner);
}

}// namespace owl::bench
