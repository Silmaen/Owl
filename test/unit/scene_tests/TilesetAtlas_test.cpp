/**
 * @file TilesetAtlas_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/TilesetAtlas.h>

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::scene;

namespace {
class TilesetAtlasTest : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

// A solid image of one colour.
auto solid(const math::vec2ui& iSize, const uint8_t iValue) -> std::vector<uint8_t> {
	return std::vector<uint8_t>(static_cast<size_t>(iSize.surface()) * 4, iValue);
}
}// namespace

TEST_F(TilesetAtlasTest, StartsTransparentOnItsGrid) {
	const TilesetAtlas atlas{{4, 2}, 3, 2};
	EXPECT_EQ(atlas.getSize(), (math::vec2ui{12, 4}));
	EXPECT_EQ(atlas.getTileCount(), 6u);
	EXPECT_EQ(atlas.getPixels().size(), 12u * 4u * 4u);
	for (uint32_t index = 0; index < atlas.getTileCount(); ++index) EXPECT_TRUE(atlas.isTileEmpty(index));
	const TilesetAtlas clamped{{0, 0}, 0, 0};
	EXPECT_EQ(clamped.getTileCount(), 1u);
}

TEST_F(TilesetAtlasTest, SetTileScalesTheImageToTheCell) {
	TilesetAtlas atlas{{4, 4}, 2, 1};
	// A 2x2 image: top-left white, the rest black, opaque.
	std::vector<uint8_t> image(16, 0);
	for (size_t texel = 0; texel < 4; ++texel) image[texel * 4 + 3] = 255;
	image[0] = image[1] = image[2] = 255;
	ASSERT_TRUE(atlas.setTile(1, image, {2, 2}));
	const auto tile = atlas.getTile(1);
	ASSERT_EQ(tile.size(), 64u);
	EXPECT_EQ(tile[0], 255);
	EXPECT_EQ(tile[(1 * 4 + 1) * 4], 255);
	EXPECT_EQ(tile[(2 * 4 + 2) * 4], 0);
	EXPECT_EQ(tile[(2 * 4 + 2) * 4 + 3], 255);
	EXPECT_TRUE(atlas.isTileEmpty(0));
	EXPECT_FALSE(atlas.isTileEmpty(1));
	EXPECT_FALSE(atlas.setTile(2, image, {2, 2}));
	EXPECT_FALSE(atlas.setTile(0, image, {4, 4}));
}

TEST_F(TilesetAtlasTest, ResizeKeepsEachCellAtItsColumnAndRow) {
	TilesetAtlas atlas{{2, 2}, 2, 2};
	ASSERT_TRUE(atlas.setTile(2, solid({2, 2}, 200), {2, 2}));
	atlas.resize(3, 3);
	EXPECT_EQ(atlas.getTileCount(), 9u);
	EXPECT_FALSE(atlas.isTileEmpty(3));
	EXPECT_TRUE(atlas.isTileEmpty(2));
	atlas.resize(1, 2);
	EXPECT_FALSE(atlas.isTileEmpty(1));
	EXPECT_EQ(atlas.getSize(), (math::vec2ui{2, 4}));
}

TEST_F(TilesetAtlasTest, ClearAndSwapTiles) {
	TilesetAtlas atlas{{2, 2}, 2, 1};
	ASSERT_TRUE(atlas.setTile(0, solid({2, 2}, 90), {2, 2}));
	atlas.swapTiles(0, 1);
	EXPECT_TRUE(atlas.isTileEmpty(0));
	EXPECT_EQ(atlas.getTile(1), solid({2, 2}, 90));
	atlas.clearTile(1);
	EXPECT_TRUE(atlas.isTileEmpty(1));
	atlas.swapTiles(0, 5);
	EXPECT_TRUE(atlas.getTile(5).empty());
}

TEST_F(TilesetAtlasTest, InsertionFillsEmptySlotsThenAddsRows) {
	TilesetAtlas atlas{{1, 1}, 3, 1};
	ASSERT_TRUE(atlas.setTile(1, solid({1, 1}, 255), {1, 1}));
	const auto [slots, rows] = atlas.planInsertion(0, 4);
	EXPECT_EQ(slots, (std::vector<uint32_t>{0, 2, 3, 4}));
	EXPECT_EQ(rows, 2u);
	const auto [onFilled, sameRows] = atlas.planInsertion(1, 1);
	EXPECT_EQ(onFilled, std::vector<uint32_t>{1});
	EXPECT_EQ(sameRows, 1u);
}

TEST_F(TilesetAtlasTest, FromImageCropsAndPadsAndFlipsRows) {
	std::vector<uint8_t> image(3 * 2 * 4, 0);
	for (size_t i = 0; i < 12; ++i) image[i] = 255;
	const auto atlas = TilesetAtlas::fromImage(image, {3, 2}, {2, 2}, 1, 2);
	EXPECT_EQ(atlas.getSize(), (math::vec2ui{2, 4}));
	EXPECT_EQ(atlas.getPixels()[0], 255);
	EXPECT_TRUE(atlas.isTileEmpty(1));
	const auto flipped = TilesetAtlas::flipRows(image, {3, 2});
	EXPECT_EQ(flipped[0], 0);
	EXPECT_EQ(flipped[12], 255);
	EXPECT_EQ(TilesetAtlas::flipRows(flipped, {3, 2}), image);
	const auto empty = TilesetAtlas::fromImage(std::vector<uint8_t>(4, 0), {3, 2}, {2, 2}, 1, 1);
	EXPECT_TRUE(empty.isTileEmpty(0));
}
