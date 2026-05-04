/**
 * @file Tileset_test.cpp
 * @author Silmaen
 * @date 02/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Tileset.h>

using namespace owl;
using namespace owl::scene;

namespace {
class TilesetFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};
}// namespace

TEST_F(TilesetFixture, ResizeAllocatesPerTileMeta) {
	Tileset ts;
	ts.resize(4, 3);
	EXPECT_EQ(ts.columns, 4u);
	EXPECT_EQ(ts.rows, 3u);
	EXPECT_EQ(ts.tileCount(), 12u);
	EXPECT_EQ(ts.tiles.size(), 12u);
	for (const auto& meta: ts.tiles) {
		EXPECT_FALSE(meta.collidable);
		EXPECT_TRUE(meta.name.empty());
	}
}

TEST_F(TilesetFixture, ResizeClampsToOne) {
	Tileset ts;
	ts.resize(0, 0);
	EXPECT_EQ(ts.columns, 1u);
	EXPECT_EQ(ts.rows, 1u);
	EXPECT_EQ(ts.tiles.size(), 1u);
}

TEST_F(TilesetFixture, GetTileUvComputesCornerCoordinates) {
	Tileset ts;
	ts.resize(4, 2);// 4 columns × 2 rows, total 8 tiles
	// Tile size defaults to 32 px → atlas is 128×64 px → half-texel inset of
	// 1/256 horizontally and 1/128 vertically is added to each UV corner.
	const float kTol = 1e-2f;// loose tolerance covers the inset (≈ 0.004 / 0.008)
	// Tile 0 sits in the top-left of the atlas (row 0, col 0); UVs ≈ {[0, 0.5], [0.25, 1.0]}.
	const auto uv0 = ts.getTileUv(0);
	EXPECT_NEAR(uv0[0].x(), 0.0f, kTol);
	EXPECT_NEAR(uv0[0].y(), 0.5f, kTol);
	EXPECT_NEAR(uv0[1].x(), 0.25f, kTol);
	EXPECT_NEAR(uv0[2].x(), 0.25f, kTol);
	EXPECT_NEAR(uv0[2].y(), 1.0f, kTol);
	EXPECT_NEAR(uv0[3].x(), 0.0f, kTol);
	EXPECT_NEAR(uv0[3].y(), 1.0f, kTol);
	// Tile 7 sits at row 1, col 3 (bottom-right of atlas).
	const auto uv7 = ts.getTileUv(7);
	EXPECT_NEAR(uv7[0].x(), 0.75f, kTol);
	EXPECT_NEAR(uv7[0].y(), 0.0f, kTol);
	EXPECT_NEAR(uv7[2].x(), 1.0f, kTol);
	EXPECT_NEAR(uv7[2].y(), 0.5f, kTol);
}

TEST_F(TilesetFixture, GetTileUvAppliesHalfTexelInset) {
	Tileset ts;
	ts.resize(4, 2);
	ts.tileWidth = 32;
	ts.tileHeight = 32;
	const auto uv0 = ts.getTileUv(0);
	// Inset of 0.5 / 128 = 0.0039 on U, 0.5 / 64 = 0.0078 on V.
	EXPECT_GT(uv0[0].x(), 0.0f);
	EXPECT_LT(uv0[1].x(), 0.25f);
	EXPECT_GT(uv0[0].y(), 0.5f);
	EXPECT_LT(uv0[2].y(), 1.0f);
}

TEST_F(TilesetFixture, GetTileUvOutOfRangeReturnsFullAtlas) {
	Tileset ts;
	ts.resize(2, 2);
	const auto uv = ts.getTileUv(99);
	EXPECT_FLOAT_EQ(uv[0].x(), 0.0f);
	EXPECT_FLOAT_EQ(uv[0].y(), 0.0f);
	EXPECT_FLOAT_EQ(uv[2].x(), 1.0f);
	EXPECT_FLOAT_EQ(uv[2].y(), 1.0f);
}

TEST_F(TilesetFixture, IsCollidableReturnsFalseForOutOfRange) {
	Tileset ts;
	ts.resize(2, 2);
	ts.tiles[1].collidable = true;
	EXPECT_TRUE(ts.isCollidable(1));
	EXPECT_FALSE(ts.isCollidable(0));
	EXPECT_FALSE(ts.isCollidable(99));// out of range → default
}

TEST_F(TilesetFixture, RoundTripDefault) {
	Tileset ts;
	ts.resize(8, 4);
	const auto yaml = ts.serializeToString("default");

	Tileset restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_EQ(restored.columns, 8u);
	EXPECT_EQ(restored.rows, 4u);
	EXPECT_EQ(restored.tileWidth, 32u);
	EXPECT_EQ(restored.tileHeight, 32u);
	EXPECT_EQ(restored.tiles.size(), 32u);
	// All-default → no `tiles:` block in the YAML.
	EXPECT_EQ(yaml.find("tiles"), std::string::npos);
}

TEST_F(TilesetFixture, RoundTripWithCustomMetadata) {
	Tileset ts;
	ts.resize(4, 2);
	ts.tileWidth = 16;
	ts.tileHeight = 24;
	ts.tiles[0] = {.collidable = true, .name = "wall"};
	ts.tiles[3] = {.collidable = true, .name = ""};
	ts.tiles[6] = {.collidable = false, .name = "door"};

	const auto yaml = ts.serializeToString("dungeon");
	EXPECT_NE(yaml.find("tiles"), std::string::npos);
	EXPECT_NE(yaml.find("wall"), std::string::npos);
	EXPECT_NE(yaml.find("door"), std::string::npos);

	Tileset restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_EQ(restored.columns, 4u);
	EXPECT_EQ(restored.rows, 2u);
	EXPECT_EQ(restored.tileWidth, 16u);
	EXPECT_EQ(restored.tileHeight, 24u);
	ASSERT_EQ(restored.tiles.size(), 8u);
	EXPECT_TRUE(restored.tiles[0].collidable);
	EXPECT_EQ(restored.tiles[0].name, "wall");
	EXPECT_TRUE(restored.tiles[3].collidable);
	EXPECT_TRUE(restored.tiles[3].name.empty());
	EXPECT_FALSE(restored.tiles[6].collidable);
	EXPECT_EQ(restored.tiles[6].name, "door");
	// Untouched slots stay default.
	EXPECT_FALSE(restored.tiles[1].collidable);
	EXPECT_TRUE(restored.tiles[7].name.empty());
}

TEST_F(TilesetFixture, DeserializeRejectsMalformed) {
	Tileset ts;
	EXPECT_FALSE(ts.deserializeFromString(""));
	EXPECT_FALSE(ts.deserializeFromString("NotATileset:\n  foo: bar\n"));
}

TEST_F(TilesetFixture, DeserializeIgnoresOutOfRangeTileIndex) {
	const std::string yaml = R"(Tileset: bad
Version: 1
columns: 2
rows: 2
tiles:
  - { index: 99, collidable: true }
  - { index: 1, name: "ok" }
)";
	Tileset ts;
	ASSERT_TRUE(ts.deserializeFromString(yaml));
	EXPECT_EQ(ts.tiles.size(), 4u);
	EXPECT_FALSE(ts.tiles[0].collidable);
	EXPECT_EQ(ts.tiles[1].name, "ok");
}

TEST_F(TilesetFixture, FileRoundTrip) {
	Tileset ts;
	ts.resize(3, 3);
	ts.tiles[4] = {.collidable = true, .name = "centre"};

	const auto path = std::filesystem::temp_directory_path() / "tileset_test.owltileset";
	ASSERT_TRUE(ts.saveToFile(path, "centred"));

	Tileset restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.columns, 3u);
	EXPECT_EQ(restored.rows, 3u);
	EXPECT_TRUE(restored.tiles[4].collidable);
	EXPECT_EQ(restored.tiles[4].name, "centre");

	std::filesystem::remove(path);
}
