/**
 * @file ImageDocument_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Headless tests of the image viewer document: file information, read-only behaviour, tileset overlay choice.
 * Drawing needs a GPU and an ImGui frame and stays a manual check.
 */

#include "document/ImageDocument.h"

#include <core/Log.h>
#include <renderer/TextureDecoder.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace owl;
using namespace owl::nest;

namespace {
class ImageDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_image_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_image = m_root / "ground.png";
		const std::vector<uint8_t> pixels(8 * 4 * 4, 128);
		ASSERT_TRUE(renderer::writeImagePng(m_image, {8, 4}, pixels));
		m_document.onAttach(nullptr);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_image;
	ImageDocument m_document;
};
}// namespace

TEST_F(ImageDocumentTest, readsTheImageInformation) {
	ASSERT_TRUE(m_document.loadFromFile(m_image));
	EXPECT_EQ(m_document.type(), DocumentType::Image);
	EXPECT_EQ(m_document.title(), "ground.png");
	EXPECT_EQ(m_document.filePath(), m_image);
	EXPECT_EQ(m_document.info().size.x(), 8u);
	EXPECT_EQ(m_document.info().size.y(), 4u);
	EXPECT_EQ(m_document.info().channels, 4u);
	EXPECT_EQ(m_document.info().fileBytes, std::filesystem::file_size(m_image));
	EXPECT_TRUE(m_document.overridesGlobalPanels());
}

TEST_F(ImageDocumentTest, refusesAFileThatIsNotAnImage) {
	std::ofstream{m_root / "fake.png"} << "not an image";
	EXPECT_FALSE(m_document.loadFromFile(m_root / "fake.png"));
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.png"));
}

TEST_F(ImageDocumentTest, isReadOnly) {
	ASSERT_TRUE(m_document.loadFromFile(m_image));
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.canUndo());
	EXPECT_FALSE(m_document.save());
	EXPECT_FALSE(m_document.saveAs(m_root / "copy.png"));
	EXPECT_FALSE(m_document.recoverySnapshot().has_value());
	EXPECT_FALSE(std::filesystem::exists(m_root / "copy.png"));
}

TEST_F(ImageDocumentTest, listsTheTilesetsUsingTheImage) {
	ASSERT_TRUE(m_document.loadFromFile(m_image));
	EXPECT_TRUE(m_document.tilesets().empty());
	std::ofstream{m_root / "a.owltileset"} << "Tileset: a\ntexture: nam:ground.png\ntileWidth: 4\ntileHeight: 4\n"
											  "columns: 2\nrows: 1\n";
	std::ofstream{m_root / "b.owltileset"} << "Tileset: b\ntexture: nam:other.png\n";
	const std::array<std::filesystem::path, 1> roots{m_root};
	m_document.findTilesets(roots, roots);
	ASSERT_EQ(m_document.tilesets().size(), 1u);
	EXPECT_EQ(m_document.tilesets()[0].tileset, "a.owltileset");
	EXPECT_FALSE(m_document.overlay().has_value());
	m_document.setOverlay(0);
	EXPECT_EQ(m_document.overlay(), 0u);
	m_document.setOverlay(3);
	EXPECT_FALSE(m_document.overlay().has_value());
}
