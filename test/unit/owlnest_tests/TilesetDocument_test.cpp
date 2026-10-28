/**
 * @file TilesetDocument_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Headless tests of the atlas composition of the tileset document: images dropped on slots, folders, clear, move,
 * grid resize, undo and the atlas written on save. Drawing needs an ImGui frame and stays a manual check.
 */

#include "document/TilesetDocument.h"

#include <core/Log.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/RenderCommand.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::nest;

namespace {
class TilesetDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		renderer::gpu::RenderCommand::create(renderer::gpu::RenderAPI::Type::Null);
		m_root = std::filesystem::temp_directory_path() / "owl_tileset_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root / "images");
		for (const auto& [name, value]: {std::pair{"a.png", 50}, std::pair{"b.png", 100}, std::pair{"c.png", 150}})
			ASSERT_TRUE(renderer::writeImagePng(m_root / "images" / name, {2, 2},
												std::vector<uint8_t>(16, static_cast<uint8_t>(value))));
		m_document.onAttach(nullptr);
		m_document.tileset().tileWidth = 2;
		m_document.tileset().tileHeight = 2;
		m_document.tileset().resize(2, 1);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		renderer::gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}

	[[nodiscard]] auto saved() -> bool { return m_document.saveAs(m_root / "blocks.owltileset"); }

	std::filesystem::path m_root;
	TilesetDocument m_document;
};
}// namespace

TEST_F(TilesetDocumentTest, AnUnsavedTilesetWithoutTextureCannotCompose) {
	EXPECT_FALSE(m_document.setTileImage(0, m_root / "images" / "a.png"));
	EXPECT_FALSE(m_document.getAtlas().has_value());
}

TEST_F(TilesetDocumentTest, ADroppedImageFillsTheSlotAndIsWrittenOnSave) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(1, m_root / "images" / "b.png"));
	EXPECT_EQ(m_document.getAtlasFile(), m_root / "blocks.png");
	ASSERT_NE(m_document.tileset().texture, nullptr);
	EXPECT_TRUE(m_document.isDirty());
	EXPECT_FALSE(m_document.getAtlas()->isTileEmpty(1));
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	const auto written = renderer::decodeImageFile(m_root / "blocks.png", 4);
	ASSERT_TRUE(written.valid);
	EXPECT_EQ(written.size, (math::vec2ui{4, 2}));
	EXPECT_EQ(written.pixels[2 * 4], 100);
	EXPECT_EQ(written.pixels[0], 0);
}

TEST_F(TilesetDocumentTest, UndoAndRedoAComposition) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(0, m_root / "images" / "a.png"));
	m_document.performUndo();
	EXPECT_TRUE(m_document.getAtlas()->isTileEmpty(0));
	m_document.performRedo();
	EXPECT_FALSE(m_document.getAtlas()->isTileEmpty(0));
}

TEST_F(TilesetDocumentTest, AFolderFillsEmptySlotsAndAddsRows) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(1, m_root / "images" / "c.png"));
	ASSERT_TRUE(m_document.insertTileImages(0, {m_root / "images"}));
	EXPECT_EQ(m_document.tileset().rows, 2u);
	EXPECT_EQ(m_document.tileset().tiles.size(), 4u);
	const auto& atlas = *m_document.getAtlas();
	EXPECT_EQ(atlas.getRows(), 2u);
	EXPECT_EQ(atlas.getTile(0)[0], 50);
	EXPECT_EQ(atlas.getTile(1)[0], 150);
	EXPECT_EQ(atlas.getTile(2)[0], 100);
	EXPECT_EQ(atlas.getTile(3)[0], 150);
	m_document.performUndo();
	EXPECT_EQ(m_document.tileset().rows, 1u);
	EXPECT_EQ(m_document.getAtlas()->getRows(), 1u);
	EXPECT_FALSE(m_document.insertTileImages(0, {m_root / "missing"}));
}

TEST_F(TilesetDocumentTest, ClearKeepsTheIndexAndResetsTheMetadata) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(0, m_root / "images" / "a.png"));
	m_document.tileset().tiles[0].collidable = true;
	ASSERT_TRUE(m_document.clearTiles({0}));
	EXPECT_TRUE(m_document.getAtlas()->isTileEmpty(0));
	EXPECT_FALSE(m_document.tileset().tiles[0].collidable);
	EXPECT_EQ(m_document.tileset().tileCount(), 2u);
}

TEST_F(TilesetDocumentTest, SelectedTilesMoveWithTheirMetadata) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(0, m_root / "images" / "a.png"));
	m_document.tileset().tiles[0].name = "grass";
	m_document.selectTile(0);
	ASSERT_TRUE(m_document.moveTiles(m_document.getSelectedTiles(), 1));
	EXPECT_TRUE(m_document.getAtlas()->isTileEmpty(0));
	EXPECT_EQ(m_document.getAtlas()->getTile(1)[0], 50);
	EXPECT_EQ(m_document.tileset().tiles[1].name, "grass");
	EXPECT_EQ(m_document.getSelectedTiles(), std::vector<uint32_t>{1});
	EXPECT_FALSE(m_document.moveTiles({1}, 1));
	m_document.performUndo();
	EXPECT_EQ(m_document.tileset().tiles[0].name, "grass");
	EXPECT_FALSE(m_document.getAtlas()->isTileEmpty(0));
}

TEST_F(TilesetDocumentTest, ResizingTheGridKeepsTheTiles) {
	ASSERT_TRUE(saved());
	ASSERT_TRUE(m_document.setTileImage(1, m_root / "images" / "b.png"));
	m_document.resizeGrid(3, 2);
	EXPECT_EQ(m_document.tileset().columns, 3u);
	EXPECT_EQ(m_document.getAtlas()->getSize(), (math::vec2ui{6, 4}));
	EXPECT_EQ(m_document.getAtlas()->getTile(1)[0], 100);
	m_document.resizeGrid(1, 1);
	EXPECT_TRUE(m_document.getAtlas()->isTileEmpty(0));
	m_document.performUndo();
	EXPECT_EQ(m_document.getAtlas()->getTile(1)[0], 100);
}

TEST_F(TilesetDocumentTest, SelectionTogglesAndRanges) {
	m_document.tileset().resize(4, 1);
	m_document.selectTile(0);
	m_document.selectTile(2, true);
	EXPECT_EQ(m_document.getSelectedTiles(), (std::vector<uint32_t>{0, 2}));
	m_document.selectTile(2, true);
	EXPECT_EQ(m_document.getSelectedTiles(), std::vector<uint32_t>{0});
	m_document.selectTile(3, false, true);
	EXPECT_EQ(m_document.getSelectedTiles(), (std::vector<uint32_t>{0, 1, 2, 3}));
}
