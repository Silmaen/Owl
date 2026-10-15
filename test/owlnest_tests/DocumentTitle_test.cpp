/**
 * @file DocumentTitle_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/CodeEditorDocument.h"
#include "document/Document.h"
#include "testHelper.h"

#include <gui/IconFont.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace owl;
using namespace owl::nest;

TEST(DocumentTitle, ShowsTheGlyphAndKeepsAnIdWithoutTheName) {
	const auto title = documentWindowTitle(DocumentType::Scene, "main_menu.owl", 0xab);
	EXPECT_TRUE(title.starts_with(gui::icon::clapperboard));
	EXPECT_NE(title.find("main_menu.owl"), std::string::npos);
	EXPECT_TRUE(title.ends_with("###scene_ab"));
	// Renaming keeps the part after ###, so the window keeps its dock.
	EXPECT_TRUE(documentWindowTitle(DocumentType::Scene, "level.owl*", 0xab).ends_with("###scene_ab"));
	EXPECT_NE(documentWindowTitle(DocumentType::Code, "", 1).find("Untitled"), std::string::npos);
	EXPECT_TRUE(documentWindowTitle(DocumentType::Tileset, "atlas", 2).starts_with(gui::icon::layoutGrid));
	EXPECT_TRUE(documentWindowTitle(DocumentType::ActionMap, "input_actions", 3).ends_with("###actionmap_3"));
}

TEST(DocumentTitle, FocusRequestLastsAFewFrames) {
	CodeEditorDocument document;
	// A new document asks for the focus, for a few frames so that its first dock does not leave it behind.
	for (uint8_t frame = 0; frame < Document::focusFrames; ++frame) EXPECT_TRUE(document.consumeFocusRequest());
	EXPECT_FALSE(document.consumeFocusRequest());
	document.requestFocus();
	EXPECT_TRUE(document.consumeFocusRequest());
}
