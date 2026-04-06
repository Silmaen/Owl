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

namespace owl::gui {

void Theme::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile)) {
		OWL_CORE_ERROR("Theme::loadFromFile: path {} does not exist", iFile.string())
		return;
	}
	if (!is_regular_file(iFile)) {
		OWL_CORE_ERROR("Theme::loadFromFile: path {} is not a file", iFile.string())
		return;
	}
	if (iFile.extension() != ".yml") {
		OWL_CORE_ERROR("Theme::loadFromFile: File {} has not the right format", iFile.string())
		return;
	}
	const YAML::Node data = YAML::LoadFile(iFile.string());
	if (const auto theme = data["Theme"]; theme) {
		if (const auto colors = theme["Colors"]; colors) {
			get(colors, "Text", text);
			get(colors, "WindowBackground", windowBackground);
			get(colors, "ChildBackground", childBackground);
			get(colors, "BackgroundPopup", backgroundPopup);
			get(colors, "Border", border);
			get(colors, "FrameBackground", frameBackground);
			get(colors, "FrameBackgroundHovered", frameBackgroundHovered);
			get(colors, "FrameBackgroundActive", frameBackgroundActive);
			get(colors, "TitleBar", titleBar);
			get(colors, "TitleBarActive", titleBarActive);
			get(colors, "TitleBarCollapsed", titleBarCollapsed);
			get(colors, "MenubarBackground", menubarBackground);
			get(colors, "ScrollbarBackground", scrollbarBackground);
			get(colors, "ScrollbarGrab", scrollbarGrab);
			get(colors, "ScrollbarGrabHovered", scrollbarGrabHovered);
			get(colors, "ScrollbarGrabActive", scrollbarGrabActive);
			get(colors, "CheckMark", checkMark);
			get(colors, "SliderGrab", sliderGrab);
			get(colors, "SliderGrabActive", sliderGrabActive);
			get(colors, "Button", button);
			get(colors, "ButtonHovered", buttonHovered);
			get(colors, "ButtonActive", buttonActive);
			get(colors, "GroupHeader", groupHeader);
			get(colors, "GroupHeaderHovered", groupHeaderHovered);
			get(colors, "GroupHeaderActive", groupHeaderActive);
			get(colors, "Separator", separator);
			get(colors, "SeparatorActive", separatorActive);
			get(colors, "SeparatorHovered", separatorHovered);
			get(colors, "ResizeGrip", resizeGrip);
			get(colors, "ResizeGripHovered", resizeGripHovered);
			get(colors, "ResizeGripActive", resizeGripActive);
			get(colors, "TabHovered", tabHovered);
			get(colors, "Tab", tab);
			get(colors, "TabSelected", tabSelected);
			get(colors, "TabSelectedOverline", tabSelectedOverline);
			get(colors, "TabDimmed", tabDimmed);
			get(colors, "tabDimmedSelected", tabDimmedSelected);
			get(colors, "TabDimmedSelectedOverline", tabDimmedSelectedOverline);
			get(colors, "DockingPreview", dockingPreview);
			get(colors, "DockingEmptyBackground", dockingEmptyBackground);
			get(colors, "Highlight", highlight);
			get(colors, "PropertyField", propertyField);
		}
		if (const auto frame = theme["Frame"]; frame) {
			get(frame, "WindowRounding", windowRounding);
			get(frame, "FrameRounding", frameRounding);
			get(frame, "FrameBorderSize", frameBorderSize);
			get(frame, "IndentSpacing", indentSpacing);
		}
		if (const auto tabParam = theme["Tab"]; tabParam) {
			get(tabParam, "Rounding", tabRounding);
			get(tabParam, "Overline", tabOverline);
			get(tabParam, "Border", tabBorder);
		}
		if (const auto controls = theme["Controls"]; controls) {
			get(controls, "Rounding", controlsRounding);
		}
	}
}

