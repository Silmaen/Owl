/**
 * @file EditorSettings_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSettings.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::nest;

TEST(ProjectSession, StoresProjectFilesRelativeAndOthersAbsolute) {
	const std::filesystem::path project{"/games/demo"};
	EXPECT_EQ(ProjectSession::toStored(project, "/games/demo/scenes/a.owl"), "scenes/a.owl");
	EXPECT_EQ(ProjectSession::toStored(project, "/elsewhere/notes.md"), "/elsewhere/notes.md");
	EXPECT_EQ(ProjectSession::resolve(project, "scenes/a.owl"), std::filesystem::path{"/games/demo/scenes/a.owl"});
	EXPECT_EQ(ProjectSession::resolve(project, "/elsewhere/notes.md"), std::filesystem::path{"/elsewhere/notes.md"});
}

TEST(EditorSettings, ProjectSessionRoundTripsThroughTheFile) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_editor_settings_session_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	settings.autosaveIntervalSeconds = 120;
	settings.pushRecentProject("/games/demo/");
	settings.setProjectSession("/games/demo", {.documents = {"scenes/a.owl", "scripts/b.lua"},
											   .activeDocument = "scripts/b.lua",
											   .selectedEntity = 1234});
	settings.setProjectSession("/games/forgotten", {.documents = {"x.owl"}, .activeDocument = {}, .selectedEntity = 0});
	settings.saveToFile(dir / "settings.yml");

	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_EQ(loaded.autosaveIntervalSeconds, 120);
	const auto session = loaded.getProjectSession("/games/demo/");
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(session->documents, (std::vector<std::string>{"scenes/a.owl", "scripts/b.lua"}));
	EXPECT_EQ(session->activeDocument, "scripts/b.lua");
	EXPECT_EQ(session->selectedEntity, 1234u);
	EXPECT_FALSE(loaded.getProjectSession("/games/forgotten").has_value());
	std::filesystem::remove_all(dir);
}
