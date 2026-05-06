/**
 * @file Project.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
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
		if (config["version"])
			version = config["version"].as<std::string>();
		if (config["author"])
			author = config["author"].as<std::string>();
		if (config["description"])
			description = config["description"].as<std::string>();
		if (config["icon"])
			icon = config["icon"].as<std::string>();
		if (auto win = config["window"]; win) {
			if (win["width"])
				window.width = win["width"].as<uint32_t>();
			if (win["height"])
				window.height = win["height"].as<uint32_t>();
			if (win["fullscreen"])
				window.fullscreen = win["fullscreen"].as<bool>();
			if (win["resizable"])
				window.resizable = win["resizable"].as<bool>();
		}
		if (const auto stack = config["RendererStack"]; stack)
			rendererStack = renderer::RendererStackConfig::fromYaml(stack);
	}
	projectDirectory = iFile.parent_path();
}

void Project::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "OwlProject" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "name" << YAML::Value << name;
	out << YAML::Key << "firstScene" << YAML::Value << firstScene;
	if (!version.empty())
		out << YAML::Key << "version" << YAML::Value << version;
	if (!author.empty())
		out << YAML::Key << "author" << YAML::Value << author;
	if (!description.empty())
		out << YAML::Key << "description" << YAML::Value << description;
	if (!icon.empty())
		out << YAML::Key << "icon" << YAML::Value << icon;
	out << YAML::Key << "window" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "width" << YAML::Value << window.width;
	out << YAML::Key << "height" << YAML::Value << window.height;
	out << YAML::Key << "fullscreen" << YAML::Value << window.fullscreen;
	out << YAML::Key << "resizable" << YAML::Value << window.resizable;
	out << YAML::EndMap;// window
	if (!rendererStack.isEmpty())
		out << YAML::Key << "RendererStack" << YAML::Value << rendererStack.toYaml();
	out << YAML::EndMap;// OwlProject
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace owl::nest
