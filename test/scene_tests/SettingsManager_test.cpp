/**
 * @file SettingsManager_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/SettingsManager.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

using namespace owl;
using namespace owl::scene;

namespace {
struct SettingsGuard {
	SettingsGuard() {
		settings.setGameName("OwlSettingsTest_" +
							 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	}
	~SettingsGuard() {
		// Cleanup user settings file.
		const auto dir = settings.getUserDirectory();
		std::filesystem::remove_all(dir);
	}
	/// The settings under test (fresh for every test).
	SettingsManager settings;
};

}// namespace

TEST(SettingsManager, DefaultValues) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("speed", 8.0f);
	guard.settings.setDefault("lives", int64_t{3});

	const auto speed = guard.settings.get("speed");
	ASSERT_TRUE(speed.has_value());
	EXPECT_FLOAT_EQ(std::get<float>(speed.value()), 8.0f);

	const auto lives = guard.settings.getAs<int64_t>("lives");
	ASSERT_TRUE(lives.has_value());
	EXPECT_EQ(lives.value(), 3);

	EXPECT_FALSE(guard.settings.get("nonexistent").has_value());
	core::Log::invalidate();
}

TEST(SettingsManager, OverridesTakesPrecedence) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("volume", 1.0f);
	guard.settings.set("volume", 0.5f);

	const auto vol = guard.settings.getAs<float>("volume");
	ASSERT_TRUE(vol.has_value());
	EXPECT_FLOAT_EQ(vol.value(), 0.5f);

	EXPECT_TRUE(guard.settings.hasOverride("volume"));
	core::Log::invalidate();
}

TEST(SettingsManager, ResetToDefault) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("speed", 8.0f);
	guard.settings.set("speed", 12.0f);
	EXPECT_FLOAT_EQ(guard.settings.getAs<float>("speed").value(), 12.0f);

	guard.settings.resetToDefault("speed");
	EXPECT_FALSE(guard.settings.hasOverride("speed"));
	EXPECT_FLOAT_EQ(guard.settings.getAs<float>("speed").value(), 8.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, ResetAllToDefaults) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("a", int64_t{1});
	guard.settings.set("a", int64_t{99});
	guard.settings.set("b", 3.14f);

	guard.settings.resetAllToDefaults();
	EXPECT_FALSE(guard.settings.hasOverride("a"));
	EXPECT_FALSE(guard.settings.hasOverride("b"));
	EXPECT_EQ(guard.settings.getAs<int64_t>("a").value(), 1);
	EXPECT_FALSE(guard.settings.get("b").has_value());// no default for b
	core::Log::invalidate();
}

TEST(SettingsManager, SaveLoadRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("default_only", 1.0f);
	guard.settings.set("volume", 0.75f);
	guard.settings.set("name", std::string("test"));
	guard.settings.set("fullscreen", true);
	guard.settings.set("score", int64_t{42});

	EXPECT_TRUE(guard.settings.saveUserSettings());

	// Clear overrides and reload.
	guard.settings.resetAllToDefaults();
	EXPECT_FALSE(guard.settings.hasOverride("volume"));

	guard.settings.loadUserSettings();
	EXPECT_FLOAT_EQ(guard.settings.getAs<float>("volume").value(), 0.75f);
	EXPECT_EQ(guard.settings.getAs<std::string>("name").value(), "test");
	EXPECT_EQ(guard.settings.getAs<bool>("fullscreen").value(), true);
	EXPECT_EQ(guard.settings.getAs<int64_t>("score").value(), 42);

	// Default-only key should still be accessible.
	EXPECT_FLOAT_EQ(guard.settings.getAs<float>("default_only").value(), 1.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, GetWithFallback) {
	core::Log::init(core::Log::Level::Off);
	const SettingsGuard guard;

	const auto val = guard.settings.get("missing", 42.0f);
	EXPECT_FLOAT_EQ(std::get<float>(val), 42.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, HasAndKeys) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.setDefault("a", int64_t{1});
	guard.settings.set("b", 2.0f);

	EXPECT_TRUE(guard.settings.has("a"));
	EXPECT_TRUE(guard.settings.has("b"));
	EXPECT_FALSE(guard.settings.has("c"));

	const auto allKeys = guard.settings.keys();
	EXPECT_EQ(allKeys.size(), 2u);
	core::Log::invalidate();
}

TEST(SettingsManager, LoadDefaultsFromFile) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	const auto dir = std::filesystem::temp_directory_path() / "owl_settings_test";
	std::filesystem::create_directories(dir);
	const auto path = dir / "game_settings.yml";
	{
		std::ofstream f(path);
		f << "GameSettings:\n"
		  << "  - key: player_speed\n"
		  << "    type: float\n"
		  << "    value: 10.0\n"
		  << "  - key: max_lives\n"
		  << "    type: int\n"
		  << "    value: 5\n";
	}

	guard.settings.loadDefaults(path);
	EXPECT_FLOAT_EQ(guard.settings.getAs<float>("player_speed").value(), 10.0f);
	EXPECT_EQ(guard.settings.getAs<int64_t>("max_lives").value(), 5);

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(SettingsManager, LoadDefaultsNonexistentFile) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.loadDefaults("/nonexistent/path.yml");
	EXPECT_TRUE(guard.settings.keys().empty());
	core::Log::invalidate();
}

TEST(SettingsManager, TypedGetMismatchReturnsNullopt) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	guard.settings.set("val", int64_t{42});
	EXPECT_FALSE(guard.settings.getAs<float>("val").has_value());
	EXPECT_TRUE(guard.settings.getAs<int64_t>("val").has_value());
	core::Log::invalidate();
}

TEST(SettingsManager, EmptyGameNameFallback) {
	core::Log::init(core::Log::Level::Off);
	const SettingsManager settings;

	const auto dir = settings.getUserDirectory();
	EXPECT_NE(dir.string().find("OwlGame"), std::string::npos);

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
