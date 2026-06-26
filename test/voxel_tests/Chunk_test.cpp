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

TEST_F(ChunkFixture, MetaDefaultsToZero) {
	const Chunk chunk;
	EXPECT_EQ(chunk.getMeta(0, 0, 0), g_DefaultMeta);
	EXPECT_EQ(chunk.metadata().size(), g_ChunkVolume);
}

TEST_F(ChunkFixture, SetBlockStoresMetaAndDirties) {
	Chunk chunk;
	const PackedMeta meta = packMeta({.orientation = BlockOrientation::AxisX, .state = 7});
	chunk.setBlock(1, 2, 3, 5, meta);
	EXPECT_EQ(chunk.getBlock(1, 2, 3), 5u);
	EXPECT_EQ(chunk.getMeta(1, 2, 3), meta);
	EXPECT_TRUE(chunk.isDirty());
}

TEST_F(ChunkFixture, SetMetaOnlyDirtiesOnChange) {
	Chunk chunk;
	chunk.setBlock(0, 0, 0, 4);
	chunk.markClean();
	chunk.setMeta(0, 0, 0, packMeta({.orientation = BlockOrientation::Yaw180, .state = 0}));
	EXPECT_TRUE(chunk.isDirty());
	EXPECT_EQ(unpackMeta(chunk.getMeta(0, 0, 0)).orientation, BlockOrientation::Yaw180);
	chunk.markClean();
	chunk.setMeta(0, 0, 0, packMeta({.orientation = BlockOrientation::Yaw180, .state = 0}));
	EXPECT_FALSE(chunk.isDirty());
}

TEST_F(ChunkFixture, FillResetsMeta) {
	Chunk chunk;
	chunk.setBlock(2, 2, 2, 9, packMeta({.orientation = BlockOrientation::AxisZ, .state = 3}));
	chunk.fill(1);
	EXPECT_EQ(chunk.getMeta(2, 2, 2), g_DefaultMeta);
}

TEST_F(ChunkFixture, EncodeDecodeCarriesMeta) {
	Chunk chunk;
	const PackedMeta a = packMeta({.orientation = BlockOrientation::AxisX, .state = 12});
	const PackedMeta b = packMeta({.orientation = BlockOrientation::YawCw90, .state = 0});
	chunk.setBlock(0, 0, 0, 3, a);
	chunk.setBlock(1, 0, 0, 3, b);
	chunk.setBlock(2, 0, 0, 3, g_DefaultMeta);
	const std::string encoded = chunk.encode();
	Chunk restored;
	ASSERT_TRUE(restored.decode(encoded));
	EXPECT_EQ(restored.getMeta(0, 0, 0), a);
	EXPECT_EQ(restored.getMeta(1, 0, 0), b);
	EXPECT_EQ(restored.getMeta(2, 0, 0), g_DefaultMeta);
	EXPECT_EQ(restored.metadata(), chunk.metadata());
}

TEST_F(ChunkFixture, LegacyEncodingDecodesToDefaultMeta) {
	// A legacy id-only run string (no ":meta" suffix) must still decode, with default metadata everywhere.
	Chunk chunk;
	const std::string legacy = std::to_string(g_ChunkVolume - 1) + "x0 1x5";
	ASSERT_TRUE(chunk.decode(legacy));
	EXPECT_EQ(chunk.getBlock(g_ChunkSize - 1, g_ChunkSize - 1, g_ChunkSize - 1), 5u);
	EXPECT_EQ(chunk.getMeta(g_ChunkSize - 1, g_ChunkSize - 1, g_ChunkSize - 1), g_DefaultMeta);
}

TEST_F(ChunkFixture, EncodeOmitsMetaSuffixWhenDefault) {
	// All-default metadata must produce byte-identical output to the legacy id-only format.
	Chunk chunk;
	chunk.setBlock(0, 0, 0, 7);
	EXPECT_EQ(chunk.encode().find(':'), std::string::npos);
}
