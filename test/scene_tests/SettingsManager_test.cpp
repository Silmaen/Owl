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

using namespace owl;
using namespace owl::scene;

namespace {

struct SettingsGuard {
	SettingsGuard() {
		SettingsManager::clear();
		SettingsManager::setGameName("OwlSettingsTest_" +
									 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	}
	~SettingsGuard() {
		// Cleanup user settings file.
		const auto dir = SettingsManager::getUserDirectory();
		SettingsManager::clear();
		std::filesystem::remove_all(dir);
	}
};

}// namespace

TEST(SettingsManager, DefaultValues) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("speed", 8.0f);
	SettingsManager::setDefault("lives", int64_t{3});

	const auto speed = SettingsManager::get("speed");
	ASSERT_TRUE(speed.has_value());
	EXPECT_FLOAT_EQ(std::get<float>(speed.value()), 8.0f);

	const auto lives = SettingsManager::getAs<int64_t>("lives");
	ASSERT_TRUE(lives.has_value());
	EXPECT_EQ(lives.value(), 3);

	EXPECT_FALSE(SettingsManager::get("nonexistent").has_value());
	core::Log::invalidate();
}

TEST(SettingsManager, OverridesTakesPrecedence) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("volume", 1.0f);
	SettingsManager::set("volume", 0.5f);

	const auto vol = SettingsManager::getAs<float>("volume");
	ASSERT_TRUE(vol.has_value());
	EXPECT_FLOAT_EQ(vol.value(), 0.5f);

	EXPECT_TRUE(SettingsManager::hasOverride("volume"));
	core::Log::invalidate();
}

TEST(SettingsManager, ResetToDefault) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("speed", 8.0f);
	SettingsManager::set("speed", 12.0f);
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("speed").value(), 12.0f);

	SettingsManager::resetToDefault("speed");
	EXPECT_FALSE(SettingsManager::hasOverride("speed"));
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("speed").value(), 8.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, ResetAllToDefaults) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("a", int64_t{1});
	SettingsManager::set("a", int64_t{99});
	SettingsManager::set("b", 3.14f);

	SettingsManager::resetAllToDefaults();
	EXPECT_FALSE(SettingsManager::hasOverride("a"));
	EXPECT_FALSE(SettingsManager::hasOverride("b"));
	EXPECT_EQ(SettingsManager::getAs<int64_t>("a").value(), 1);
	EXPECT_FALSE(SettingsManager::get("b").has_value());// no default for b
	core::Log::invalidate();
}

TEST(SettingsManager, SaveLoadRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("default_only", 1.0f);
	SettingsManager::set("volume", 0.75f);
	SettingsManager::set("name", std::string("test"));
	SettingsManager::set("fullscreen", true);
	SettingsManager::set("score", int64_t{42});

	SettingsManager::saveUserSettings();

	// Clear overrides and reload.
	SettingsManager::resetAllToDefaults();
	EXPECT_FALSE(SettingsManager::hasOverride("volume"));

	SettingsManager::loadUserSettings();
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("volume").value(), 0.75f);
	EXPECT_EQ(SettingsManager::getAs<std::string>("name").value(), "test");
	EXPECT_EQ(SettingsManager::getAs<bool>("fullscreen").value(), true);
	EXPECT_EQ(SettingsManager::getAs<int64_t>("score").value(), 42);

	// Default-only key should still be accessible.
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("default_only").value(), 1.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, GetWithFallback) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	const auto val = SettingsManager::get("missing", 42.0f);
	EXPECT_FLOAT_EQ(std::get<float>(val), 42.0f);
	core::Log::invalidate();
}

TEST(SettingsManager, HasAndKeys) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::setDefault("a", int64_t{1});
	SettingsManager::set("b", 2.0f);

	EXPECT_TRUE(SettingsManager::has("a"));
	EXPECT_TRUE(SettingsManager::has("b"));
	EXPECT_FALSE(SettingsManager::has("c"));

	const auto allKeys = SettingsManager::keys();
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

	SettingsManager::loadDefaults(path);
	EXPECT_FLOAT_EQ(SettingsManager::getAs<float>("player_speed").value(), 10.0f);
	EXPECT_EQ(SettingsManager::getAs<int64_t>("max_lives").value(), 5);

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(SettingsManager, LoadDefaultsNonexistentFile) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::loadDefaults("/nonexistent/path.yml");
	EXPECT_TRUE(SettingsManager::keys().empty());
	core::Log::invalidate();
}

TEST(SettingsManager, TypedGetMismatchReturnsNullopt) {
	core::Log::init(core::Log::Level::Off);
	SettingsGuard guard;

	SettingsManager::set("val", int64_t{42});
	EXPECT_FALSE(SettingsManager::getAs<float>("val").has_value());
	EXPECT_TRUE(SettingsManager::getAs<int64_t>("val").has_value());
	core::Log::invalidate();
}

TEST(SettingsManager, EmptyGameNameFallback) {
	core::Log::init(core::Log::Level::Off);
	SettingsManager::clear();

	const auto dir = SettingsManager::getUserDirectory();
	EXPECT_NE(dir.string().find("OwlGame"), std::string::npos);

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
