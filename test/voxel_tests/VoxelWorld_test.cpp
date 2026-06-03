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
