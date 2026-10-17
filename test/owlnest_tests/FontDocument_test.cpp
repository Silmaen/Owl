/**
 * @file FontDocument_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Headless tests of the font document: charset edits, undo, recovery and the `.owlfont` sidecar it writes.
 */

#include "document/FontDocument.h"
#include "AssetKind.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <set>
#include <string>

using namespace owl;
using namespace owl::nest;
using data::fonts::CodePointRange;
using data::fonts::FontSettings;

namespace {

class FontDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_root = std::filesystem::temp_directory_path() / "owl_font_document_test";
		std::filesystem::remove_all(m_root);
		std::filesystem::create_directories(m_root);
		m_font = m_root / "DocFont.ttf";
		std::filesystem::copy_file(
				test::getRootPath() / "engine_assets" / "fonts" / "opensans" / "OpenSans-Regular.ttf", m_font);
		m_document.onAttach(nullptr);
	}

	void TearDown() override {
		m_document.onDetach();
		std::filesystem::remove_all(m_root);
		core::Log::invalidate();
	}

	std::filesystem::path m_root;
	std::filesystem::path m_font;
	FontDocument m_document;
};

}// namespace

TEST_F(FontDocumentTest, opensFontAndSidecar) {
	EXPECT_EQ(classifyAsset("a.ttf"), AssetKind::Font);
	EXPECT_EQ(classifyAsset("a.otf"), AssetKind::Font);
	EXPECT_EQ(classifyAsset("a.owlfont"), AssetKind::Font);
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.ttf"));
	EXPECT_FALSE(m_document.loadFromFile(m_root / "missing.owlfont"));
	ASSERT_TRUE(m_document.loadFromFile(m_root / "DocFont.owlfont"));
	EXPECT_EQ(m_document.filePath(), m_font);
	EXPECT_EQ(m_document.title(), "DocFont");
	EXPECT_EQ(m_document.type(), DocumentType::Font);
	EXPECT_TRUE(m_document.settings().extraRanges.empty());
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_TRUE(m_document.overridesGlobalPanels());
	EXPECT_FALSE(FontDocument::rangePresets().empty());
}

TEST_F(FontDocumentTest, editsAreUndoable) {
	ASSERT_TRUE(m_document.loadFromFile(m_font));
	m_document.addRange({.first = 0x04FF, .last = 0x0400});
	ASSERT_EQ(m_document.settings().extraRanges.size(), 1u);
	EXPECT_EQ(m_document.settings().extraRanges[0], (CodePointRange{.first = 0x0400, .last = 0x04FF}));
	EXPECT_TRUE(m_document.isDirty());
	m_document.addRange({.first = 0x0370, .last = 0x03FF});
	m_document.setRange(1, {.first = 0x0380, .last = 0x03FF});
	m_document.setRange(1, {.first = 0x0390, .last = 0x03FF});
	m_document.setRange(5, {.first = 0x0390, .last = 0x03FF});
	EXPECT_EQ(m_document.settings().extraRanges[1].first, 0x0390u);
	// The two bound edits merged into one step.
	m_document.performUndo();
	EXPECT_EQ(m_document.settings().extraRanges[1].first, 0x0370u);
	m_document.performRedo();
	EXPECT_EQ(m_document.settings().extraRanges[1].first, 0x0390u);
	m_document.removeRanges({0, 1, 7});
	EXPECT_TRUE(m_document.settings().extraRanges.empty());
	m_document.performUndo();
	EXPECT_EQ(m_document.settings().extraRanges.size(), 2u);
	m_document.performUndo();
	m_document.performUndo();
	m_document.performUndo();
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.canUndo());
	EXPECT_TRUE(m_document.canRedo());
}

TEST_F(FontDocumentTest, saveWritesAndRemovesTheSidecar) {
	ASSERT_TRUE(m_document.loadFromFile(m_font));
	const auto sidecar = FontSettings::sidecarPath(m_font);
	m_document.addRange({.first = 0x0400, .last = 0x04FF});
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(m_document.isDirty());
	ASSERT_TRUE(exists(sidecar));
	EXPECT_EQ(FontSettings::loadForFont(m_font), m_document.settings());

	FontDocument reopened;
	ASSERT_TRUE(reopened.loadFromFile(sidecar));
	EXPECT_EQ(reopened.settings(), m_document.settings());

	m_document.removeRanges({0});
	ASSERT_TRUE(m_document.save());
	EXPECT_FALSE(exists(sidecar));
}

TEST_F(FontDocumentTest, recoverySnapshotRoundTrip) {
	ASSERT_TRUE(m_document.loadFromFile(m_font));
	m_document.addRange({.first = 0x2190, .last = 0x21FF});
	const auto snapshot = m_document.recoverySnapshot();
	ASSERT_TRUE(snapshot.has_value());
	FontDocument restored;
	ASSERT_TRUE(restored.loadFromFile(m_font));
	ASSERT_TRUE(restored.restoreRecoverySnapshot(*snapshot));
	EXPECT_EQ(restored.settings(), m_document.settings());
	EXPECT_TRUE(restored.isDirty());
	EXPECT_FALSE(restored.restoreRecoverySnapshot("garbage: ["));
}

TEST_F(FontDocumentTest, summarizesTheCharsetWithTheExtraRanges) {
	ASSERT_TRUE(m_document.loadFromFile(m_font));
	const auto base = m_document.charsetSummary();
	EXPECT_GT(base.codePoints, 0u);
	m_document.addRange({.first = 0x0400, .last = 0x04FF});
	const auto summary = m_document.charsetSummary();
	EXPECT_EQ(summary.codePoints, base.codePoints + 256);
	m_document.performUndo();
	EXPECT_EQ(m_document.charsetSummary().codePoints, base.codePoints);
}

TEST_F(FontDocumentTest, samplesAreViewOptionsNotEdits) {
	ASSERT_TRUE(m_document.loadFromFile(m_font));
	EXPECT_EQ(FontDocument::sampleSizes().size(), 5u);
	EXPECT_FLOAT_EQ(FontDocument::sampleSizes().front(), 12.f);
	EXPECT_FLOAT_EQ(FontDocument::sampleSizes().back(), 64.f);
	m_document.setSampleText("Hello");
	m_document.setOutlineShown(true);
	m_document.setBoxShown(true);
	EXPECT_EQ(m_document.sampleText(), "Hello");
	EXPECT_TRUE(m_document.isOutlineShown());
	EXPECT_TRUE(m_document.isBoxShown());
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.canUndo());
}
