/**
 * @file Project.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Project.h"

#include <platform/AtomicFile.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

#include <array>
#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

namespace owl::nest {

namespace {

constexpr std::array<core::MigrationStep, 0> g_projectMigrations{};
constexpr core::DocumentFormat g_projectFormat{.name = "Project", .migrations = g_projectMigrations};

void readWindowSettings(const YAML::Node& iNode, Project::WindowSettings& oWindow) {
	if (iNode["width"])
		oWindow.width = iNode["width"].as<uint32_t>();
	if (iNode["height"])
		oWindow.height = iNode["height"].as<uint32_t>();
	if (iNode["fullscreen"])
		oWindow.fullscreen = iNode["fullscreen"].as<bool>();
	if (iNode["resizable"])
		oWindow.resizable = iNode["resizable"].as<bool>();
}

void readProjectConfig(const YAML::Node& iConfig, Project& oProject) {
	if (iConfig["name"])
		oProject.name = iConfig["name"].as<std::string>();
	if (iConfig["firstScene"])
		oProject.firstScene = iConfig["firstScene"].as<std::string>();
	if (iConfig["version"])
		oProject.version = iConfig["version"].as<std::string>();
	if (iConfig["author"])
		oProject.author = iConfig["author"].as<std::string>();
	if (iConfig["description"])
		oProject.description = iConfig["description"].as<std::string>();
	if (iConfig["icon"])
		oProject.icon = iConfig["icon"].as<std::string>();
	if (const auto win = iConfig["window"]; win)
		readWindowSettings(win, oProject.window);
	if (const auto stack = iConfig["RendererStack"]; stack)
		oProject.rendererStack = renderer::RendererStackConfig::fromYaml(stack);
}

}// namespace

auto Project::format() -> const core::DocumentFormat& { return g_projectFormat; }

auto Project::loadFromFile(const std::filesystem::path& iFile) -> bool {
	const std::ifstream in(iFile, std::ios::binary);
	if (!in.is_open()) {
		OWL_ERROR("Project: Cannot open {}.", iFile.string())
		return false;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	std::string text = buffer.str();
	if (const auto fileFormat = core::upgradeDocumentText(g_projectFormat, text, iFile.string()); !fileFormat) {
		OWL_ERROR("Project: Cannot load {}: {}.", iFile.string(), describe(fileFormat.error()))
		return false;
	}
	Project loaded = *this;
	try {
		if (const YAML::Node data = YAML::Load(text); data["OwlProject"])
			readProjectConfig(data["OwlProject"], loaded);
	} catch (const std::exception& iEx) {
		OWL_ERROR("Project: Cannot load {}: {}.", iFile.string(), iEx.what())
		return false;
	}
	loaded.projectDirectory = iFile.parent_path();
	*this = std::move(loaded);
	return true;
}

auto Project::makeExportSettings(const std::filesystem::path& iOutputDir, const std::filesystem::path& iRunnerDir) const
		-> data::assets::pack::ExportSettings {
	return {.gameName = name,
			.firstScene = firstScene,
			.version = version,
			.author = author,
			.description = description,
			.icon = icon,
			.projectDirectory = projectDirectory,
			.outputDirectory = iOutputDir,
			.runnerDirectory = iRunnerDir,
			.windowSize = {window.width, window.height},
			.fullscreen = window.fullscreen,
			.resizable = window.resizable,
			.rendererStack = rendererStack,
			.packFlags = data::assets::pack::PackFlags::Default};
}

auto Project::saveToFile(const std::filesystem::path& iFile) const -> bool {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << std::string{core::g_FormatVersionKey} << YAML::Value << g_projectFormat.currentVersion();
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

	if (const auto written = platform::writeFileAtomic(iFile, out.c_str()); !written) {
		OWL_ERROR("Project: Cannot save {}: {}.", iFile.string(), describe(written.error()))
		return false;
	}
	return true;
}

}// namespace owl::nest
