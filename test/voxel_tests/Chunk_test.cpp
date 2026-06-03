/**
 * @file Chunk_test.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/Chunk.h>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class ChunkFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};
}// namespace

TEST_F(ChunkFixture, DefaultIsAirAndClean) {
	const Chunk chunk;
	EXPECT_FALSE(chunk.isDirty());
	EXPECT_TRUE(chunk.isEmpty());
	EXPECT_EQ(chunk.getBlock(0, 0, 0), g_AirBlock);
	EXPECT_EQ(chunk.getBlock(15, 15, 15), g_AirBlock);
	EXPECT_EQ(chunk.blocks().size(), g_ChunkVolume);
}

TEST_F(ChunkFixture, SetBlockStoresAndDirties) {
	Chunk chunk;
	chunk.setBlock(1, 2, 3, 5);
	EXPECT_EQ(chunk.getBlock(1, 2, 3), 5u);
	EXPECT_TRUE(chunk.isDirty());
	EXPECT_FALSE(chunk.isEmpty());
}

TEST_F(ChunkFixture, SetSameValueDoesNotDirty) {
	Chunk chunk;
	chunk.setBlock(0, 0, 0, g_AirBlock);
	EXPECT_FALSE(chunk.isDirty());
}

TEST_F(ChunkFixture, OutOfRangeIgnored) {
	Chunk chunk;
	chunk.setBlock(-1, 0, 0, 7);
	chunk.setBlock(16, 0, 0, 7);
	chunk.setBlock(0, 0, 16, 7);
	EXPECT_FALSE(chunk.isDirty());
	EXPECT_EQ(chunk.getBlock(-1, 0, 0), g_AirBlock);
	EXPECT_EQ(chunk.getBlock(0, 16, 0), g_AirBlock);
}

TEST_F(ChunkFixture, FillThenClean) {
	Chunk chunk;
	chunk.fill(4);
	EXPECT_TRUE(chunk.isDirty());
	EXPECT_FALSE(chunk.isEmpty());
	EXPECT_EQ(chunk.getBlock(7, 7, 7), 4u);
	chunk.markClean();
	EXPECT_FALSE(chunk.isDirty());
}

TEST_F(ChunkFixture, CoordIsStored) {
	const Chunk chunk{math::vec3i{2, -3, 4}};
	EXPECT_EQ(chunk.getCoord().x(), 2);
	EXPECT_EQ(chunk.getCoord().y(), -3);
	EXPECT_EQ(chunk.getCoord().z(), 4);
}

TEST_F(ChunkFixture, LocalIndexLayout) {
	EXPECT_EQ(localIndex(0, 0, 0), 0u);
	EXPECT_EQ(localIndex(1, 0, 0), 1u);
	EXPECT_EQ(localIndex(0, 0, 1), g_ChunkSize);
	EXPECT_EQ(localIndex(0, 1, 0), g_ChunkSize * g_ChunkSize);
}

TEST_F(ChunkFixture, EncodeDecodeRoundTrip) {
	Chunk chunk;
	for (int32_t i = 0; i < static_cast<int32_t>(g_ChunkSize); ++i) chunk.setBlock(i, 0, 0, static_cast<BlockId>(i));
	chunk.setBlock(5, 5, 5, 99);
	const std::string encoded = chunk.encode();
	Chunk restored;
	ASSERT_TRUE(restored.decode(encoded));
	EXPECT_FALSE(restored.isDirty());
	for (int32_t i = 0; i < static_cast<int32_t>(g_ChunkSize); ++i)
		EXPECT_EQ(restored.getBlock(i, 0, 0), static_cast<BlockId>(i));
	EXPECT_EQ(restored.getBlock(5, 5, 5), 99u);
	EXPECT_EQ(restored.blocks(), chunk.blocks());
}

TEST_F(ChunkFixture, EncodeEmptyChunkIsSingleRun) {
	const Chunk chunk;
	EXPECT_EQ(chunk.encode(), std::to_string(g_ChunkVolume) + "x0");
}

TEST_F(ChunkFixture, DecodeRejectsShortRun) {
	Chunk chunk;
	EXPECT_FALSE(chunk.decode("10x1"));
}
