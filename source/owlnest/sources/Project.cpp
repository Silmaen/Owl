/**
 * @file Project.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Project.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::nest {

void Project::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile))
		return;
	YAML::Node data = YAML::LoadFile(iFile.string());
	if (auto config = data["OwlProject"]; config) {
		if (config["name"])
			name = config["name"].as<std::string>();
		if (config["firstScene"])
			firstScene = config["firstScene"].as<std::string>();
	}
	projectDirectory = iFile.parent_path();
}

void Project::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "OwlProject" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "name" << YAML::Value << name;
	out << YAML::Key << "firstScene" << YAML::Value << firstScene;
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace owl::nest
