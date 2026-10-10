/**
 * @file themePreset_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/Theme.h>
#include <gui/UiLayer.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <tuple>

using namespace owl;
using namespace owl::gui;
using namespace owl::core;

namespace {

// Relative luminance (WCAG 2) of a colour.
auto luminance(const math::vec4& iColor) -> float {
	const auto channel = [](const float iValue) -> float {
		return iValue <= 0.03928f ? iValue / 12.92f : std::pow((iValue + 0.055f) / 1.055f, 2.4f);
	};
	return 0.2126f * channel(iColor.x()) + 0.7152f * channel(iColor.y()) + 0.0722f * channel(iColor.z());
}

// Contrast ratio (WCAG 2) between two colours.
auto contrast(const math::vec4& iFirst, const math::vec4& iSecond) -> float {
	const float first = luminance(iFirst);
	const float second = luminance(iSecond);
	return (std::max(first, second) + 0.05f) / (std::min(first, second) + 0.05f);
}

auto sameColor(const ImVec4& iFirst, const ImVec4& iSecond) -> bool {
	constexpr float tolerance = 1e-4f;
	return std::abs(iFirst.x - iSecond.x) < tolerance && std::abs(iFirst.y - iSecond.y) < tolerance &&
		   std::abs(iFirst.z - iSecond.z) < tolerance && std::abs(iFirst.w - iSecond.w) < tolerance;
}

}// namespace

TEST(Theme, PresetNames) {
	const auto names = Theme::getPresetNames();
	ASSERT_EQ(names.size(), 2u);
	EXPECT_EQ(names[0].first, ThemePreset::OwlDark);
	EXPECT_EQ(names[0].second, "Owl Dark");
	EXPECT_EQ(names[1].first, ThemePreset::OwlLight);
	EXPECT_EQ(names[1].second, "Owl Light");
}

TEST(Theme, PresetFromNameMapsRetiredNames) {
	EXPECT_EQ(Theme::presetFromName("Owl Dark"), ThemePreset::OwlDark);
	EXPECT_EQ(Theme::presetFromName("Owl Light"), ThemePreset::OwlLight);
	EXPECT_EQ(Theme::presetFromName("Light"), ThemePreset::OwlLight);
	for (const auto* retired: {"Dark", "Dark Blue", "Nord", "Solarized Dark", "Custom", ""})
		EXPECT_EQ(Theme::presetFromName(retired), ThemePreset::OwlDark) << retired;
}

TEST(Theme, OwlDarkIsTheDefault) {
	constexpr Theme defaults;
	const auto dark = Theme::fromPreset(ThemePreset::OwlDark);
	EXPECT_EQ(dark.surface1, defaults.surface1);
	EXPECT_EQ(dark.accent, defaults.accent);
	EXPECT_NEAR(dark.frameRounding, defaults.frameRounding, 0.001f);
}

TEST(Theme, SurfacesStepTowardsThePressedState) {
	for (const auto& [preset, name]: Theme::getPresetNames()) {
		const auto theme = Theme::fromPreset(preset);
		const bool dark = luminance(theme.surface1) < luminance(theme.text);
		const auto steps = {theme.surface1, theme.surface2, theme.surface3, theme.surface4};
		float previous = luminance(theme.surface0);
		if (!dark)
			previous = luminance(theme.surface1) + 1.f;
		for (const auto& surface: steps) {
			const float current = luminance(surface);
			if (dark)
				EXPECT_GT(current, previous) << name;
			else
				EXPECT_LT(current, previous) << name;
			previous = current;
		}
	}
}

TEST(Theme, TextIsReadableOnEverySurface) {
	for (const auto& [preset, name]: Theme::getPresetNames()) {
		const auto theme = Theme::fromPreset(preset);
		for (const auto& surface: {theme.surface0, theme.surface1, theme.surface2, theme.well})
			EXPECT_GE(contrast(theme.text, surface), 7.f) << name;
		EXPECT_GE(contrast(theme.textMuted, theme.surface1), 4.5f) << name;
		EXPECT_GE(contrast(theme.onAccent, theme.accent), 4.5f) << name;
	}
}

TEST(Theme, MixAndAlphaHelpers) {
	constexpr math::vec4 black{0.f, 0.f, 0.f, 1.f};
	constexpr math::vec4 white{1.f, 1.f, 1.f, 1.f};
	EXPECT_EQ(Theme::mix(black, white, 0.5f), (math::vec4{0.5f, 0.5f, 0.5f, 1.f}));
	EXPECT_EQ(Theme::withAlpha(white, 0.25f), (math::vec4{1.f, 1.f, 1.f, 0.25f}));
	constexpr Theme theme;
	EXPECT_NEAR(theme.getSelection().w(), 0.18f, 0.001f);
}

TEST(Theme, SaveLoadAllPresets) {
	Log::init(Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_theme_presets_test";
	std::filesystem::create_directories(dir);
	for (const auto& [preset, name]: Theme::getPresetNames()) {
		const auto original = Theme::fromPreset(preset);
		const auto file = dir / "theme.yml";
		original.saveToFile(file);
		Theme loaded;
		loaded.accent = {0.f, 0.f, 0.f, 0.f};
		loaded.loadFromFile(file);
		EXPECT_EQ(loaded.surface0, original.surface0) << name;
		EXPECT_EQ(loaded.accent, original.accent) << name;
		EXPECT_EQ(loaded.axisZ, original.axisZ) << name;
		EXPECT_NEAR(loaded.tabRounding, original.tabRounding, 0.001f) << name;
	}
	std::filesystem::remove_all(dir);
	Log::invalidate();
}

TEST(Theme, AppliedStatesAreDistinct) {
	Log::init(Log::Level::Off);
	UiLayer layer;
	layer.disableApp();
	layer.onAttach();
	ImGui::GetIO().IniFilename = nullptr;
	for (const auto& [preset, name]: Theme::getPresetNames()) {
		const auto theme = Theme::fromPreset(preset);
		UiLayer::setTheme(theme);
		EXPECT_EQ(UiLayer::getTheme().accent, theme.accent) << name;
		const auto& colors = ImGui::GetStyle().Colors;
		for (const auto& [rest, hovered, active]:
			 {std::tuple{ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive},
			  std::tuple{ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive},
			  std::tuple{ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive}}) {
			EXPECT_FALSE(sameColor(colors[rest], colors[hovered])) << name;
			EXPECT_FALSE(sameColor(colors[hovered], colors[active])) << name;
		}
		EXPECT_TRUE(sameColor(colors[ImGuiCol_TabSelectedOverline],
							  ImVec4{theme.accent.x(), theme.accent.y(), theme.accent.z(), theme.accent.w()}))
				<< name;
	}
	UiLayer::setTheme();
	layer.onDetach();
	Log::invalidate();
}

TEST(Theme, TextStylesPushAndPop) {
	Log::init(Log::Level::Off);
	UiLayer layer;
	layer.disableApp();
	layer.onAttach();
	ImGui::GetIO().IniFilename = nullptr;
	layer.begin();
	ImGui::Begin("Typography");
	const float base = ImGui::GetFontSize();
	UiLayer::pushTextStyle(TextStyle::Title);
	EXPECT_GT(ImGui::GetFontSize(), base);
	UiLayer::popTextStyle();
	UiLayer::pushTextStyle(TextStyle::Caption);
	EXPECT_LT(ImGui::GetFontSize(), base);
	UiLayer::popTextStyle();
	for (const auto style: {TextStyle::Body, TextStyle::Strong, TextStyle::Section, TextStyle::Label, TextStyle::Value,
							TextStyle::Code}) {
		UiLayer::pushTextStyle(style);
		ImGui::TextUnformatted("Sample");
		UiLayer::popTextStyle();
	}
	EXPECT_FLOAT_EQ(ImGui::GetFontSize(), base);
	ImGui::End();
	layer.end();
	layer.onDetach();
	Log::invalidate();
}
