/**
 * @file Theme.cpp
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/Theme.h"

#include "../math/YamlSerializers.h"

#include <array>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::gui {

namespace {

using ColorMember = math::vec4 Theme::*;
using SizeMember = float Theme::*;

constexpr std::array<std::pair<const char*, ColorMember>, 21> g_colorKeys{{
		{"Surface0", &Theme::surface0},
		{"Surface1", &Theme::surface1},
		{"Surface2", &Theme::surface2},
		{"Surface3", &Theme::surface3},
		{"Surface4", &Theme::surface4},
		{"Well", &Theme::well},
		{"Line", &Theme::line},
		{"LineSoft", &Theme::lineSoft},
		{"Text", &Theme::text},
		{"TextMuted", &Theme::textMuted},
		{"TextDisabled", &Theme::textDisabled},
		{"Accent", &Theme::accent},
		{"OnAccent", &Theme::onAccent},
		{"Play", &Theme::play},
		{"Success", &Theme::success},
		{"Warning", &Theme::warning},
		{"Error", &Theme::error},
		{"Info", &Theme::info},
		{"AxisX", &Theme::axisX},
		{"AxisY", &Theme::axisY},
		{"AxisZ", &Theme::axisZ},
}};

constexpr std::array<std::pair<const char*, SizeMember>, 4> g_sizeKeys{{
		{"WindowRounding", &Theme::windowRounding},
		{"FrameRounding", &Theme::frameRounding},
		{"TabRounding", &Theme::tabRounding},
		{"IndentSpacing", &Theme::indentSpacing},
}};

auto makeOwlLight() -> Theme {
	Theme theme;
	theme.surface0 = {0.851f, 0.863f, 0.882f, 1.f};
	theme.surface1 = {0.953f, 0.957f, 0.965f, 1.f};
	theme.surface2 = {0.914f, 0.922f, 0.933f, 1.f};
	theme.surface3 = {0.875f, 0.886f, 0.902f, 1.f};
	theme.surface4 = {0.824f, 0.839f, 0.863f, 1.f};
	theme.well = {1.f, 1.f, 1.f, 1.f};
	theme.line = {0.784f, 0.8f, 0.827f, 1.f};
	theme.lineSoft = {0.867f, 0.878f, 0.898f, 1.f};
	theme.text = {0.114f, 0.125f, 0.145f, 1.f};
	theme.textMuted = {0.333f, 0.361f, 0.4f, 1.f};
	theme.textDisabled = {0.569f, 0.592f, 0.631f, 1.f};
	theme.accent = {0.788f, 0.51f, 0.102f, 1.f};
	theme.onAccent = {0.102f, 0.071f, 0.02f, 1.f};
	theme.play = {0.122f, 0.439f, 0.898f, 1.f};
	theme.success = {0.18f, 0.604f, 0.388f, 1.f};
	theme.warning = {0.816f, 0.416f, 0.11f, 1.f};
	theme.error = {0.812f, 0.231f, 0.2f, 1.f};
	theme.info = {0.184f, 0.498f, 0.839f, 1.f};
	theme.axisX = {0.816f, 0.267f, 0.235f, 1.f};
	theme.axisY = {0.247f, 0.604f, 0.275f, 1.f};
	theme.axisZ = {0.184f, 0.439f, 0.839f, 1.f};
	return theme;
}

}// namespace

void Theme::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile)) {
		OWL_CORE_ERROR("Theme::loadFromFile: path {} does not exist.", iFile.string())
		return;
	}
	if (!is_regular_file(iFile)) {
		OWL_CORE_ERROR("Theme::loadFromFile: path {} is not a file.", iFile.string())
		return;
	}
	if (iFile.extension() != ".yml") {
		OWL_CORE_ERROR("Theme::loadFromFile: File {} has not the right format.", iFile.string())
		return;
	}
	const YAML::Node data = YAML::LoadFile(iFile.string());
	const auto theme = data["Theme"];
	if (!theme)
		return;
	if (const auto colors = theme["Colors"]; colors) {
		for (const auto& [key, member]: g_colorKeys) get(colors, key, this->*member);
	}
	if (const auto shape = theme["Shape"]; shape) {
		for (const auto& [key, member]: g_sizeKeys) get(shape, key, this->*member);
	}
}

void Theme::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Theme" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "Colors" << YAML::Value << YAML::BeginMap;
	for (const auto& [key, member]: g_colorKeys) out << YAML::Key << key << YAML::Value << this->*member;
	out << YAML::EndMap;
	out << YAML::Key << "Shape" << YAML::Value << YAML::BeginMap;
	for (const auto& [key, member]: g_sizeKeys) out << YAML::Key << key << YAML::Value << this->*member;
	out << YAML::EndMap;
	out << YAML::EndMap << YAML::EndMap;
	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

auto Theme::fromPreset(const ThemePreset iPreset) -> Theme {
	switch (iPreset) {
		case ThemePreset::OwlDark:
			return {};
		case ThemePreset::OwlLight:
			return makeOwlLight();
	}
	return {};
}

auto Theme::getPresetNames() -> std::vector<std::pair<ThemePreset, std::string>> {
	return {{ThemePreset::OwlDark, "Owl Dark"}, {ThemePreset::OwlLight, "Owl Light"}};
}

auto Theme::presetFromName(const std::string_view iName) -> ThemePreset {
	if (iName == "Owl Light" || iName == "Light")
		return ThemePreset::OwlLight;
	return ThemePreset::OwlDark;
}

}// namespace owl::gui
