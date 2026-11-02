/**
 * @file LogLevelSetting_test.cpp
 * @author Silmaen
 * @date 02/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSettings.h"
#include "panel/LogPanel.h"
#include "testHelper.h"

#include <core/Log.h>

#include <gtest/gtest.h>

#include <filesystem>

using namespace owl;

TEST(LogLevelSetting, AnEmptySettingFollowsTheProgramDefault) {
	EXPECT_EQ(nest::panel::getLogLevelSetting(""), core::Log::getDefaultLevel());
	EXPECT_EQ(nest::panel::getLogLevelSetting("trace"), core::Log::Level::Trace);
	EXPECT_EQ(nest::panel::getLogLevelSetting("warning"), core::Log::Level::Warning);
	EXPECT_EQ(nest::panel::getLogLevelSetting("bogus"), core::Log::getDefaultLevel());
}

TEST(LogLevelSetting, TheEditorSettingsKeepTheLevel) {
	const auto file = std::filesystem::temp_directory_path() / "log_level_settings.yml";
	nest::EditorSettings saved;
	saved.logLevel = "debug";
	saved.saveToFile(file);
	nest::EditorSettings loaded;
	loaded.loadFromFile(file);
	EXPECT_EQ(loaded.logLevel, "debug");
	std::filesystem::remove(file);
}