void Theme::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "Theme" << YAML::Value << YAML::BeginMap;
	{
		out << YAML::Key << "Colors" << YAML::Value << YAML::BeginMap;
		{
			out << YAML::Key << "Text" << YAML::Value << text;
			out << YAML::Key << "WindowBackground" << YAML::Value << windowBackground;
			out << YAML::Key << "ChildBackground" << YAML::Value << childBackground;
			out << YAML::Key << "BackgroundPopup" << YAML::Value << backgroundPopup;
			out << YAML::Key << "Border" << YAML::Value << border;
			out << YAML::Key << "FrameBackground" << YAML::Value << frameBackground;
			out << YAML::Key << "FrameBackgroundHovered" << YAML::Value << frameBackgroundHovered;
			out << YAML::Key << "FrameBackgroundActive" << YAML::Value << frameBackgroundActive;
			out << YAML::Key << "TitleBar" << YAML::Value << titleBar;
			out << YAML::Key << "TitleBarActive" << YAML::Value << titleBarActive;
			out << YAML::Key << "TitleBarCollapsed" << YAML::Value << titleBarCollapsed;
			out << YAML::Key << "MenubarBackground" << YAML::Value << menubarBackground;
			out << YAML::Key << "ScrollbarBackground" << YAML::Value << scrollbarBackground;
			out << YAML::Key << "ScrollbarGrab" << YAML::Value << scrollbarGrab;
			out << YAML::Key << "ScrollbarGrabHovered" << YAML::Value << scrollbarGrabHovered;
			out << YAML::Key << "ScrollbarGrabActive" << YAML::Value << scrollbarGrabActive;
			out << YAML::Key << "CheckMark" << YAML::Value << checkMark;
			out << YAML::Key << "SliderGrab" << YAML::Value << sliderGrab;
			out << YAML::Key << "SliderGrabActive" << YAML::Value << sliderGrabActive;
			out << YAML::Key << "Button" << YAML::Value << button;
			out << YAML::Key << "ButtonHovered" << YAML::Value << buttonHovered;
			out << YAML::Key << "ButtonActive" << YAML::Value << buttonActive;
			out << YAML::Key << "GroupHeader" << YAML::Value << groupHeader;
			out << YAML::Key << "GroupHeaderHovered" << YAML::Value << groupHeaderHovered;
			out << YAML::Key << "GroupHeaderActive" << YAML::Value << groupHeaderActive;
			out << YAML::Key << "Separator" << YAML::Value << separator;
			out << YAML::Key << "SeparatorActive" << YAML::Value << separatorActive;
			out << YAML::Key << "SeparatorHovered" << YAML::Value << separatorHovered;
			out << YAML::Key << "ResizeGrip" << YAML::Value << resizeGrip;
			out << YAML::Key << "ResizeGripHovered" << YAML::Value << resizeGripHovered;
			out << YAML::Key << "ResizeGripActive" << YAML::Value << resizeGripActive;
			out << YAML::Key << "TabHovered" << YAML::Value << tabHovered;
			out << YAML::Key << "Tab" << YAML::Value << tab;
			out << YAML::Key << "TabSelected" << YAML::Value << tabSelected;
			out << YAML::Key << "TabSelectedOverline" << YAML::Value << tabSelectedOverline;
			out << YAML::Key << "TabDimmed" << YAML::Value << tabDimmed;
			out << YAML::Key << "tabDimmedSelected" << YAML::Value << tabDimmedSelected;
			out << YAML::Key << "TabDimmedSelectedOverline" << YAML::Value << tabDimmedSelectedOverline;
			out << YAML::Key << "DockingPreview" << YAML::Value << dockingPreview;
			out << YAML::Key << "DockingEmptyBackground" << YAML::Value << dockingEmptyBackground;
			out << YAML::Key << "Highlight" << YAML::Value << highlight;
			out << YAML::Key << "PropertyField" << YAML::Value << propertyField;
			out << YAML::EndMap;
		}
		out << YAML::Key << "Frame" << YAML::Value << YAML::BeginMap;
		{
			out << YAML::Key << "WindowRounding" << YAML::Value << windowRounding;
			out << YAML::Key << "FrameRounding" << YAML::Value << frameRounding;
			out << YAML::Key << "FrameBorderSize" << YAML::Value << frameBorderSize;
			out << YAML::Key << "IndentSpacing" << YAML::Value << indentSpacing;
			out << YAML::EndMap;
		}
		out << YAML::Key << "Tab" << YAML::Value << YAML::BeginMap;
		{
			out << YAML::Key << "Rounding" << YAML::Value << tabRounding;
			out << YAML::Key << "Overline" << YAML::Value << tabOverline;
			out << YAML::Key << "Border" << YAML::Value << tabBorder;
			out << YAML::EndMap;
		}
		out << YAML::Key << "Controls" << YAML::Value << YAML::BeginMap;
		{
			out << YAML::Key << "Rounding" << YAML::Value << controlsRounding;
			out << YAML::EndMap;
		}
		out << YAML::EndMap << YAML::EndMap;
	}
	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
