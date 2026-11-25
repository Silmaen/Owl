/**
 * @file PluginDocument_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/PluginDocument.h"
#include "document/Document.h"
#include "testHelper.h"

#include <core/Log.h>
#include <platform/FileUtils.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using namespace owl;
using namespace owl::nest;

TEST(PluginDocument, EditsTheTextOfItsFileThroughItsHistory) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_plugin_document";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto file = dir / "level.owlnote";
	std::ofstream{file} << "- [ ] First\n";

	PluginDocument document{"demo.notes.document"};
	EXPECT_EQ(document.type(), DocumentType::Plugin);
	EXPECT_EQ(document.getPluginType(), "demo.notes.document");
	EXPECT_EQ(document.title(), "Untitled");
	EXPECT_FALSE(document.save());
	EXPECT_FALSE(document.loadFromFile(dir / "missing.owlnote"));
	ASSERT_TRUE(document.loadFromFile(file));
	EXPECT_EQ(document.title(), "level.owlnote");
	EXPECT_EQ(document.filePath(), file);
	EXPECT_EQ(document.getText(), "- [ ] First\n");
	EXPECT_FALSE(document.isDirty());
	EXPECT_FALSE(document.canUndo());

	EXPECT_FALSE(document.setText("- [ ] First\n", "Nothing"));
	ASSERT_TRUE(document.setText("- [x] First\n", "Tick"));
	EXPECT_TRUE(document.isDirty());
	ASSERT_TRUE(document.canUndo());
	document.performUndo();
	EXPECT_EQ(document.getText(), "- [ ] First\n");
	EXPECT_FALSE(document.isDirty());
	ASSERT_TRUE(document.canRedo());
	document.performRedo();
	EXPECT_EQ(document.getText(), "- [x] First\n");

	ASSERT_TRUE(document.save());
	EXPECT_FALSE(document.isDirty());
	EXPECT_EQ(platform::fileToString(file), "- [x] First\n");
	EXPECT_EQ(document.recoverySnapshot().value_or(""), "- [x] First\n");
	ASSERT_TRUE(document.restoreRecoverySnapshot("- [ ] Recovered\n"));
	EXPECT_EQ(document.getText(), "- [ ] Recovered\n");
	EXPECT_TRUE(document.isDirty());
	EXPECT_FALSE(document.canUndo());
	EXPECT_TRUE(document.isOpen());
	document.setOpen(false);
	EXPECT_FALSE(document.isOpen());

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
