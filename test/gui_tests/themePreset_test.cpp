/**
 * @file themePreset_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/Theme.h>

using namespace owl::gui;
using namespace owl::core;

TEST(Theme, PresetNames) {
	const auto names = Theme::getPresetNames();
	EXPECT_EQ(names.size(), 5u);
	bool hasDark = false;
	bool hasLight = false;
	for (const auto& [preset, name]: names) {
		if (name == "Dark")
			hasDark = true;
		if (name == "Light")
			hasLight = true;
	}
	EXPECT_TRUE(hasDark);
	EXPECT_TRUE(hasLight);
}

TEST(Theme, FromPresetDark) {
	const auto theme = Theme::fromPreset(ThemePreset::Dark);
	constexpr Theme defaultTheme;
	EXPECT_NEAR(theme.windowRounding, defaultTheme.windowRounding, 0.001f);
	EXPECT_NEAR(theme.frameRounding, defaultTheme.frameRounding, 0.001f);
	EXPECT_NEAR(theme.controlsRounding, defaultTheme.controlsRounding, 0.001f);
}

TEST(Theme, FromPresetCustomFallsToDark) {
	const auto custom = Theme::fromPreset(ThemePreset::Custom);
	const auto dark = Theme::fromPreset(ThemePreset::Dark);
	EXPECT_NEAR(custom.windowRounding, dark.windowRounding, 0.001f);
	EXPECT_NEAR(custom.frameRounding, dark.frameRounding, 0.001f);
}

TEST(Theme, FromPresetLight) {
	const auto theme = Theme::fromPreset(ThemePreset::Light);
	// Light theme has different background than dark.
	const auto dark = Theme::fromPreset(ThemePreset::Dark);
	// At minimum one colour should differ.
	const bool differs = theme.windowBackground != dark.windowBackground || theme.text != dark.text;
	EXPECT_TRUE(differs);
}

TEST(Theme, FromPresetDarkBlue) {
	const auto theme = Theme::fromPreset(ThemePreset::DarkBlue);
	EXPECT_TRUE(theme.windowRounding > 0.f);
}

TEST(Theme, FromPresetNord) {
	const auto theme = Theme::fromPreset(ThemePreset::Nord);
	EXPECT_TRUE(theme.windowRounding > 0.f);
}

TEST(Theme, FromPresetSolarized) {
	const auto theme = Theme::fromPreset(ThemePreset::Solarized);
	EXPECT_TRUE(theme.windowRounding > 0.f);
}

TEST(Theme, SaveLoadRoundtrip) {
	Log::init(Log::Level::Off);
	const auto original = Theme::fromPreset(ThemePreset::Nord);
	const std::filesystem::path filePath("theme_roundtrip_test.yml");
	original.saveToFile(filePath);

	Theme loaded;
	loaded.loadFromFile(filePath);

	EXPECT_NEAR(loaded.windowRounding, original.windowRounding, 0.001f);
	EXPECT_NEAR(loaded.frameRounding, original.frameRounding, 0.001f);
	EXPECT_NEAR(loaded.frameBorderSize, original.frameBorderSize, 0.001f);
	EXPECT_NEAR(loaded.indentSpacing, original.indentSpacing, 0.001f);
	EXPECT_NEAR(loaded.tabRounding, original.tabRounding, 0.001f);
	EXPECT_NEAR(loaded.tabOverline, original.tabOverline, 0.001f);
	EXPECT_NEAR(loaded.tabBorder, original.tabBorder, 0.001f);
	EXPECT_NEAR(loaded.controlsRounding, original.controlsRounding, 0.001f);

	// Check a few colours.
	EXPECT_NEAR(loaded.text.x(), original.text.x(), 0.001f);
	EXPECT_NEAR(loaded.windowBackground.x(), original.windowBackground.x(), 0.001f);
	EXPECT_NEAR(loaded.button.x(), original.button.x(), 0.001f);

	remove(filePath);
	Log::invalidate();
}

TEST(Theme, SaveLoadAllPresets) {
	Log::init(Log::Level::Off);
	const std::filesystem::path filePath("theme_allpreset_test.yml");
	const auto presets = {ThemePreset::Dark, ThemePreset::Light, ThemePreset::DarkBlue, ThemePreset::Nord,
						  ThemePreset::Solarized};
	for (const auto preset: presets) {
		const auto original = Theme::fromPreset(preset);
		original.saveToFile(filePath);
		Theme loaded;
		loaded.loadFromFile(filePath);
		EXPECT_NEAR(loaded.controlsRounding, original.controlsRounding, 0.001f);
	}
	remove(filePath);
	Log::invalidate();
}
