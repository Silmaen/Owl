/**
 * @file ChunkNeighborhood_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/ChunkMesher.h>
#include <data/voxel/ChunkNeighborhood.h>
#include <data/voxel/VoxelWorld.h>

#include <cstddef>
#include <cstdint>
#include <random>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class ChunkNeighborhoodFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

struct Palette {
	BlockRegistry registry;
	BlockId stone = 0;
	BlockId glass = 0;
	BlockId water = 0;

	Palette() {
		const auto add = [&](const char* iName, const BlockRenderKind iKind, const uint16_t iFirstTexture) -> BlockId {
			BlockType type;
			type.name = iName;
			type.renderKind = iKind;
			type.solid = iKind != BlockRenderKind::Water;
			for (uint16_t f = 0; f < g_FaceCount; ++f) type.faceTextures[f] = static_cast<uint16_t>(iFirstTexture + f);
			return registry.registerBlock(type);
		};
		stone = add("stone", BlockRenderKind::Opaque, 1);
		glass = add("glass", BlockRenderKind::Transparent, 10);
		water = add("water", BlockRenderKind::Water, 20);
	}
};

// Random blocks (with random orientations) over the 3x3x3 chunks around the origin chunk.
// 70 % stone, 20 % glass, 10 % water.
auto pick(const Palette& iPalette, const int iRoll) -> BlockId {
	if (iRoll < 7)
		return iPalette.stone;
	return iRoll < 9 ? iPalette.glass : iPalette.water;
}

auto makeRandomWorld(const Palette& iPalette, const uint32_t iSeed, const double iDensity) -> VoxelWorld {
	VoxelWorld world;
	std::mt19937 rng(iSeed);
	std::bernoulli_distribution solid(iDensity);
	std::uniform_int_distribution<int> kind(0, 9);
	std::uniform_int_distribution<int> orientation(0, 5);
	const auto size = static_cast<int32_t>(g_ChunkSize);
	for (int32_t y = -size; y < 2 * size; ++y) {
		for (int32_t z = -size; z < 2 * size; ++z) {
			for (int32_t x = -size; x < 2 * size; ++x) {
				if (!solid(rng))
					continue;
				const int k = kind(rng);
				const BlockId id = pick(iPalette, k);
				world.setBlock(math::vec3i{x, y, z}, id,
							   packMeta({.orientation = static_cast<BlockOrientation>(orientation(rng)), .state = 0}));
			}
		}
	}
	return world;
}

auto liveMesh(const VoxelWorld& iWorld, const BlockRegistry& iRegistry, const math::vec3i& iCoord, const bool iAo)
		-> ChunkMeshSet {
	const auto size = static_cast<int32_t>(g_ChunkSize);
	const ChunkMesher::NeighborProvider neighbor = [&](const int32_t iX, const int32_t iY,
													   const int32_t iZ) -> BlockId {
		return iWorld.getBlock(math::vec3i{iCoord.x() * size + iX, iCoord.y() * size + iY, iCoord.z() * size + iZ});
	};
	const auto chunk = iWorld.getChunk(iCoord);
	return ChunkMesher::meshByKind(chunk ? *chunk : Chunk{iCoord}, iRegistry, neighbor, iAo);
}

void expectSameMesh(const ChunkMesh& iA, const ChunkMesh& iB) {
	ASSERT_EQ(iA.vertices.size(), iB.vertices.size());
	ASSERT_EQ(iA.indices, iB.indices);
	for (size_t i = 0; i < iA.vertices.size(); ++i) {
		const auto& a = iA.vertices[i];
		const auto& b = iB.vertices[i];
		EXPECT_EQ(a.position, b.position) << "vertex " << i;
		EXPECT_EQ(a.normal, b.normal) << "vertex " << i;
		EXPECT_EQ(a.uv, b.uv) << "vertex " << i;
		EXPECT_EQ(a.textureIndex, b.textureIndex) << "vertex " << i;
		EXPECT_EQ(a.ao, b.ao) << "vertex " << i;
	}
}
}// namespace

TEST_F(ChunkNeighborhoodFixture, MeshMatchesLiveWorld) {
	const Palette palette;
	for (const double density: {0.15, 0.5, 0.85}) {
		const VoxelWorld world = makeRandomWorld(palette, 7U, density);
		for (const bool ao: {true, false}) {
			const auto neighborhood = ChunkNeighborhood::capture(world, math::vec3i{0, 0, 0});
			const auto captured = ChunkMesher::meshByKind(neighborhood, palette.registry, ao);
			const auto live = liveMesh(world, palette.registry, math::vec3i{0, 0, 0}, ao);
			EXPECT_FALSE(live.opaque.isEmpty());
			expectSameMesh(captured.opaque, live.opaque);
			expectSameMesh(captured.transparent, live.transparent);
		}
	}
}

TEST_F(ChunkNeighborhoodFixture, MeshMatchesLiveWorldWithMissingNeighbors) {
	const Palette palette;
	const VoxelWorld full = makeRandomWorld(palette, 11U, 0.5);
	VoxelWorld world;
	*world.getOrCreateChunk(math::vec3i{0, 0, 0}) = *full.getChunk(math::vec3i{0, 0, 0});
	*world.getOrCreateChunk(math::vec3i{1, 1, 0}) = *full.getChunk(math::vec3i{1, 1, 0});
	const auto neighborhood = ChunkNeighborhood::capture(world, math::vec3i{0, 0, 0});
	const auto captured = ChunkMesher::meshByKind(neighborhood, palette.registry);
	const auto live = liveMesh(world, palette.registry, math::vec3i{0, 0, 0}, true);
	expectSameMesh(captured.opaque, live.opaque);
	expectSameMesh(captured.transparent, live.transparent);
}

TEST_F(ChunkNeighborhoodFixture, CaptureCopiesBorderShell) {
	VoxelWorld world;
	world.setBlock(math::vec3i{-1, -1, -1}, 3);// corner neighbour (-1,-1,-1)
	world.setBlock(math::vec3i{16, 5, 7}, 4);// face neighbour (1,0,0)
	world.setBlock(math::vec3i{17, 5, 7}, 5);// two cells away: outside the shell
	world.setBlock(math::vec3i{4, 16, -1}, 6);// edge neighbour (0,1,-1)
	world.setBlock(math::vec3i{2, 3, 4}, 7);// inside the chunk
	const auto neighborhood = ChunkNeighborhood::capture(world, math::vec3i{0, 0, 0});
	EXPECT_EQ(neighborhood.getBlock(-1, -1, -1), 3u);
	EXPECT_EQ(neighborhood.getBlock(16, 5, 7), 4u);
	EXPECT_EQ(neighborhood.getBlock(17, 5, 7), g_AirBlock);
	EXPECT_EQ(neighborhood.getBlock(4, 16, -1), 6u);
	EXPECT_EQ(neighborhood.getBlock(2, 3, 4), 7u);
	EXPECT_EQ(neighborhood.getChunk().getBlock(2, 3, 4), 7u);
	EXPECT_EQ(neighborhood.getCoord(), (math::vec3i{0, 0, 0}));
}

TEST_F(ChunkNeighborhoodFixture, CaptureIsIndependentOfLaterEdits) {
	VoxelWorld world;
	world.setBlock(math::vec3i{15, 0, 0}, 1);
	world.setBlock(math::vec3i{16, 0, 0}, 1);
	const auto neighborhood = ChunkNeighborhood::capture(world, math::vec3i{0, 0, 0});
	const uint64_t revision = world.getChunk(math::vec3i{0, 0, 0})->getRevision();
	EXPECT_EQ(neighborhood.getRevision(), revision);
	world.setBlock(math::vec3i{15, 0, 0}, g_AirBlock);
	world.setBlock(math::vec3i{16, 0, 0}, g_AirBlock);
	EXPECT_EQ(neighborhood.getBlock(15, 0, 0), 1u);
	EXPECT_EQ(neighborhood.getBlock(16, 0, 0), 1u);
	EXPECT_NE(world.getChunk(math::vec3i{0, 0, 0})->getRevision(), revision);
}

TEST_F(ChunkNeighborhoodFixture, AbsentChunkCapturesAir) {
	VoxelWorld world;
	world.setBlock(math::vec3i{16, 0, 0}, 1);
	const auto neighborhood = ChunkNeighborhood::capture(world, math::vec3i{0, 0, 0});
	EXPECT_EQ(neighborhood.getRevision(), 0u);
	EXPECT_TRUE(neighborhood.getChunk().isEmpty());
	EXPECT_EQ(neighborhood.getBlock(16, 0, 0), 1u);
	EXPECT_EQ(neighborhood.getBlock(-2, 0, 0), g_AirBlock);
}
