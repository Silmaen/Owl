/**
 * @file EditorSettings.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSettings.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::nest {

void EditorSettings::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile))
		return;
	YAML::Node data = YAML::LoadFile(iFile.string());
	if (auto config = data["EditorSettings"]; config) {
		if (config["showStats"])
			showStats = config["showStats"].as<bool>();
	}
}

void EditorSettings::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "showStats" << YAML::Value << showStats;
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace owl::nest
