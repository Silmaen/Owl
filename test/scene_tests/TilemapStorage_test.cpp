/**
 * @file TilemapStorage_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/TilemapAsset.h>
#include <scene/TilemapCodec.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

using namespace owl;
using namespace owl::scene;
using component::TileCell;
using component::TilemapChunk;
using component::TilemapLayer;

namespace {

class TilemapStorage : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

// A 70 × 40 map of two layers touching several chunks, overlays and flags included.
auto makeMap() -> TilemapAsset {
	TilemapAsset map;
	map.width = 70;
	map.height = 40;
	map.cellSize = 0.5f;
	map.tilesetPath = "tilesets/terrain.owltileset";
	auto& ground = map.addLayer("ground");
	ground.fill(3);
	ground.setCell(65, 39, {.terrain = 7, .overlay = 12, .flags = 0x81});
	auto& roads = map.addLayer("roads");
	roads.visible = false;
	roads.parallax = math::vec2{0.5f, 0.25f};
	roads.setCell(40, 2, {.terrain = component::g_EmptyTileIndex, .overlay = 1000, .flags = 0});
	roads.setCell(1, 33, {.terrain = component::g_MaxTileIndex, .overlay = component::g_EmptyTileIndex, .flags = 4});
	return map;
}

auto byteAt(const std::string_view iBytes, const size_t iOffset) -> uint32_t {
	return static_cast<uint8_t>(iBytes[iOffset]);
}

auto u32At(const std::string_view iBytes, const size_t iOffset) -> uint32_t {
	return byteAt(iBytes, iOffset) | (byteAt(iBytes, iOffset + 1) << 8U) | (byteAt(iBytes, iOffset + 2) << 16U) |
		   (byteAt(iBytes, iOffset + 3) << 24U);
}

void putU32(std::string& ioBytes, const size_t iOffset, const uint32_t iValue) {
	for (size_t i = 0; i < 4; ++i) ioBytes[iOffset + i] = static_cast<char>((iValue >> (8U * i)) & 0xFFU);
}

// Rewrite the checksum of a payload after a deliberate change, so only the structural checks see it.
void reseal(std::string& ioPayload) {
	const auto body = std::string_view{ioPayload}.substr(0, ioPayload.size() - 4);
	putU32(ioPayload, ioPayload.size() - 4, codec::checksum(body));
}

auto withPayload(const std::string& iText, const std::string& iPayload) -> std::string {
	const auto parts = codec::splitFile(iText);
	return std::string{parts.header} + "...\n" + iPayload;
}

constexpr size_t g_headerBytes = codec::g_PayloadMagic.size() + 8;
constexpr size_t g_chunkBytes = 8 + TilemapChunk::cellCount * 5;

}// namespace

TEST_F(TilemapStorage, RoundTripKeepsEveryCellPlane) {
	const auto map = makeMap();
	const auto text = map.serializeToString("terrain");
	EXPECT_NE(text.find("FormatVersion: 2"), std::string::npos);
	EXPECT_NE(text.find("payloadBytes:"), std::string::npos);
	EXPECT_EQ(text.find("tiles:"), std::string::npos);
	const auto parts = codec::splitFile(text);
	ASSERT_TRUE(parts.hasPayload);
	EXPECT_EQ(parts.payload.substr(0, codec::g_PayloadMagic.size()), codec::g_PayloadMagic);

	TilemapAsset restored;
	ASSERT_TRUE(restored.deserializeFromString(text));
	EXPECT_TRUE(restored.hasSameContent(map));
	EXPECT_EQ(restored.layers[0].getCell(65, 39), (TileCell{.terrain = 7, .overlay = 12, .flags = 0x81}));
	EXPECT_EQ(restored.layers[0].getTile(0, 0), 3);
	EXPECT_EQ(restored.layers[1].getCell(40, 2).overlay, 1000);
	EXPECT_EQ(restored.layers[1].getCell(1, 33).terrain, component::g_MaxTileIndex);
	EXPECT_EQ(restored.layers[1].getCell(1, 33).flags, 4u);
	EXPECT_FALSE(restored.layers[1].visible);
	EXPECT_FLOAT_EQ(restored.layers[1].parallax.y(), 0.25f);
	EXPECT_EQ(restored.layers[1].getAllocatedChunkCount(), 2u);
	EXPECT_EQ(restored.serializeToString("terrain"), text);
}

TEST_F(TilemapStorage, FileRoundTripThroughDisk) {
	const auto map = makeMap();
	const auto path = std::filesystem::temp_directory_path() / "owl_tilemap_storage_test.owltilemap";
	ASSERT_TRUE(map.saveToFile(path));
	TilemapAsset restored;
	ASSERT_TRUE(restored.loadFromFile(path));
	EXPECT_TRUE(restored.hasSameContent(map));
	std::filesystem::remove(path);
}

TEST_F(TilemapStorage, PayloadIsLittleEndian) {
	TilemapAsset map;
	map.width = 1;
	map.height = 1;
	map.addLayer("one").setCell(0, 0, {.terrain = 0x0102, .overlay = 0x0304, .flags = 0x56});
	const auto text = map.serializeToString("one");
	const auto payload = codec::splitFile(text).payload;
	ASSERT_EQ(payload.size(), g_headerBytes + 4 + g_chunkBytes + 4);
	EXPECT_EQ(payload.substr(0, 8), "OWLTCHNK");
	EXPECT_EQ(std::string(payload.substr(8, 4)), (std::string{"\x20\x00\x00\x00", 4}));
	EXPECT_EQ(u32At(payload, 12), 1u);// layers
	EXPECT_EQ(u32At(payload, 16), 1u);// chunks of layer 0
	EXPECT_EQ(u32At(payload, 20), 0u);// chunk x
	EXPECT_EQ(u32At(payload, 24), 0u);// chunk y
	constexpr size_t terrain = 28;
	EXPECT_EQ(byteAt(payload, terrain), 0x02u);
	EXPECT_EQ(byteAt(payload, terrain + 1), 0x01u);
	EXPECT_EQ(byteAt(payload, terrain + 2), 0xFFu);
	EXPECT_EQ(byteAt(payload, terrain + 3), 0xFFu);
	constexpr size_t overlay = terrain + 2 * TilemapChunk::cellCount;
	EXPECT_EQ(byteAt(payload, overlay), 0x04u);
	EXPECT_EQ(byteAt(payload, overlay + 1), 0x03u);
	constexpr size_t flags = overlay + 2 * TilemapChunk::cellCount;
	EXPECT_EQ(byteAt(payload, flags), 0x56u);
	EXPECT_EQ(byteAt(payload, flags + 1), 0u);
	const auto body = payload.substr(0, payload.size() - 4);
	EXPECT_EQ(u32At(payload, payload.size() - 4), codec::checksum(body));
	EXPECT_EQ(codec::checksum("a"), 0xE40C292Cu);// FNV-1a 32 reference value
}

TEST_F(TilemapStorage, FormatOneMigratesAndIsWrittenAsFormatTwo) {
	const std::string legacy = R"(Tilemap: legacy
FormatVersion: 1
tilesetPath: tilesets/walls.owltileset
width: 3
height: 2
cellSize: 1
layers:
  - name: walls
    tiles: 3,3,-1,7,-1,3
  - name: hidden
    visible: false
)";
	TilemapAsset asset;
	ASSERT_TRUE(asset.deserializeFromString(legacy));
	ASSERT_EQ(asset.layers.size(), 2u);
	EXPECT_EQ(asset.getTile(0, 0, 0), 3);
	EXPECT_EQ(asset.getTile(0, 2, 0), component::g_EmptyTileIndex);
	EXPECT_EQ(asset.getTile(0, 0, 1), 7);
	EXPECT_EQ(asset.layers[1].getAllocatedChunkCount(), 0u);
	const auto rewritten = asset.serializeToString("legacy");
	EXPECT_NE(rewritten.find("FormatVersion: 2"), std::string::npos);
	EXPECT_EQ(rewritten.find("tiles:"), std::string::npos);
	EXPECT_EQ(rewritten.find("terrain:"), std::string::npos);
	TilemapAsset reloaded;
	ASSERT_TRUE(reloaded.deserializeFromString(rewritten));
	EXPECT_TRUE(reloaded.hasSameContent(asset));
}

TEST_F(TilemapStorage, TruncatedFileIsRefusedAndLeavesTheAssetUnchanged) {
	const auto text = makeMap().serializeToString("terrain");
	const auto separator = text.find(codec::g_PayloadSeparator);
	ASSERT_NE(separator, std::string::npos);
	const std::array cuts{separator - 20,
						  separator,
						  separator + codec::g_PayloadSeparator.size(),
						  separator + codec::g_PayloadSeparator.size() + 10,
						  text.size() / 2,
						  text.size() - 1};
	for (const size_t cut: cuts) {
		TilemapAsset target;
		target.width = 5;
		target.addLayer("kept");
		EXPECT_FALSE(target.deserializeFromString(std::string_view{text}.substr(0, cut))) << "cut at " << cut;
		EXPECT_EQ(target.width, 5u) << "cut at " << cut;
		ASSERT_EQ(target.layers.size(), 1u) << "cut at " << cut;
		EXPECT_EQ(target.layers[0].name, "kept") << "cut at " << cut;
	}
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(text + "x"));
}

TEST_F(TilemapStorage, CorruptedPayloadIsRefused) {
	const auto map = makeMap();
	const auto text = map.serializeToString("terrain");
	const std::string payload{codec::splitFile(text).payload};

	auto flipped = payload;
	flipped[g_headerBytes + 4 + 100] ^= 0x10;
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, flipped)));

	auto badMagic = payload;
	badMagic[0] = 'X';
	reseal(badMagic);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, badMagic)));

	auto badSide = payload;
	putU32(badSide, 8, 16);
	reseal(badSide);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, badSide)));

	auto badLayers = payload;
	putU32(badLayers, 12, 3);
	reseal(badLayers);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, badLayers)));

	auto outside = payload;
	putU32(outside, g_headerBytes + 4, 9);// x of the first chunk of layer 0: the grid has 3 columns
	reseal(outside);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, outside)));

	auto twice = payload;
	putU32(twice, g_headerBytes + 4 + g_chunkBytes, 0);// second chunk of layer 0 takes the coordinates of the first
	putU32(twice, g_headerBytes + 4 + g_chunkBytes + 4, 0);
	reseal(twice);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, twice)));

	auto tooMany = payload;
	putU32(tooMany, g_headerBytes, 1000);
	reseal(tooMany);
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(withPayload(text, tooMany)));

	auto noSize = text;
	noSize.replace(noSize.find("payloadBytes:"), 13, "payloadBytez:");
	EXPECT_FALSE(TilemapAsset{}.deserializeFromString(noSize));

	TilemapAsset intact;
	EXPECT_TRUE(intact.deserializeFromString(withPayload(text, payload)));
	EXPECT_TRUE(intact.hasSameContent(map));
}

TEST_F(TilemapStorage, StorageFollowsThePaintedAreaNotTheGrid) {
	TilemapLayer layer;
	layer.resize({4096, 4096});
	EXPECT_EQ(layer.getChunkGridSize(), (math::vec2ui{128, 128}));
	EXPECT_EQ(layer.getAllocatedChunkCount(), 0u);
	EXPECT_LT(layer.getStorageBytes(), 512u * 1024u);
	EXPECT_TRUE(layer.setTile(4000, 4001, 9));
	EXPECT_EQ(layer.getAllocatedChunkCount(), 1u);
	EXPECT_EQ(layer.getChunk({125, 125})->terrain[(4001 % 32) * 32 + 4000 % 32], 9u);
	EXPECT_EQ(layer.getChunk({0, 0}), nullptr);

	TilemapAsset map;
	map.width = 4096;
	map.height = 4096;
	map.layers.push_back(layer);
	const auto text = map.serializeToString("huge");
	EXPECT_LT(text.size(), 8u * 1024u);
	TilemapAsset restored;
	ASSERT_TRUE(restored.deserializeFromString(text));
	EXPECT_EQ(restored.getTile(0, 4000, 4001), 9);
	EXPECT_EQ(restored.layers[0].getAllocatedChunkCount(), 1u);
}

TEST_F(TilemapStorage, FullMapOf1024FitsTheBudget) {
	TilemapAsset map;
	map.width = 1024;
	map.height = 1024;
	map.addLayer("ground").fill(1);
	EXPECT_EQ(map.layers[0].getAllocatedChunkCount(), 1024u);
	EXPECT_LE(map.getStorageBytes(), 16u * 1024u * 1024u);
	TilemapAsset restored;
	ASSERT_TRUE(restored.deserializeFromString(map.serializeToString("big")));
	EXPECT_TRUE(restored.hasSameContent(map));
	EXPECT_EQ(restored.layers[0].getOccupiedCount(), 1024u * 1024u);
}

TEST_F(TilemapStorage, InvalidTileIndicesAreRefused) {
	TilemapLayer layer;
	layer.resize({2, 2});
	EXPECT_FALSE(layer.setTile(0, 0, component::g_MaxTileIndex + 1));
	EXPECT_FALSE(layer.setTile(0, 0, -2));
	EXPECT_FALSE(layer.setCell(0, 0, {.terrain = 1, .overlay = 70000, .flags = 0}));
	EXPECT_EQ(layer.getAllocatedChunkCount(), 0u);
	EXPECT_TRUE(layer.setTile(0, 0, component::g_MaxTileIndex));
	EXPECT_EQ(layer.getTile(0, 0), component::g_MaxTileIndex);
}

TEST_F(TilemapStorage, CopiesShareChunksUntilWritten) {
	TilemapLayer original;
	original.resize({64, 64});
	original.setTile(1, 1, 5);
	const TilemapLayer copy = original;
	EXPECT_EQ(copy.getChunkRef({0, 0}), original.getChunkRef({0, 0}));
	const auto kept = original.getChunkRef({0, 0});
	original.setTile(2, 2, 6);
	EXPECT_FALSE(original.getChunkRef({0, 0}) == kept);
	EXPECT_EQ(copy.getChunkRef({0, 0}), kept);
	EXPECT_EQ(copy.getTile(2, 2), component::g_EmptyTileIndex);
	EXPECT_EQ(kept.get()->terrain[2 * 32 + 2], TilemapChunk::emptyId);
	EXPECT_TRUE(original.hasSameContent(original));
	EXPECT_FALSE(copy.hasSameContent(original));

	const auto generation = original.getChunkGeneration({0, 0});
	EXPECT_TRUE(original.setChunkRef({0, 0}, kept));
	EXPECT_GT(original.getChunkGeneration({0, 0}), generation);
	EXPECT_TRUE(copy.hasSameContent(original));
	EXPECT_FALSE(original.setChunkRef({2, 0}, kept));
	EXPECT_TRUE(original.setChunkRef({1, 1}, {}));
	EXPECT_EQ(original.getChunk({1, 1}), nullptr);
}

TEST_F(TilemapStorage, ShrinkingClearsTheCellsLeftOutside) {
	TilemapLayer layer;
	layer.resize({40, 40});
	layer.fill(2);
	const TilemapLayer before = layer;
	layer.resize({10, 10});
	layer.resize({40, 40});
	EXPECT_EQ(layer.getTile(9, 9), 2);
	EXPECT_EQ(layer.getTile(20, 5), component::g_EmptyTileIndex);
	EXPECT_EQ(layer.getTile(5, 35), component::g_EmptyTileIndex);
	EXPECT_EQ(layer.getOccupiedCount(), 100u);
	EXPECT_EQ(before.getOccupiedCount(), 1600u);
	EXPECT_EQ(before.getTile(20, 5), 2);
}

TEST_F(TilemapStorage, SampleTilemapsAreFormatTwo) {
	const auto root = owl::test::getRootPath();
	const std::array files{
			root / "sample_project" / "tilemaps" / "world_map.owltilemap",
			root / "sample_project" / "tilemaps" / "platformer_house.owltilemap",
			root / "sample_project" / "tilemaps" / "raycast_demo.owltilemap",
			root / "engine_assets" / "project_templates" / "raycast" / "tilemaps" / "room.owltilemap",
			root / "engine_assets" / "project_templates" / "mixed" / "tilemaps" / "room.owltilemap",
	};
	for (const auto& file: files) {
		if (!std::filesystem::exists(file))
			continue;
		std::ifstream in(file, std::ios::binary);
		std::stringstream buffer;
		buffer << in.rdbuf();
		const auto text = buffer.str();
		EXPECT_NE(text.find("FormatVersion: 2"), std::string::npos) << file;
		TilemapAsset asset;
		ASSERT_TRUE(asset.deserializeFromString(text)) << file;
		EXPECT_GT(asset.layers.at(0).getOccupiedCount(), 0u) << file;
		EXPECT_EQ(asset.serializeToString(file.stem().string()), text) << file;
	}
}
