/**
 * @file TilemapAsset_test.cpp
 * @author Silmaen
 * @date 08/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/TilemapAsset.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

using namespace owl;
using namespace owl::scene;

namespace {
// Row-major cell `iIndex` of a layer.
auto tileAt(const component::TilemapLayer& iLayer, const uint32_t iIndex) -> int32_t {
	return iLayer.getTile(iIndex % iLayer.getSize().x(), iIndex / iLayer.getSize().x());
}

class TilemapAssetFixture : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};
}// namespace

TEST_F(TilemapAssetFixture, FileExtensionIsOwltilemap) { EXPECT_STREQ(TilemapAsset::fileExtension(), ".owltilemap"); }

TEST_F(TilemapAssetFixture, AddLayerInitializesGrid) {
	TilemapAsset asset;
	asset.width = 4;
	asset.height = 3;
	auto& layer = asset.addLayer("ground");
	EXPECT_EQ(layer.name, "ground");
	EXPECT_TRUE(layer.visible);
	EXPECT_FLOAT_EQ(layer.parallax.x(), 1.f);
	EXPECT_FLOAT_EQ(layer.parallax.y(), 1.f);
	EXPECT_EQ(static_cast<size_t>(layer.getSize().x()) * layer.getSize().y(), 12u);
	EXPECT_EQ(layer.getOccupiedCount(), 0u);
	EXPECT_EQ(layer.getAllocatedChunkCount(), 0u);
}

TEST_F(TilemapAssetFixture, ResizeClampsToOne) {
	TilemapAsset asset;
	asset.addLayer("a");
	asset.resize(0, 0);
	EXPECT_EQ(asset.width, 1u);
	EXPECT_EQ(asset.height, 1u);
	ASSERT_EQ(asset.layers.size(), 1u);
	EXPECT_EQ(static_cast<size_t>(asset.layers[0].getSize().x()) * asset.layers[0].getSize().y(), 1u);
}

TEST_F(TilemapAssetFixture, ResizePreservesExistingCells) {
	TilemapAsset asset;
	asset.width = 3;
	asset.height = 2;
	auto& layer = asset.addLayer("a");
	// Encode a recognisable pattern: row-major 0..5
	for (uint32_t i = 0; i < 6; ++i) layer.setTile(i % 3, i / 3, static_cast<int32_t>(i));
	// Grow to 4x3 — original cells must stay at their (x,y) positions, new ones empty.
	asset.resize(4, 3);
	EXPECT_EQ(asset.width, 4u);
	EXPECT_EQ(asset.height, 3u);
	ASSERT_EQ(static_cast<size_t>(asset.layers[0].getSize().x()) * asset.layers[0].getSize().y(), 12u);
	EXPECT_EQ(asset.getTile(0, 0, 0), 0);
	EXPECT_EQ(asset.getTile(0, 1, 0), 1);
	EXPECT_EQ(asset.getTile(0, 2, 0), 2);
	EXPECT_EQ(asset.getTile(0, 0, 1), 3);
	EXPECT_EQ(asset.getTile(0, 2, 1), 5);
	// New column / row are empty.
	EXPECT_EQ(asset.getTile(0, 3, 0), scene::component::g_EmptyTileIndex);
	EXPECT_EQ(asset.getTile(0, 0, 2), scene::component::g_EmptyTileIndex);
}

TEST_F(TilemapAssetFixture, GetSetTileBoundsHandling) {
	TilemapAsset asset;
	asset.width = 2;
	asset.height = 2;
	asset.addLayer("a");
	asset.setTile(0, 0, 0, 7);
	EXPECT_EQ(asset.getTile(0, 0, 0), 7);
	// Out-of-grid set is silently ignored.
	asset.setTile(0, 99, 99, 9);
	EXPECT_EQ(asset.getTile(0, 99, 99), scene::component::g_EmptyTileIndex);
	// Out-of-range layer ignored.
	asset.setTile(5, 1, 1, 3);
	EXPECT_EQ(asset.getTile(5, 1, 1), scene::component::g_EmptyTileIndex);
}

TEST_F(TilemapAssetFixture, RoundTripDefaults) {
	TilemapAsset asset;
	asset.width = 8;
	asset.height = 4;
	asset.cellSize = 2.f;
	asset.tilesetPath = "tilesets/dungeon.owltileset";
	asset.addLayer("ground");

	const auto yaml = asset.serializeToString("dungeon_l1");
	EXPECT_NE(yaml.find("Tilemap: dungeon_l1"), std::string::npos);
	EXPECT_NE(yaml.find("tilesetPath"), std::string::npos);

	TilemapAsset restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	EXPECT_EQ(restored.width, 8u);
	EXPECT_EQ(restored.height, 4u);
	EXPECT_FLOAT_EQ(restored.cellSize, 2.f);
	EXPECT_EQ(restored.tilesetPath.generic_string(), "tilesets/dungeon.owltileset");
	ASSERT_EQ(restored.layers.size(), 1u);
	EXPECT_EQ(restored.layers[0].name, "ground");
	EXPECT_TRUE(restored.layers[0].visible);
	EXPECT_EQ(static_cast<size_t>(restored.layers[0].getSize().x()) * restored.layers[0].getSize().y(), 32u);
}

TEST_F(TilemapAssetFixture, RoundTripWithCustomLayers) {
	TilemapAsset asset;
	asset.width = 2;
	asset.height = 2;
	asset.cellSize = 1.f;
	auto& bg = asset.addLayer("bg");
	bg.visible = false;
	bg.parallax = math::vec2{0.5f, 0.25f};
	bg.assign({2, 2}, std::array{0, 1, 2, 3});
	auto& fg = asset.addLayer("fg");
	fg.assign({2, 2}, std::array{-1, 5, -1, 7});

	const auto yaml = asset.serializeToString("twolayer");
	EXPECT_NE(yaml.find("visible: false"), std::string::npos);
	EXPECT_NE(yaml.find("parallax"), std::string::npos);

	TilemapAsset restored;
	ASSERT_TRUE(restored.deserializeFromString(yaml));
	ASSERT_EQ(restored.layers.size(), 2u);
	EXPECT_EQ(restored.layers[0].name, "bg");
	EXPECT_FALSE(restored.layers[0].visible);
	EXPECT_FLOAT_EQ(restored.layers[0].parallax.x(), 0.5f);
	EXPECT_FLOAT_EQ(restored.layers[0].parallax.y(), 0.25f);
	EXPECT_EQ(static_cast<size_t>(restored.layers[0].getSize().x()) * restored.layers[0].getSize().y(), 4u);
	EXPECT_EQ(tileAt(restored.layers[0], 0), 0);
	EXPECT_EQ(tileAt(restored.layers[0], 3), 3);
	EXPECT_EQ(restored.layers[1].name, "fg");
	EXPECT_TRUE(restored.layers[1].visible);
	EXPECT_EQ(tileAt(restored.layers[1], 1), 5);
	EXPECT_EQ(tileAt(restored.layers[1], 3), 7);
}

TEST_F(TilemapAssetFixture, DeserializeMalformedYamlFails) {
	TilemapAsset asset;
	EXPECT_FALSE(asset.deserializeFromString(""));
	EXPECT_FALSE(asset.deserializeFromString("NotATilemap:\n  foo: bar\n"));
	// Unbalanced YAML triggers the parser exception path.
	EXPECT_FALSE(asset.deserializeFromString("Tilemap: x\nlayers: [unterminated"));
}

TEST_F(TilemapAssetFixture, DeserializeTruncatesOversizedTileBuffer) {
	const std::string yaml = R"(Tilemap: bad
Version: 1
width: 2
height: 2
layers:
  - { name: a, tiles: "1,2,3,4,5,6,7,8" }
)";
	TilemapAsset asset;
	ASSERT_TRUE(asset.deserializeFromString(yaml));
	ASSERT_EQ(asset.layers.size(), 1u);
	EXPECT_EQ(static_cast<size_t>(asset.layers[0].getSize().x()) * asset.layers[0].getSize().y(), 4u);
	EXPECT_EQ(tileAt(asset.layers[0], 0), 1);
	EXPECT_EQ(tileAt(asset.layers[0], 3), 4);
}

TEST_F(TilemapAssetFixture, DeserializePadsShortTileBuffer) {
	const std::string yaml = R"(Tilemap: short
Version: 1
width: 3
height: 2
layers:
  - { name: a, tiles: "1,2" }
)";
	TilemapAsset asset;
	ASSERT_TRUE(asset.deserializeFromString(yaml));
	ASSERT_EQ(asset.layers.size(), 1u);
	EXPECT_EQ(static_cast<size_t>(asset.layers[0].getSize().x()) * asset.layers[0].getSize().y(), 6u);
	EXPECT_EQ(tileAt(asset.layers[0], 0), 1);
	EXPECT_EQ(tileAt(asset.layers[0], 1), 2);
	EXPECT_EQ(tileAt(asset.layers[0], 2), scene::component::g_EmptyTileIndex);
}

TEST_F(TilemapAssetFixture, FileRoundTrip) {
	TilemapAsset asset;
	asset.width = 4;
	asset.height = 3;
	asset.cellSize = 0.5f;
	auto& layer = asset.addLayer("level");
	layer.setTile(1, 1, 42);

	const auto path = std::filesystem::temp_directory_path() / "tilemap_asset_test.owltilemap";
	ASSERT_TRUE(asset.saveToFile(path, "level1"));

	TilemapAsset restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_EQ(restored.width, 4u);
	EXPECT_EQ(restored.height, 3u);
	EXPECT_FLOAT_EQ(restored.cellSize, 0.5f);
	ASSERT_EQ(restored.layers.size(), 1u);
	EXPECT_EQ(tileAt(restored.layers[0], 5), 42);

	std::filesystem::remove(path);
}

TEST_F(TilemapAssetFixture, LoadFromMissingFileFails) {
	TilemapAsset asset;
	EXPECT_FALSE(asset.loadFromFile(std::filesystem::temp_directory_path() / "owl_no_tilemap_xyz.owltilemap"));
}

TEST_F(TilemapAssetFixture, SaveToUnopenablePathFails) {
	TilemapAsset asset;
	asset.addLayer("a");
	const auto bogus = std::filesystem::temp_directory_path() / "no_such_owl_subdir/x.owltilemap";
	std::filesystem::remove_all(std::filesystem::temp_directory_path() / "no_such_owl_subdir");
	EXPECT_FALSE(asset.saveToFile(bogus, "x"));
}

TEST_F(TilemapAssetFixture, LoadsMigratedSampleTilemaps) {
	// Smoke-test the YAML produced by `tools/migrate_inline_tilemaps.py` (PyYAML
	// safe_dump output with unquoted comma-separated tile strings) — every sample
	// scene tilemap must round-trip through `loadFromFile` without falling back
	// to the empty-buffer path.
	const auto root = owl::test::getRootPath();
	const auto tilemapsDir = root / "engine_demo" / "tilemaps";
	if (!std::filesystem::exists(tilemapsDir))
		GTEST_SKIP() << "engine_demo tilemaps directory not present";

	struct Sample {
		const char* file;
		uint32_t width;
		uint32_t height;
	};
	const std::array samples{
			Sample{"world_map.owltilemap", 48u, 24u},
			Sample{"platformer_house.owltilemap", 0u, 0u},
			Sample{"raycast_demo.owltilemap", 64u, 64u},
	};
	for (const auto& s: samples) {
		const auto path = tilemapsDir / s.file;
		if (!std::filesystem::exists(path))
			continue;// tolerate partial migrations during development
		TilemapAsset asset;
		ASSERT_TRUE(asset.loadFromFile(path)) << s.file;
		ASSERT_FALSE(asset.layers.empty()) << s.file;
		EXPECT_EQ(static_cast<size_t>(asset.layers[0].getSize().x()) * asset.layers[0].getSize().y(),
				  static_cast<size_t>(asset.width) * asset.height)
				<< s.file;
		if (s.width != 0u) {
			EXPECT_EQ(asset.width, s.width) << s.file;
		}
		if (s.height != 0u) {
			EXPECT_EQ(asset.height, s.height) << s.file;
		}
	}
}

TEST(TilemapLayer, ChunkGridCoversPartialChunks) {
	component::TilemapLayer layer;
	EXPECT_EQ(layer.getChunkGridSize(), (math::vec2ui{0, 0}));
	EXPECT_EQ(layer.getGeneration(), 0u);
	layer.resize({33, 64});
	EXPECT_EQ(layer.getSize(), (math::vec2ui{33, 64}));
	EXPECT_EQ(layer.getChunkGridSize(), (math::vec2ui{2, 2}));
	EXPECT_NE(layer.getChunkGeneration({1, 1}), 0u);
	EXPECT_EQ(layer.getChunkGeneration({2, 0}), 0u);
	EXPECT_EQ(layer.getTile(40, 0), component::g_EmptyTileIndex);
}

TEST(TilemapLayer, SetTileStampsOnlyItsChunk) {
	component::TilemapLayer layer;
	layer.resize({64, 64});
	const auto before00 = layer.getChunkGeneration({0, 0});
	const auto before11 = layer.getChunkGeneration({1, 1});
	EXPECT_TRUE(layer.setTile(40, 33, 7));
	EXPECT_EQ(layer.getTile(40, 33), 7);
	EXPECT_EQ(layer.getChunkGeneration({0, 0}), before00);
	EXPECT_GT(layer.getChunkGeneration({1, 1}), before11);
	EXPECT_EQ(layer.getGeneration(), layer.getChunkGeneration({1, 1}));
	const auto stamped = layer.getGeneration();
	EXPECT_FALSE(layer.setTile(40, 33, 7));
	EXPECT_FALSE(layer.setTile(64, 0, 7));
	EXPECT_EQ(layer.getGeneration(), stamped);
	EXPECT_EQ(layer.getOccupiedCount(), 1u);
}

TEST(TilemapLayer, GenerationsAreUniqueAcrossLayers) {
	component::TilemapLayer first;
	component::TilemapLayer second;
	first.resize({4, 4});
	second.resize({4, 4});
	EXPECT_NE(first.getGeneration(), second.getGeneration());
	auto copy = first;
	EXPECT_EQ(copy.getChunkGeneration({0, 0}), first.getChunkGeneration({0, 0}));
	copy.setTile(0, 0, 1);
	EXPECT_NE(copy.getChunkGeneration({0, 0}), first.getChunkGeneration({0, 0}));
	EXPECT_EQ(first.getTile(0, 0), component::g_EmptyTileIndex);
}

TEST(TilemapLayer, MoveLeavesAnEmptyGrid) {
	component::TilemapLayer source;
	source.name = "ground";
	source.assign({2, 2}, std::array{1, 2, 3});
	EXPECT_EQ(source.getTile(1, 1), component::g_EmptyTileIndex);
	const auto generation = source.getGeneration();
	component::TilemapLayer moved{std::move(source)};
	EXPECT_EQ(moved.getTile(0, 1), 3);
	EXPECT_EQ(moved.getGeneration(), generation);
	// NOLINTBEGIN(bugprone-use-after-move,clang-analyzer-cplusplus.Move): checks the moved-from state.
	EXPECT_EQ(source.getSize(), (math::vec2ui{0, 0}));
	EXPECT_EQ(source.getAllocatedChunkCount(), 0u);
	// NOLINTEND(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
	component::TilemapLayer assigned;
	assigned = std::move(moved);
	EXPECT_EQ(assigned.name, "ground");
	EXPECT_EQ(assigned.getTile(0, 0), 1);
	assigned.fill(9);
	EXPECT_EQ(assigned.getOccupiedCount(), 4u);
}
