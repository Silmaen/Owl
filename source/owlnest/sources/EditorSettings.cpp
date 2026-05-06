/**
 * @file EditorSettings.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSettings.h"

#include <algorithm>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::nest {

void EditorSettings::pushRecentProject(const std::filesystem::path& iProjectDir) {
	const auto canonical = iProjectDir.lexically_normal().generic_string();
	// Remove any existing entry for this project.
	std::erase_if(recentProjects, [&canonical](const std::string& iEntry) -> bool { return iEntry == canonical; });
	// Insert at the front.
	recentProjects.insert(recentProjects.begin(), canonical);
	// Cap the list length.
	if (recentProjects.size() > maxRecentProjects)
		recentProjects.resize(maxRecentProjects);
}

void EditorSettings::removeRecentProject(const std::filesystem::path& iProjectDir) {
	const auto canonical = iProjectDir.lexically_normal().generic_string();
	std::erase_if(recentProjects, [&canonical](const std::string& iEntry) -> bool { return iEntry == canonical; });
}

void EditorSettings::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile))
		return;
	YAML::Node data = YAML::LoadFile(iFile.string());
	if (auto config = data["EditorSettings"]; config) {
		if (config["showStats"])
			showStats = config["showStats"].as<bool>();
		if (config["themePreset"])
			themePreset = config["themePreset"].as<std::string>();
		if (config["codeEditorFontSize"])
			codeEditorFontSize = std::clamp(config["codeEditorFontSize"].as<int>(), 8, 48);
		if (config["uiFontSize"])
			uiFontSize = std::clamp(config["uiFontSize"].as<int>(), 14, 24);
		if (const auto bindings = config["keybindings"]; bindings && bindings.IsMap()) {
			keybindingOverrides.clear();
			for (const auto& pair: bindings)
				keybindingOverrides[pair.first.as<std::string>()] = pair.second.as<std::string>();
		}
		if (const auto recents = config["recentProjects"]; recents && recents.IsSequence()) {
			recentProjects.clear();
			for (const auto& entry: recents)
				recentProjects.push_back(entry.as<std::string>());
		}
	}
}

void EditorSettings::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "showStats" << YAML::Value << showStats;
	out << YAML::Key << "themePreset" << YAML::Value << themePreset;
	out << YAML::Key << "codeEditorFontSize" << YAML::Value << codeEditorFontSize;
	out << YAML::Key << "uiFontSize" << YAML::Value << uiFontSize;
	if (!keybindingOverrides.empty()) {
		out << YAML::Key << "keybindings" << YAML::Value << YAML::BeginMap;
		for (const auto& [id, shortcut]: keybindingOverrides)
			out << YAML::Key << id << YAML::Value << shortcut;
		out << YAML::EndMap;
	}
	if (!recentProjects.empty()) {
		out << YAML::Key << "recentProjects" << YAML::Value << YAML::BeginSeq;
		for (const auto& path: recentProjects)
			out << path;
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace owl::nest
