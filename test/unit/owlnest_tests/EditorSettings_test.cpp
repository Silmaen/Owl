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
	settings.frameRateLimit = 144;
	settings.backgroundFrameRateLimit = 5000;
	settings.idleWaitMs = 0;
	settings.pushRecentProject("/games/demo/");
	settings.setProjectSession("/games/demo", {.documents = {"scenes/a.owl", "scripts/b.lua"},
											   .activeDocument = "scripts/b.lua",
											   .selectedEntity = 1234});
	settings.setProjectSession("/games/forgotten", {.documents = {"x.owl"}, .activeDocument = {}, .selectedEntity = 0});
	settings.saveToFile(dir / "settings.yml");

	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_EQ(loaded.autosaveIntervalSeconds, 120);
	EXPECT_EQ(loaded.frameRateLimit, 144);
	EXPECT_EQ(loaded.backgroundFrameRateLimit, 1000);
	EXPECT_EQ(loaded.idleWaitMs, 0);
	const auto session = loaded.getProjectSession("/games/demo/");
	ASSERT_TRUE(session.has_value());
	EXPECT_EQ(session->documents, (std::vector<std::string>{"scenes/a.owl", "scripts/b.lua"}));
	EXPECT_EQ(session->activeDocument, "scripts/b.lua");
	EXPECT_EQ(session->selectedEntity, 1234u);
	EXPECT_FALSE(loaded.getProjectSession("/games/forgotten").has_value());
	std::filesystem::remove_all(dir);
}

TEST(EditorSettings, ViewportDisplayOptionsRoundTrip) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_editor_settings_display_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	EXPECT_TRUE(settings.showGrid);
	EXPECT_TRUE(settings.showTriggerIcons);
	EXPECT_FALSE(settings.gizmoWorldSpace);
	EXPECT_TRUE(settings.showCameraPreview);
	settings.showCameraPreview = false;
	settings.showGrid = false;
	settings.editorAmbient = 0.6f;
	settings.showTriggerIcons = false;
	settings.gizmoWorldSpace = true;
	settings.playControlsCollapsed = true;
	settings.showFrameChip = false;
	settings.saveToFile(dir / "settings.yml");

	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_FALSE(loaded.showGrid);
	EXPECT_FLOAT_EQ(loaded.editorAmbient, 0.6f);
	EXPECT_FALSE(loaded.showTriggerIcons);
	EXPECT_TRUE(loaded.gizmoWorldSpace);
	EXPECT_TRUE(loaded.playControlsCollapsed);
	EXPECT_FALSE(loaded.showFrameChip);
	EXPECT_FALSE(loaded.showCameraPreview);
	std::filesystem::remove_all(dir);
}

TEST(EditorSettings, CameraNavigationRoundTripsAndClamps) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_editor_settings_camera_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	EXPECT_EQ(settings.cameraRotatePercent, 100);
	EXPECT_EQ(settings.cameraDeadZone, 0);
	EXPECT_FALSE(settings.cameraInvertY);
	settings.cameraRotatePercent = 150;
	settings.cameraPanPercent = 50;
	settings.cameraZoomPercent = 1000;// out of range: clamped when read back
	settings.cameraInvertY = true;
	settings.cameraDeadZone = 4;
	settings.saveToFile(dir / "settings.yml");

	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_EQ(loaded.cameraRotatePercent, 150);
	EXPECT_EQ(loaded.cameraPanPercent, 50);
	EXPECT_EQ(loaded.cameraZoomPercent, 300);
	EXPECT_TRUE(loaded.cameraInvertY);
	EXPECT_EQ(loaded.cameraDeadZone, 4);
	std::filesystem::remove_all(dir);
}

TEST(EditorSettings, McpServerIsOffByDefaultAndRoundTrips) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_editor_settings_mcp_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	EXPECT_FALSE(settings.mcpEnabled);
	EXPECT_EQ(settings.mcpPort, 47800);
	settings.saveToFile(dir / "settings.yml");
	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_FALSE(loaded.mcpEnabled);

	settings.mcpEnabled = true;
	settings.mcpPort = 80;// below 1024: clamped when read back
	settings.saveToFile(dir / "settings.yml");
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_TRUE(loaded.mcpEnabled);
	EXPECT_EQ(loaded.mcpPort, 1024);
	std::filesystem::remove_all(dir);
}

TEST(EditorSettings, RotationSnapStepRoundTripsAndClamps) {
	const auto dir = std::filesystem::temp_directory_path() / "owl_editor_settings_rotation_snap_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	EditorSettings settings;
	EXPECT_FLOAT_EQ(settings.snapRotationDegrees, 45.f);
	settings.snapRotationDegrees = 15.f;
	settings.saveToFile(dir / "settings.yml");
	EditorSettings loaded;
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_FLOAT_EQ(loaded.snapRotationDegrees, 15.f);

	settings.snapRotationDegrees = 720.f;// out of range: clamped when read back
	settings.saveToFile(dir / "settings.yml");
	loaded.loadFromFile(dir / "settings.yml");
	EXPECT_FLOAT_EQ(loaded.snapRotationDegrees, 180.f);
	std::filesystem::remove_all(dir);
}
