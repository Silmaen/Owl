/**
 * @file TilemapCommands_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "commands/TilemapCommands.h"
#include "UndoManager.h"
#include "nestTestHelper.h"
#include "testHelper.h"

#include <cstdint>
#include <utility>

using namespace owl;
using namespace owl::nest;
using namespace owl::nest::commands;
using namespace owl::nest::test;
using scene::component::g_EmptyTileIndex;
using scene::component::TilemapChunk;

namespace {

class TilemapCommandsTest : public NestTest {
protected:
	void SetUp() override {
		NestTest::SetUp();
		m_asset.width = 256;
		m_asset.height = 256;
		m_asset.addLayer("ground").fill(1);
		m_asset.addLayer("decor");
	}

	scene::TilemapAsset m_asset;
	UndoManager<scene::TilemapAsset> m_tilemapUndo;
};

}// namespace

TEST_F(TilemapCommandsTest, OneCellEditRecordsOneChunk) {
	TilemapStroke stroke;
	EXPECT_TRUE(stroke.setTile(m_asset, 0, 100, 100, 5));
	auto command = stroke.finish(m_asset, "Paint tiles");
	ASSERT_NE(command, nullptr);
	ASSERT_EQ(command->getRecords().size(), 1u);
	EXPECT_EQ(command->getRecords()[0].chunk, (math::vec2ui{3, 3}));
	EXPECT_EQ(command->getStoredBytes(), sizeof(TilemapChunk));
	EXPECT_TRUE(stroke.isEmpty());

	m_tilemapUndo.push(std::move(command));
	m_tilemapUndo.undo(m_asset);
	EXPECT_EQ(m_asset.getTile(0, 100, 100), 1);
	m_tilemapUndo.redo(m_asset);
	EXPECT_EQ(m_asset.getTile(0, 100, 100), 5);
	m_tilemapUndo.undo(m_asset);
	EXPECT_EQ(m_asset.getTile(0, 100, 100), 1);
}

TEST_F(TilemapCommandsTest, StrokeRecordsEachChunkOnce) {
	TilemapStroke stroke;
	for (uint32_t x = 0; x < 64; ++x) EXPECT_TRUE(stroke.setTile(m_asset, 1, x, 10, 7));
	EXPECT_TRUE(stroke.setCell(m_asset, 1, 200, 200, {.terrain = 2, .overlay = 9, .flags = 3}));
	EXPECT_FALSE(stroke.setTile(m_asset, 1, 0, 10, 7));
	EXPECT_FALSE(stroke.setTile(m_asset, 5, 0, 0, 7));
	auto command = stroke.finish(m_asset, "Paint tiles");
	ASSERT_NE(command, nullptr);
	EXPECT_EQ(command->getRecords().size(), 3u);
	EXPECT_EQ(command->getStoredBytes(), 0u);// the decor chunks were empty before the stroke

	const scene::TilemapAsset painted = m_asset;
	m_tilemapUndo.push(std::move(command));
	m_tilemapUndo.undo(m_asset);
	EXPECT_EQ(m_asset.layers[1].getAllocatedChunkCount(), 0u);
	EXPECT_EQ(m_asset.getTile(1, 63, 10), g_EmptyTileIndex);
	m_tilemapUndo.redo(m_asset);
	EXPECT_TRUE(m_asset.hasSameContent(painted));
	EXPECT_EQ(m_asset.layers[1].getCell(200, 200).overlay, 9);
}

TEST_F(TilemapCommandsTest, StrokeBackToTheStartIsNotAnEdit) {
	TilemapStroke stroke;
	EXPECT_TRUE(stroke.setTile(m_asset, 0, 3, 3, 8));
	EXPECT_TRUE(stroke.setTile(m_asset, 0, 3, 3, 1));
	EXPECT_EQ(stroke.finish(m_asset, "Paint tiles"), nullptr);
	EXPECT_EQ(stroke.finish(m_asset, "Paint tiles"), nullptr);
}

TEST_F(TilemapCommandsTest, UndoRestoresTheSavedChunks) {
	const scene::TilemapAsset saved = m_asset;
	TilemapStroke stroke;
	EXPECT_TRUE(stroke.setTile(m_asset, 0, 0, 0, 4));
	m_tilemapUndo.push(stroke.finish(m_asset, "Paint tiles"));
	EXPECT_FALSE(m_asset.hasSameContent(saved));
	m_tilemapUndo.undo(m_asset);
	EXPECT_EQ(m_asset.layers[0].getChunkRef({0, 0}), saved.layers[0].getChunkRef({0, 0}));
	EXPECT_TRUE(m_asset.hasSameContent(saved));
}

TEST_F(TilemapCommandsTest, LayoutCommandSharesTheCells) {
	scene::TilemapAsset before = m_asset;
	m_asset.resize(48, 64);
	m_asset.layers.erase(m_asset.layers.begin() + 1);
	EXPECT_EQ(before.layers[0].getChunkRef({0, 0}), m_asset.layers[0].getChunkRef({0, 0}));
	EXPECT_FALSE(before.layers[0].getChunkRef({1, 0}) == m_asset.layers[0].getChunkRef({1, 0}));
	m_tilemapUndo.push(mkUniq<ModifyTilemapLayoutCommand>(std::move(before), m_asset, "Resize grid"));
	m_tilemapUndo.undo(m_asset);
	EXPECT_EQ(m_asset.width, 256u);
	ASSERT_EQ(m_asset.layers.size(), 2u);
	EXPECT_EQ(m_asset.getTile(0, 200, 200), 1);
	EXPECT_EQ(m_asset.getTile(0, 40, 10), 1);
	m_tilemapUndo.redo(m_asset);
	EXPECT_EQ(m_asset.width, 48u);
	EXPECT_EQ(m_asset.layers.size(), 1u);
	EXPECT_EQ(m_asset.getTile(0, 40, 10), 1);
	EXPECT_EQ(m_asset.getTile(0, 200, 200), g_EmptyTileIndex);
	EXPECT_EQ(m_tilemapUndo.undoDescription(), "Resize grid");
}
