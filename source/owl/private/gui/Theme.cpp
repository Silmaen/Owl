/**
 * @file Theme.cpp
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/Theme.h"

#include "math/YamlSerializers.h"

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

}// namespace owl::gui
