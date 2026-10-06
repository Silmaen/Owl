/**
 * @file VoxelBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <data/voxel/ChunkMesher.h>
#include <data/voxel/TerrainGenerator.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/VoxelWorld.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace owl::bench {

namespace {

using data::voxel::BlockId;
using data::voxel::Chunk;
using data::voxel::g_ChunkSize;

struct Palette {
	data::voxel::BlockRegistry registry;
	data::voxel::TerrainParams params;

	Palette() {
		const auto add = [&](const char* iName, const data::voxel::BlockRenderKind iKind,
							 const uint16_t iTex) -> BlockId {
			data::voxel::BlockType type;
			type.name = iName;
			type.renderKind = iKind;
			type.solid = iKind != data::voxel::BlockRenderKind::Water;
			type.setAllFaces(iTex);
			return registry.registerBlock(type);
		};
		params.stone = add("stone", data::voxel::BlockRenderKind::Opaque, 1);
		params.grass = add("grass", data::voxel::BlockRenderKind::Opaque, 2);
		params.dirt = add("dirt", data::voxel::BlockRenderKind::Opaque, 3);
		params.sand = add("sand", data::voxel::BlockRenderKind::Opaque, 4);
		params.water = add("water", data::voxel::BlockRenderKind::Water, 5);
	}
};

auto meshBytes(const data::voxel::ChunkMesh& iMesh) -> size_t {
	return iMesh.vertices.size() * sizeof(data::voxel::VoxelVertex) + iMesh.indices.size() * sizeof(uint32_t);
}

auto makePatterns(const Palette& iPalette) -> std::vector<std::pair<std::string, Chunk>> {
	std::vector<std::pair<std::string, Chunk>> chunks;
	Chunk full;
	full.fill(iPalette.params.stone);
	chunks.emplace_back("full", full);
	Chunk random;
	std::mt19937 rng(42U);
	std::bernoulli_distribution coin(0.5);
	Chunk checker;
	for (uint32_t z = 0; z < g_ChunkSize; ++z) {
		for (uint32_t y = 0; y < g_ChunkSize; ++y) {
			for (uint32_t x = 0; x < g_ChunkSize; ++x) {
				const auto ix = static_cast<int32_t>(x);
				const auto iy = static_cast<int32_t>(y);
				const auto iz = static_cast<int32_t>(z);
				if (coin(rng))
					random.setBlock(ix, iy, iz, iPalette.params.stone);
				if ((x + y + z) % 2 == 0)
					checker.setBlock(ix, iy, iz, iPalette.params.stone);
			}
		}
	}
	chunks.emplace_back("random50", random);
	chunks.emplace_back("checkerboard", checker);
	const data::voxel::TerrainGenerator generator(iPalette.params);
	Chunk surface(math::vec3i{0, 0, 0});
	generator.generateChunk(surface);
	chunks.emplace_back("terrain_surface", surface);
	Chunk underground(math::vec3i{0, -3, 0});
	generator.generateChunk(underground);
	chunks.emplace_back("terrain_underground", underground);
	return chunks;
}

void runGeneration(Runner& ioRunner, const Palette& iPalette) {
	const data::voxel::TerrainGenerator generator(iPalette.params);
	for (const auto& [label, coord]:
		 {std::pair{"surface", math::vec3i{0, 0, 0}}, std::pair{"underground", math::vec3i{0, -3, 0}},
		  std::pair{"sky", math::vec3i{0, 4, 0}}}) {
		Chunk chunk(coord);
		ioRunner.measure(std::format("voxel/generate_chunk/{}", label), g_ChunkSize * g_ChunkSize * g_ChunkSize,
						 [&]() -> void {
							 chunk.fill(data::voxel::g_AirBlock);
							 generator.generateChunk(chunk);
							 doNotOptimize(chunk.blocks().data());
						 });
	}
	ioRunner.metric(
			"voxel/chunk_storage_bytes",
			static_cast<double>(data::voxel::g_ChunkVolume * (sizeof(BlockId) + sizeof(data::voxel::PackedMeta))),
			"bytes (blocks + meta, excluding mesh)");
}

void runMeshing(Runner& ioRunner, const Palette& iPalette) {
	const data::voxel::ChunkMesher::NeighborProvider air = [](int32_t, int32_t, int32_t) -> BlockId {
		return data::voxel::g_AirBlock;
	};
	for (const auto& [label, chunk]: makePatterns(iPalette)) {
		for (const bool ao: {false, true}) {
			const std::string name = std::format("voxel/mesh_by_kind/{}/{}", label, ao ? "ao" : "no_ao");
			ioRunner.measure(name, 1, [&]() -> void {
				doNotOptimize(data::voxel::ChunkMesher::meshByKind(chunk, iPalette.registry, air, ao));
			});
		}
		const auto set = data::voxel::ChunkMesher::meshByKind(chunk, iPalette.registry, air, true);
		ioRunner.metric(std::format("voxel/mesh_by_kind/{}/quads", label),
						static_cast<double>(set.opaque.quadCount() + set.transparent.quadCount()), "quads");
		ioRunner.metric(std::format("voxel/mesh_by_kind/{}/mesh_bytes", label),
						static_cast<double>(meshBytes(set.opaque) + meshBytes(set.transparent)), "bytes");
		ioRunner.measure(std::format("voxel/encode_rle/{}", label), 1,
						 [&]() -> void { doNotOptimize(chunk.encode()); });
	}
}

void runSceneCopy(Runner& ioRunner, const Palette& iPalette) {
	const data::voxel::TerrainGenerator generator(iPalette.params);
	for (const int32_t radius: {1, 4}) {
		auto scn = mkShared<scene::Scene>();
		auto& voxel = scn->createEntity("voxel").addComponent<scene::component::VoxelWorld>();
		voxel.registry = iPalette.registry;
		for (int32_t cz = -radius; cz <= radius; ++cz) {
			for (int32_t cy = -2; cy <= 2; ++cy) {
				for (int32_t cx = -radius; cx <= radius; ++cx)
					generator.generateChunk(*voxel.world.getOrCreateChunk(math::vec3i{cx, cy, cz}));
			}
		}
		const auto chunks = voxel.world.chunkCount();
		shared<scene::Scene> copy;
		ioRunner.measureWithSetup(
				std::format("voxel/scene_copy/{}chunks", chunks), static_cast<uint64_t>(chunks),
				[&]() -> void { copy.reset(); }, [&]() -> void { copy = scene::Scene::copy(scn); });
	}
}

}// namespace

void runVoxelBenches(Runner& ioRunner) {
	if (!ioRunner.wants("voxel"))
		return;
	const Palette palette;
	runGeneration(ioRunner, palette);
	runMeshing(ioRunner, palette);
	runSceneCopy(ioRunner, palette);
}

}// namespace owl::bench