namespace {

auto makeDarkTheme() -> Theme {
	// Default dark theme — the current one.
	return {};
}

auto makeLightTheme() -> Theme {
	Theme t;
	t.text = {0.10f, 0.10f, 0.10f, 1.0f};
	t.windowBackground = {0.94f, 0.94f, 0.94f, 1.0f};
	t.childBackground = {0.90f, 0.90f, 0.90f, 1.0f};
	t.backgroundPopup = {0.98f, 0.98f, 0.98f, 1.0f};
	t.border = {0.70f, 0.70f, 0.70f, 0.65f};

	t.frameBackground = {0.85f, 0.85f, 0.85f, 1.0f};
	t.frameBackgroundHovered = {0.80f, 0.80f, 0.80f, 1.0f};
	t.frameBackgroundActive = {0.75f, 0.75f, 0.75f, 1.0f};

	t.titleBar = {0.82f, 0.82f, 0.82f, 1.0f};
	t.titleBarActive = {0.76f, 0.76f, 0.76f, 1.0f};
	t.titleBarCollapsed = {0.88f, 0.88f, 0.88f, 1.0f};

	t.menubarBackground = {0.86f, 0.86f, 0.86f, 1.0f};

	t.scrollbarBackground = {0.88f, 0.88f, 0.88f, 0.53f};
	t.scrollbarGrab = {0.60f, 0.60f, 0.60f, 0.80f};
	t.scrollbarGrabHovered = {0.50f, 0.50f, 0.50f, 0.80f};
	t.scrollbarGrabActive = {0.40f, 0.40f, 0.40f, 1.0f};

	t.checkMark = {0.20f, 0.45f, 0.80f, 1.0f};

	t.sliderGrab = {0.45f, 0.45f, 0.45f, 0.78f};
	t.sliderGrabActive = {0.30f, 0.30f, 0.30f, 1.0f};

	t.button = {0.78f, 0.78f, 0.78f, 0.80f};
	t.buttonHovered = {0.70f, 0.70f, 0.70f, 1.0f};
	t.buttonActive = {0.65f, 0.65f, 0.65f, 1.0f};

	t.groupHeader = {0.80f, 0.80f, 0.80f, 1.0f};
	t.groupHeaderHovered = {0.75f, 0.75f, 0.75f, 1.0f};
	t.groupHeaderActive = {0.70f, 0.70f, 0.70f, 1.0f};

	t.separator = {0.70f, 0.70f, 0.70f, 0.62f};
	t.separatorActive = {0.15f, 0.50f, 0.85f, 1.0f};
	t.separatorHovered = {0.15f, 0.50f, 0.85f, 0.78f};

	t.resizeGrip = {0.35f, 0.35f, 0.35f, 0.17f};
	t.resizeGripHovered = {0.30f, 0.30f, 0.30f, 0.67f};
	t.resizeGripActive = {0.25f, 0.25f, 0.25f, 0.95f};

	t.tabHovered = {0.30f, 0.55f, 0.90f, 0.80f};
	t.tab = {0.82f, 0.82f, 0.82f, 1.0f};
	t.tabSelected = {0.88f, 0.88f, 0.88f, 1.0f};
	t.tabSelectedOverline = {0.20f, 0.55f, 0.90f, 1.0f};
	t.tabDimmed = {0.84f, 0.84f, 0.84f, 1.0f};
	t.tabDimmedSelected = {0.86f, 0.86f, 0.86f, 1.0f};
	t.tabDimmedSelectedOverline = {0.50f, 0.60f, 0.70f, 1.0f};

	t.dockingPreview = {0.20f, 0.55f, 0.90f, 0.70f};
	t.dockingEmptyBackground = {0.92f, 0.92f, 0.92f, 1.0f};

	t.highlight = {0.20f, 0.55f, 0.90f, 1.0f};
	t.propertyField = {0.85f, 0.85f, 0.85f, 1.0f};

	t.windowRounding = 6.f;
	t.frameRounding = 3.f;
	t.frameBorderSize = 1.0f;
	t.indentSpacing = 11.f;
	t.tabRounding = 6.f;
	t.tabOverline = 2.f;
	t.tabBorder = 0.f;
	t.controlsRounding = 6.f;
	return t;
}

auto makeDarkBlueTheme() -> Theme {
	Theme t;
	t.text = {0.82f, 0.87f, 0.94f, 1.0f};
	t.windowBackground = {0.086f, 0.102f, 0.145f, 1.0f};
	t.childBackground = {0.098f, 0.118f, 0.165f, 1.0f};
	t.backgroundPopup = {0.11f, 0.133f, 0.192f, 1.0f};
	t.border = {0.16f, 0.20f, 0.29f, 1.0f};

	t.frameBackground = {0.12f, 0.15f, 0.22f, 1.0f};
	t.frameBackgroundHovered = {0.15f, 0.19f, 0.27f, 1.0f};
	t.frameBackgroundActive = {0.18f, 0.22f, 0.31f, 1.0f};

	t.titleBar = {0.063f, 0.078f, 0.114f, 1.0f};
	t.titleBarActive = {0.075f, 0.094f, 0.137f, 1.0f};
	t.titleBarCollapsed = {0.08f, 0.098f, 0.141f, 1.0f};

	t.menubarBackground = {0.063f, 0.078f, 0.114f, 1.0f};

	t.scrollbarBackground = {0.063f, 0.078f, 0.114f, 0.53f};
	t.scrollbarGrab = {0.22f, 0.29f, 0.42f, 1.0f};
	t.scrollbarGrabHovered = {0.28f, 0.36f, 0.51f, 1.0f};
	t.scrollbarGrabActive = {0.34f, 0.43f, 0.60f, 1.0f};

	t.checkMark = {0.35f, 0.63f, 0.95f, 1.0f};

	t.sliderGrab = {0.25f, 0.45f, 0.73f, 0.78f};
	t.sliderGrabActive = {0.35f, 0.55f, 0.83f, 1.0f};

	t.button = {0.16f, 0.22f, 0.34f, 0.80f};
	t.buttonHovered = {0.22f, 0.30f, 0.45f, 1.0f};
	t.buttonActive = {0.18f, 0.25f, 0.38f, 1.0f};

	t.groupHeader = {0.12f, 0.16f, 0.24f, 1.0f};
	t.groupHeaderHovered = {0.16f, 0.22f, 0.33f, 1.0f};
	t.groupHeaderActive = {0.20f, 0.27f, 0.40f, 1.0f};

	t.separator = {0.16f, 0.20f, 0.29f, 1.0f};
	t.separatorActive = {0.30f, 0.58f, 0.92f, 1.0f};
	t.separatorHovered = {0.30f, 0.58f, 0.92f, 0.78f};

	t.resizeGrip = {0.22f, 0.30f, 0.44f, 0.25f};
	t.resizeGripHovered = {0.30f, 0.40f, 0.58f, 0.67f};
	t.resizeGripActive = {0.35f, 0.50f, 0.70f, 0.95f};

	t.tabHovered = {0.22f, 0.38f, 0.62f, 1.0f};
	t.tab = {0.063f, 0.078f, 0.114f, 1.0f};
	t.tabSelected = {0.098f, 0.133f, 0.200f, 1.0f};
	t.tabSelectedOverline = {0.30f, 0.58f, 0.92f, 1.0f};
	t.tabDimmed = {0.063f, 0.078f, 0.114f, 1.0f};
	t.tabDimmedSelected = {0.082f, 0.106f, 0.157f, 1.0f};
	t.tabDimmedSelectedOverline = {0.20f, 0.35f, 0.55f, 1.0f};

	t.dockingPreview = {0.30f, 0.58f, 0.92f, 0.70f};
	t.dockingEmptyBackground = {0.086f, 0.102f, 0.145f, 1.0f};

	t.highlight = {0.30f, 0.58f, 0.92f, 1.0f};
	t.propertyField = {0.12f, 0.15f, 0.22f, 1.0f};

	t.windowRounding = 5.f;
	t.frameRounding = 3.f;
	t.frameBorderSize = 1.0f;
	t.indentSpacing = 11.f;
	t.tabRounding = 5.f;
	t.tabOverline = 2.f;
	t.tabBorder = 0.f;
	t.controlsRounding = 5.f;
	return t;
}

auto makeNordTheme() -> Theme {
	// Nord color palette: polar night, snow storm, frost, aurora
	Theme t;
	t.text = {0.847f, 0.871f, 0.914f, 1.0f};// Nord4 snow
	t.windowBackground = {0.180f, 0.204f, 0.251f, 1.0f};// Nord0
	t.childBackground = {0.212f, 0.235f, 0.282f, 1.0f};// Nord1
	t.backgroundPopup = {0.231f, 0.259f, 0.322f, 1.0f};// Nord2
	t.border = {0.263f, 0.298f, 0.369f, 0.65f};// Nord3

	t.frameBackground = {0.212f, 0.235f, 0.282f, 1.0f};
	t.frameBackgroundHovered = {0.231f, 0.259f, 0.322f, 1.0f};
	t.frameBackgroundActive = {0.263f, 0.298f, 0.369f, 1.0f};

	t.titleBar = {0.165f, 0.188f, 0.231f, 1.0f};
	t.titleBarActive = {0.180f, 0.204f, 0.251f, 1.0f};
	t.titleBarCollapsed = {0.196f, 0.220f, 0.271f, 1.0f};

	t.menubarBackground = {0.165f, 0.188f, 0.231f, 1.0f};

	t.scrollbarBackground = {0.180f, 0.204f, 0.251f, 0.53f};
	t.scrollbarGrab = {0.263f, 0.298f, 0.369f, 1.0f};
	t.scrollbarGrabHovered = {0.306f, 0.345f, 0.424f, 1.0f};
	t.scrollbarGrabActive = {0.337f, 0.380f, 0.467f, 1.0f};

	t.checkMark = {0.533f, 0.753f, 0.816f, 1.0f};// Nord8 frost

	t.sliderGrab = {0.502f, 0.631f, 0.757f, 0.78f};// Nord9
	t.sliderGrabActive = {0.533f, 0.753f, 0.816f, 1.0f};

	t.button = {0.263f, 0.298f, 0.369f, 0.80f};
	t.buttonHovered = {0.306f, 0.345f, 0.424f, 1.0f};
	t.buttonActive = {0.337f, 0.380f, 0.467f, 1.0f};

	t.groupHeader = {0.231f, 0.259f, 0.322f, 1.0f};
	t.groupHeaderHovered = {0.263f, 0.298f, 0.369f, 1.0f};
	t.groupHeaderActive = {0.306f, 0.345f, 0.424f, 1.0f};

	t.separator = {0.263f, 0.298f, 0.369f, 0.62f};
	t.separatorActive = {0.533f, 0.753f, 0.816f, 1.0f};
	t.separatorHovered = {0.533f, 0.753f, 0.816f, 0.78f};

	t.resizeGrip = {0.337f, 0.380f, 0.467f, 0.25f};
	t.resizeGripHovered = {0.400f, 0.440f, 0.530f, 0.67f};
	t.resizeGripActive = {0.533f, 0.753f, 0.816f, 0.95f};

	t.tabHovered = {0.337f, 0.506f, 0.584f, 0.80f};
	t.tab = {0.165f, 0.188f, 0.231f, 1.0f};
	t.tabSelected = {0.212f, 0.235f, 0.282f, 1.0f};
	t.tabSelectedOverline = {0.533f, 0.753f, 0.816f, 1.0f};// Nord8
	t.tabDimmed = {0.165f, 0.188f, 0.231f, 1.0f};
	t.tabDimmedSelected = {0.196f, 0.220f, 0.271f, 1.0f};
	t.tabDimmedSelectedOverline = {0.369f, 0.506f, 0.561f, 1.0f};

	t.dockingPreview = {0.533f, 0.753f, 0.816f, 0.70f};
	t.dockingEmptyBackground = {0.180f, 0.204f, 0.251f, 1.0f};

	t.highlight = {0.533f, 0.753f, 0.816f, 1.0f};
	t.propertyField = {0.212f, 0.235f, 0.282f, 1.0f};

	t.windowRounding = 6.f;
	t.frameRounding = 4.f;
	t.frameBorderSize = 0.f;
	t.indentSpacing = 11.f;
	t.tabRounding = 4.f;
	t.tabOverline = 2.f;
	t.tabBorder = 0.f;
	t.controlsRounding = 8.f;
	return t;
}

auto makeSolarizedTheme() -> Theme {
	// Solarized Dark palette
	Theme t;
	t.text = {0.576f, 0.631f, 0.631f, 1.0f};// base0
	t.windowBackground = {0.000f, 0.169f, 0.212f, 1.0f};// base03
	t.childBackground = {0.027f, 0.212f, 0.259f, 1.0f};// base02
	t.backgroundPopup = {0.027f, 0.212f, 0.259f, 1.0f};
	t.border = {0.071f, 0.255f, 0.302f, 0.65f};

	t.frameBackground = {0.027f, 0.212f, 0.259f, 1.0f};
	t.frameBackgroundHovered = {0.071f, 0.255f, 0.302f, 1.0f};
	t.frameBackgroundActive = {0.098f, 0.278f, 0.325f, 1.0f};

	t.titleBar = {0.000f, 0.145f, 0.180f, 1.0f};
	t.titleBarActive = {0.000f, 0.169f, 0.212f, 1.0f};
	t.titleBarCollapsed = {0.027f, 0.190f, 0.235f, 1.0f};

	t.menubarBackground = {0.000f, 0.145f, 0.180f, 1.0f};

	t.scrollbarBackground = {0.000f, 0.145f, 0.180f, 0.53f};
	t.scrollbarGrab = {0.149f, 0.341f, 0.388f, 1.0f};
	t.scrollbarGrabHovered = {0.200f, 0.400f, 0.450f, 1.0f};
	t.scrollbarGrabActive = {0.250f, 0.450f, 0.500f, 1.0f};

	t.checkMark = {0.149f, 0.545f, 0.824f, 1.0f};// blue

	t.sliderGrab = {0.149f, 0.545f, 0.824f, 0.78f};
	t.sliderGrabActive = {0.200f, 0.600f, 0.880f, 1.0f};

	t.button = {0.071f, 0.255f, 0.302f, 0.80f};
	t.buttonHovered = {0.098f, 0.310f, 0.360f, 1.0f};
	t.buttonActive = {0.120f, 0.345f, 0.400f, 1.0f};

	t.groupHeader = {0.027f, 0.212f, 0.259f, 1.0f};
	t.groupHeaderHovered = {0.071f, 0.255f, 0.302f, 1.0f};
	t.groupHeaderActive = {0.098f, 0.278f, 0.325f, 1.0f};

	t.separator = {0.071f, 0.255f, 0.302f, 0.62f};
	t.separatorActive = {0.149f, 0.545f, 0.824f, 1.0f};
	t.separatorHovered = {0.149f, 0.545f, 0.824f, 0.78f};

	t.resizeGrip = {0.149f, 0.341f, 0.388f, 0.25f};
	t.resizeGripHovered = {0.200f, 0.400f, 0.450f, 0.67f};
	t.resizeGripActive = {0.149f, 0.545f, 0.824f, 0.95f};

	t.tabHovered = {0.120f, 0.420f, 0.620f, 0.80f};
	t.tab = {0.000f, 0.145f, 0.180f, 1.0f};
	t.tabSelected = {0.027f, 0.212f, 0.259f, 1.0f};
	t.tabSelectedOverline = {0.522f, 0.600f, 0.000f, 1.0f};// yellow-green
	t.tabDimmed = {0.000f, 0.145f, 0.180f, 1.0f};
	t.tabDimmedSelected = {0.016f, 0.180f, 0.220f, 1.0f};
	t.tabDimmedSelectedOverline = {0.345f, 0.431f, 0.459f, 1.0f};

	t.dockingPreview = {0.149f, 0.545f, 0.824f, 0.70f};
	t.dockingEmptyBackground = {0.000f, 0.169f, 0.212f, 1.0f};

	t.highlight = {0.149f, 0.545f, 0.824f, 1.0f};
	t.propertyField = {0.027f, 0.212f, 0.259f, 1.0f};

	t.windowRounding = 4.f;
	t.frameRounding = 2.f;
	t.frameBorderSize = 1.0f;
	t.indentSpacing = 11.f;
	t.tabRounding = 4.f;
	t.tabOverline = 2.f;
	t.tabBorder = 0.f;
	t.controlsRounding = 4.f;
	return t;
}

}// namespace
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

auto Theme::fromPreset(const ThemePreset iPreset) -> Theme {
	switch (iPreset) {
		case ThemePreset::Dark:
			return makeDarkTheme();
		case ThemePreset::Light:
			return makeLightTheme();
		case ThemePreset::DarkBlue:
			return makeDarkBlueTheme();
		case ThemePreset::Nord:
			return makeNordTheme();
		case ThemePreset::Solarized:
			return makeSolarizedTheme();
		case ThemePreset::Custom:
			return makeDarkTheme();
	}
	return makeDarkTheme();
}

auto Theme::getPresetNames() -> std::vector<std::pair<ThemePreset, std::string>> {
	return {
			{ThemePreset::Dark, "Dark"},
			{ThemePreset::Light, "Light"},
			{ThemePreset::DarkBlue, "Dark Blue"},
			{ThemePreset::Nord, "Nord"},
			{ThemePreset::Solarized, "Solarized Dark"},
	};
}

}// namespace owl::gui
