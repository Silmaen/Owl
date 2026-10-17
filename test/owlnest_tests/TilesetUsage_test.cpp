/**
 * @file TilesetUsage_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Tests of the lookup of the tilesets that use an image (tile grid overlay of the image viewer).
 */

#include "document/viewer/TilesetUsage.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest::viewer;

namespace {
const std::filesystem::path g_assets{"/project/assets"};
const std::array<std::filesystem::path, 1> g_directories{g_assets};

constexpr auto g_tileset = R"(Tileset: tiles
texture: nam:textures/ground.png
tileWidth: 16
tileHeight: 8
columns: 4
rows: 3
filterMode: Nearest
)";

class TilesetUsageTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }
};
}// namespace

TEST_F(TilesetUsageTest, matchesAssetRelativeAndAbsoluteReferences) {
	const auto image = g_assets / "textures" / "ground.png";
	EXPECT_TRUE(isTextureReference("nam:textures/ground.png", image, g_directories));
	EXPECT_TRUE(isTextureReference("nam:./textures/../textures/ground.png", image, g_directories));
	EXPECT_TRUE(isTextureReference("pat:/project/assets/textures/ground.png", image, g_directories));
	EXPECT_FALSE(isTextureReference("nam:textures/other.png", image, g_directories));
	EXPECT_FALSE(isTextureReference("emp:", image, g_directories));
	EXPECT_FALSE(isTextureReference("nam:textures/ground.png", "/elsewhere/ground.png", g_directories));
}

TEST_F(TilesetUsageTest, readsTheGridOfATilesetUsingTheImage) {
	const auto grid =
			parseTilesetGrid(g_tileset, "tiles.owltileset", g_assets / "textures" / "ground.png", g_directories);
	ASSERT_TRUE(grid.has_value());
	EXPECT_EQ(grid->tileset, "tiles.owltileset");
	EXPECT_EQ(grid->tileWidth, 16u);
	EXPECT_EQ(grid->tileHeight, 8u);
	EXPECT_EQ(grid->columns, 4u);
	EXPECT_EQ(grid->rows, 3u);
	EXPECT_TRUE(grid->nearest);
}

TEST_F(TilesetUsageTest, ignoresOtherImagesAndOtherDocuments) {
	EXPECT_FALSE(parseTilesetGrid(g_tileset, "t", g_assets / "textures" / "other.png", g_directories).has_value());
	EXPECT_FALSE(parseTilesetGrid("Tilemap: map\n", "t", g_assets / "a.png", g_directories).has_value());
	EXPECT_FALSE(parseTilesetGrid("a: [unclosed", "t", g_assets / "a.png", g_directories).has_value());
	EXPECT_FALSE(parseTilesetGrid("Tileset: t\n", "t", g_assets / "a.png", g_directories).has_value());
}

TEST_F(TilesetUsageTest, defaultsToLinearAndSizesAtLeastOne) {
	const auto grid = parseTilesetGrid("Tileset: t\ntexture: pat:/a/b.png\ntileWidth: 0\n", "t", "/a/b.png", {});
	ASSERT_TRUE(grid.has_value());
	EXPECT_FALSE(grid->nearest);
	EXPECT_EQ(grid->tileWidth, 1u);
	EXPECT_EQ(grid->columns, 1u);
}

TEST_F(TilesetUsageTest, scansAFolderTree) {
	const auto root = std::filesystem::temp_directory_path() / "owl_tileset_usage_test";
	std::filesystem::remove_all(root);
	std::filesystem::create_directories(root / "textures");
	std::filesystem::create_directories(root / "sets");
	std::ofstream{root / "sets" / "a.owltileset"} << "Tileset: a\ntexture: nam:textures/ground.png\ncolumns: 2\n";
	std::ofstream{root / "sets" / "b.owltileset"} << "Tileset: b\ntexture: nam:textures/other.png\n";
	const std::array<std::filesystem::path, 1> roots{root};
	const auto grids = findTilesetGrids(roots, root / "textures" / "ground.png", roots);
	ASSERT_EQ(grids.size(), 1u);
	EXPECT_EQ(grids[0].tileset, "a.owltileset");
	EXPECT_EQ(grids[0].columns, 2u);
	EXPECT_TRUE(findTilesetGrids(std::array<std::filesystem::path, 1>{root / "missing"}, "x.png", roots).empty());
	std::filesystem::remove_all(root);
}
