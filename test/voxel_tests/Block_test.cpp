/**
 * @file Block_test.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/voxel/Block.h>

using namespace owl;
using namespace owl::data::voxel;

namespace {
class BlockFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

auto makeStone() -> BlockType {
	BlockType stone;
	stone.name = "stone";
	stone.renderKind = BlockRenderKind::Opaque;
	stone.solid = true;
	stone.setAllFaces(3);
	return stone;
}

auto makeWater() -> BlockType {
	BlockType water;
	water.name = "water";
	water.renderKind = BlockRenderKind::Water;
	water.solid = false;
	water.setAllFaces(9);
	return water;
}
}// namespace

TEST_F(BlockFixture, AirRegisteredByDefault) {
	const BlockRegistry registry;
	EXPECT_EQ(registry.count(), 1u);
	EXPECT_TRUE(registry.isAir(g_AirBlock));
	EXPECT_FALSE(registry.isSolid(g_AirBlock));
	EXPECT_FALSE(registry.isOpaque(g_AirBlock));
	EXPECT_EQ(registry.get(g_AirBlock).renderKind, BlockRenderKind::Air);
}

TEST_F(BlockFixture, RegisterAssignsSequentialIds) {
	BlockRegistry registry;
	const BlockId stone = registry.registerBlock(makeStone());
	const BlockId water = registry.registerBlock(makeWater());
	EXPECT_EQ(stone, 1u);
	EXPECT_EQ(water, 2u);
	EXPECT_EQ(registry.count(), 3u);
}

TEST_F(BlockFixture, GetOutOfRangeReturnsAir) {
	const BlockRegistry registry;
	EXPECT_EQ(registry.get(42).renderKind, BlockRenderKind::Air);
	EXPECT_TRUE(registry.isAir(42));
}

TEST_F(BlockFixture, FindByNameResolvesIds) {
	BlockRegistry registry;
	registry.registerBlock(makeStone());
	registry.registerBlock(makeWater());
	ASSERT_TRUE(registry.findByName("stone").has_value());
	EXPECT_EQ(registry.findByName("stone").value(), 1u);
	EXPECT_EQ(registry.findByName("water").value(), 2u);
	EXPECT_FALSE(registry.findByName("lava").has_value());
}

TEST_F(BlockFixture, FlagSemantics) {
	BlockRegistry registry;
	const BlockId stone = registry.registerBlock(makeStone());
	const BlockId water = registry.registerBlock(makeWater());
	EXPECT_TRUE(registry.isOpaque(stone));
	EXPECT_TRUE(registry.isSolid(stone));
	EXPECT_FALSE(registry.isOpaque(water));
	EXPECT_FALSE(registry.isSolid(water));
}

TEST_F(BlockFixture, FaceTextureAccessors) {
	BlockType type;
	type.setAllFaces(7);
	EXPECT_EQ(type.faceTexture(BlockFace::XNeg), 7u);
	EXPECT_EQ(type.faceTexture(BlockFace::YPos), 7u);
	type.faceTextures[static_cast<size_t>(BlockFace::YPos)] = 4;
	EXPECT_EQ(type.faceTexture(BlockFace::YPos), 4u);
	EXPECT_EQ(type.faceTexture(BlockFace::XNeg), 7u);
}

TEST_F(BlockFixture, SerializeRoundTrip) {
	BlockRegistry registry;
	registry.registerBlock(makeStone());
	registry.registerBlock(makeWater());
	const std::string yaml = registry.serializeToString("test");
	BlockRegistry restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	ASSERT_EQ(restored.count(), registry.count());
	EXPECT_EQ(restored.get(1).name, "stone");
	EXPECT_EQ(restored.get(1).renderKind, BlockRenderKind::Opaque);
	EXPECT_TRUE(restored.get(1).solid);
	EXPECT_EQ(restored.get(1).faceTexture(BlockFace::XNeg), 3u);
	EXPECT_EQ(restored.get(2).name, "water");
	EXPECT_EQ(restored.get(2).renderKind, BlockRenderKind::Water);
	EXPECT_FALSE(restored.get(2).solid);
	EXPECT_EQ(restored.get(2).faceTexture(BlockFace::ZPos), 9u);
}

TEST_F(BlockFixture, DeserializeRejectsMalformed) {
	BlockRegistry registry;
	registry.registerBlock(makeStone());
	EXPECT_FALSE(registry.deserializeFromString("not: a registry"));
	EXPECT_EQ(registry.count(), 2u);
}
