/**
 * @file VoxelWorld_test.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/VoxelWorld.h>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class VoxelWorldFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};
}// namespace

TEST_F(VoxelWorldFixture, EmptyWorldReadsAir) {
	const VoxelWorld world;
	EXPECT_EQ(world.chunkCount(), 0u);
	EXPECT_EQ(world.getBlock(math::vec3i{0, 100, -50}), g_AirBlock);
}

TEST_F(VoxelWorldFixture, SetBlockCreatesChunk) {
	VoxelWorld world;
	world.setBlock(math::vec3i{3, 4, 5}, 8);
	EXPECT_EQ(world.chunkCount(), 1u);
	EXPECT_EQ(world.getBlock(math::vec3i{3, 4, 5}), 8u);
	EXPECT_EQ(world.getBlock(math::vec3i{3, 4, 6}), g_AirBlock);
}

TEST_F(VoxelWorldFixture, WorldToChunkPositive) {
	const math::vec3i chunk = worldToChunk(math::vec3i{16, 17, 31});
	EXPECT_EQ(chunk.x(), 1);
	EXPECT_EQ(chunk.y(), 1);
	EXPECT_EQ(chunk.z(), 1);
	const math::vec3i local = worldToLocal(math::vec3i{16, 17, 31});
	EXPECT_EQ(local.x(), 0);
	EXPECT_EQ(local.y(), 1);
	EXPECT_EQ(local.z(), 15);
}

TEST_F(VoxelWorldFixture, WorldToChunkNegative) {
	const math::vec3i chunk = worldToChunk(math::vec3i{-1, -16, -17});
	EXPECT_EQ(chunk.x(), -1);
	EXPECT_EQ(chunk.y(), -1);
	EXPECT_EQ(chunk.z(), -2);
	const math::vec3i local = worldToLocal(math::vec3i{-1, -16, -17});
	EXPECT_EQ(local.x(), 15);
	EXPECT_EQ(local.y(), 0);
	EXPECT_EQ(local.z(), 15);
}

TEST_F(VoxelWorldFixture, NegativeCoordsRoundTrip) {
	VoxelWorld world;
	world.setBlock(math::vec3i{-1, -1, -1}, 6);
	EXPECT_EQ(world.getBlock(math::vec3i{-1, -1, -1}), 6u);
	EXPECT_EQ(world.getBlock(math::vec3i{0, 0, 0}), g_AirBlock);
	const auto chunk = world.getChunk(math::vec3i{-1, -1, -1});
	ASSERT_NE(chunk, nullptr);
	EXPECT_EQ(chunk->getBlock(15, 15, 15), 6u);
}

TEST_F(VoxelWorldFixture, ChunkLifecycle) {
	VoxelWorld world;
	EXPECT_EQ(world.getChunk(math::vec3i{0, 0, 0}), nullptr);
	EXPECT_FALSE(world.hasChunk(math::vec3i{0, 0, 0}));
	const auto created = world.getOrCreateChunk(math::vec3i{0, 0, 0});
	ASSERT_NE(created, nullptr);
	EXPECT_TRUE(world.hasChunk(math::vec3i{0, 0, 0}));
	EXPECT_EQ(world.getOrCreateChunk(math::vec3i{0, 0, 0}), created);
	EXPECT_EQ(world.chunkCount(), 1u);
	EXPECT_TRUE(world.removeChunk(math::vec3i{0, 0, 0}));
	EXPECT_FALSE(world.removeChunk(math::vec3i{0, 0, 0}));
	EXPECT_EQ(world.chunkCount(), 0u);
}

TEST_F(VoxelWorldFixture, CrossChunkBoundary) {
	VoxelWorld world;
	world.setBlock(math::vec3i{15, 0, 0}, 1);
	world.setBlock(math::vec3i{16, 0, 0}, 2);
	EXPECT_EQ(world.chunkCount(), 2u);
	EXPECT_EQ(world.getBlock(math::vec3i{15, 0, 0}), 1u);
	EXPECT_EQ(world.getBlock(math::vec3i{16, 0, 0}), 2u);
}

TEST_F(VoxelWorldFixture, MarkNeighborChunksDirtyOnBorder) {
	VoxelWorld world;
	world.setBlock(math::vec3i{15, 0, 0}, 1);// last column of chunk (0,0,0)
	world.setBlock(math::vec3i{16, 0, 0}, 1);// first column of chunk (1,0,0)
	world.getChunk(math::vec3i{0, 0, 0})->markClean();
	world.getChunk(math::vec3i{1, 0, 0})->markClean();
	world.markNeighborChunksDirty(math::vec3i{15, 0, 0});
	EXPECT_TRUE(world.getChunk(math::vec3i{1, 0, 0})->isDirty());
	EXPECT_FALSE(world.getChunk(math::vec3i{0, 0, 0})->isDirty());// the edited chunk itself is left untouched
}

TEST_F(VoxelWorldFixture, MarkNeighborChunksDirtyInteriorIsNoOp) {
	VoxelWorld world;
	world.setBlock(math::vec3i{8, 8, 8}, 1);// interior block of chunk (0,0,0)
	world.getChunk(math::vec3i{0, 0, 0})->markClean();
	world.markNeighborChunksDirty(math::vec3i{8, 8, 8});
	EXPECT_FALSE(world.getChunk(math::vec3i{0, 0, 0})->isDirty());
}

TEST_F(VoxelWorldFixture, EnumerationHelpers) {
	VoxelWorld world;
	world.setBlock(math::vec3i{0, 0, 0}, 1);
	world.setBlock(math::vec3i{32, 0, 0}, 1);
	EXPECT_EQ(world.chunkCoordinates().size(), 2u);
	size_t visited = 0;
	world.forEachChunk([&visited](const math::vec3i&, const Chunk&) { ++visited; });
	EXPECT_EQ(visited, 2u);
	world.clear();
	EXPECT_EQ(world.chunkCount(), 0u);
}

TEST_F(VoxelWorldFixture, CopyDoesNotShareChunks) {
	VoxelWorld original;
	original.setBlock(math::vec3i{1, 2, 3}, 1);
	VoxelWorld copy(original);
	copy.setBlock(math::vec3i{1, 2, 3}, g_AirBlock);
	copy.setBlock(math::vec3i{40, 2, 3}, 2);
	EXPECT_EQ(original.getBlock(math::vec3i{1, 2, 3}), 1u);
	EXPECT_EQ(original.chunkCount(), 1u);
	EXPECT_NE(copy.getChunk(math::vec3i{0, 0, 0}), original.getChunk(math::vec3i{0, 0, 0}));
	VoxelWorld assigned;
	assigned = original;
	assigned.setBlock(math::vec3i{1, 2, 3}, 5);
	EXPECT_EQ(original.getBlock(math::vec3i{1, 2, 3}), 1u);
	EXPECT_EQ(assigned.getBlock(math::vec3i{1, 2, 3}), 5u);
}

TEST_F(VoxelWorldFixture, MarkNeighborChunksDirtyReachesEdgeAndCornerChunks) {
	VoxelWorld world;
	for (int32_t cy = 0; cy <= 1; ++cy)
		for (int32_t cz = 0; cz <= 1; ++cz)
			for (int32_t cx = 0; cx <= 1; ++cx) world.setBlock(math::vec3i{cx * 16 + 8, cy * 16 + 8, cz * 16 + 8}, 1);
	world.setBlock(math::vec3i{-8, 8, 8}, 1);// chunk (-1,0,0): not adjacent to the corner block
	std::vector<std::pair<math::vec3i, uint64_t>> before;
	world.forEachChunk([&](const math::vec3i& iCoord, const Chunk& iChunk) -> void {
		before.emplace_back(iCoord, iChunk.getRevision());
	});
	world.markNeighborChunksDirty(math::vec3i{15, 15, 15});// corner of chunk (0,0,0)
	for (const auto& [coord, revision]: before) {
		const bool expectChange = coord != math::vec3i{0, 0, 0} && coord != math::vec3i{-1, 0, 0};
		EXPECT_EQ(world.getChunk(coord)->getRevision() != revision, expectChange)
				<< coord.x() << "," << coord.y() << "," << coord.z();
	}
}

TEST_F(VoxelWorldFixture, InsertChunkInvalidatesItsNeighbors) {
	VoxelWorld world;
	world.setBlock(math::vec3i{8, 8, 8}, 1);
	world.setBlock(math::vec3i{8 + 16, 8 + 16, 8}, 1);// edge neighbour (1,1,0)
	world.setBlock(math::vec3i{8 + 48, 8, 8}, 1);// (3,0,0): not a neighbour of (1,0,0)
	const uint64_t own = world.getChunk(math::vec3i{0, 0, 0})->getRevision();
	const uint64_t edge = world.getChunk(math::vec3i{1, 1, 0})->getRevision();
	const uint64_t far = world.getChunk(math::vec3i{3, 0, 0})->getRevision();
	Chunk generated;
	generated.setBlock(0, 0, 0, 2);
	const auto installed = world.insertChunk(math::vec3i{1, 0, 0}, std::move(generated));
	EXPECT_EQ(installed->getCoord(), (math::vec3i{1, 0, 0}));
	EXPECT_EQ(world.getBlock(math::vec3i{16, 0, 0}), 2u);
	EXPECT_NE(world.getChunk(math::vec3i{0, 0, 0})->getRevision(), own);
	EXPECT_NE(world.getChunk(math::vec3i{1, 1, 0})->getRevision(), edge);
	EXPECT_EQ(world.getChunk(math::vec3i{3, 0, 0})->getRevision(), far);
}

TEST_F(VoxelWorldFixture, InsertAirChunkLeavesNeighborsAlone) {
	VoxelWorld world;
	world.setBlock(math::vec3i{8, 8, 8}, 1);
	const uint64_t own = world.getChunk(math::vec3i{0, 0, 0})->getRevision();
	world.insertChunk(math::vec3i{1, 0, 0}, Chunk{});
	EXPECT_EQ(world.getChunk(math::vec3i{0, 0, 0})->getRevision(), own);
	EXPECT_TRUE(world.hasChunk(math::vec3i{1, 0, 0}));
}
