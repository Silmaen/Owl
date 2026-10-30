/**
 * @file EditorSettings.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorSettings.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl::nest {

namespace {

auto projectKey(const std::filesystem::path& iProjectDir) -> std::string {
	auto normal = iProjectDir.lexically_normal();
	if (!normal.has_filename() && normal.has_relative_path())
		normal = normal.parent_path();
	return normal.generic_string();
}

auto readSession(const YAML::Node& iNode) -> ProjectSession {
	ProjectSession session;
	if (const auto docs = iNode["documents"]; docs && docs.IsSequence()) {
		for (const auto& doc: docs) session.documents.push_back(doc.as<std::string>());
	}
	session.activeDocument = iNode["active"].as<std::string>("");
	session.selectedEntity = iNode["selectedEntity"].as<uint64_t>(0);
	return session;
}

}// namespace

auto ProjectSession::toStored(const std::filesystem::path& iProjectDir, const std::filesystem::path& iFile)
		-> std::string {
	const auto relative = iFile.lexically_normal().lexically_relative(iProjectDir.lexically_normal());
	if (relative.empty() || *relative.begin() == "..")
		return iFile.lexically_normal().generic_string();
	return relative.generic_string();
}

auto ProjectSession::resolve(const std::filesystem::path& iProjectDir, const std::string& iStored)
		-> std::filesystem::path {
	const std::filesystem::path stored{iStored};
	return stored.is_absolute() ? stored : (iProjectDir / stored).lexically_normal();
}

void EditorSettings::setProjectSession(const std::filesystem::path& iProjectDir, const ProjectSession& iSession) {
	projectSessions[projectKey(iProjectDir)] = iSession;
}

auto EditorSettings::getProjectSession(const std::filesystem::path& iProjectDir) const
		-> std::optional<ProjectSession> {
	if (const auto it = projectSessions.find(projectKey(iProjectDir)); it != projectSessions.end())
		return it->second;
	return std::nullopt;
}

void EditorSettings::pushRecentProject(const std::filesystem::path& iProjectDir) {
	const auto canonical = projectKey(iProjectDir);
	// Remove any existing entry for this project.
	std::erase_if(recentProjects, [&canonical](const std::string& iEntry) -> bool { return iEntry == canonical; });
	// Insert at the front.
	recentProjects.insert(recentProjects.begin(), canonical);
	// Cap the list length.
	if (recentProjects.size() > maxRecentProjects)
		recentProjects.resize(maxRecentProjects);
}

void EditorSettings::removeRecentProject(const std::filesystem::path& iProjectDir) {
	const auto canonical = projectKey(iProjectDir);
	std::erase_if(recentProjects, [&canonical](const std::string& iEntry) -> bool { return iEntry == canonical; });
}

namespace {

void readStrings(const YAML::Node& iNode, std::vector<std::string>& oStrings) {
	if (!iNode || !iNode.IsSequence())
		return;
	oStrings.clear();
	for (const auto& entry: iNode) oStrings.push_back(entry.as<std::string>());
}

}// namespace

namespace {

// Read `iKey` into `oValue` when the file has it, else leave the default.
template<typename T>
void readKey(const YAML::Node& iConfig, const char* iKey, T& oValue) {
	if (iConfig[iKey])
		oValue = iConfig[iKey].as<T>();
}

}// namespace

void EditorSettings::loadFromFile(const std::filesystem::path& iFile) {
	if (!exists(iFile))
		return;
	YAML::Node data = YAML::LoadFile(iFile.string());
	if (auto config = data["EditorSettings"]; config) {
		readKey(config, "showStats", showStats);
		if (config["themePreset"]) {
			const auto preset = gui::Theme::presetFromName(config["themePreset"].as<std::string>());
			for (const auto& [candidate, name]: gui::Theme::getPresetNames()) {
				if (candidate == preset)
					themePreset = name;
			}
		}
		if (config["codeEditorFontSize"])
			codeEditorFontSize = std::clamp(config["codeEditorFontSize"].as<int>(), 8, 48);
		if (config["uiFontSize"])
			uiFontSize = std::clamp(config["uiFontSize"].as<int>(), 14, 24);
		if (config["textScalePercent"])
			textScalePercent = std::clamp(config["textScalePercent"].as<int>(), 75, 150);
		readKey(config, "snapEnabled", snapEnabled);
		if (config["snapStep"])
			snapStep = std::max(0.0001f, config["snapStep"].as<float>());
		if (config["snapMultiplier"])
			snapMultiplier = std::max(0.0001f, config["snapMultiplier"].as<float>());
		if (config["snapRotationDegrees"])
			snapRotationDegrees = std::clamp(config["snapRotationDegrees"].as<float>(), 0.1f, 180.f);
		readKey(config, "snapAutoFromTilemap", snapAutoFromTilemap);
		readKey(config, "showCameraGizmos", showCameraGizmos);
		readKey(config, "graphSnap", graphSnap);
		readKey(config, "showCameraPreview", showCameraPreview);
		readKey(config, "showFrameChip", showFrameChip);
		readKey(config, "showGrid", showGrid);
		readKey(config, "showHud", showHud);
		if (config["editorAmbient"])
			editorAmbient = std::clamp(config["editorAmbient"].as<float>(), 0.f, 2.f);
		readKey(config, "gridOnGround", gridOnGround);
		readKey(config, "showNavGraph", showNavGraph);
		readKey(config, "showTriggerIcons", showTriggerIcons);
		readKey(config, "playControlsCollapsed", playControlsCollapsed);
		readKey(config, "gizmoWorldSpace", gizmoWorldSpace);
		if (config["cameraRotatePercent"])
			cameraRotatePercent = std::clamp(config["cameraRotatePercent"].as<int>(), 10, 300);
		if (config["cameraPanPercent"])
			cameraPanPercent = std::clamp(config["cameraPanPercent"].as<int>(), 10, 300);
		if (config["cameraZoomPercent"])
			cameraZoomPercent = std::clamp(config["cameraZoomPercent"].as<int>(), 10, 300);
		readKey(config, "cameraInvertY", cameraInvertY);
		if (config["cameraDeadZone"])
			cameraDeadZone = std::clamp(config["cameraDeadZone"].as<int>(), 0, 20);
		if (config["autosaveIntervalSeconds"])
			autosaveIntervalSeconds = std::clamp(config["autosaveIntervalSeconds"].as<int>(), 0, 3600);
		if (config["frameRateLimit"])
			frameRateLimit = std::clamp(config["frameRateLimit"].as<int>(), 0, 1000);
		if (config["backgroundFrameRateLimit"])
			backgroundFrameRateLimit = std::clamp(config["backgroundFrameRateLimit"].as<int>(), 0, 1000);
		if (config["idleWaitMs"])
			idleWaitMs = std::clamp(config["idleWaitMs"].as<int>(), 0, 1000);
		readKey(config, "mcpEnabled", mcpEnabled);
		if (config["mcpPort"])
			mcpPort = std::clamp(config["mcpPort"].as<int>(), 1024, 65535);
		readKey(config, "layoutPreset", layoutPreset);
		readKey(config, "layoutVersion", layoutVersion);
		if (const auto bindings = config["keybindings"]; bindings && bindings.IsMap()) {
			keybindingOverrides.clear();
			for (const auto& pair: bindings)
				keybindingOverrides[pair.first.as<std::string>()] = pair.second.as<std::string>();
		}
		readStrings(config["consoleHistory"], consoleHistory);
		readStrings(config["recentProjects"], recentProjects);
		if (const auto sessions = config["projectSessions"]; sessions && sessions.IsSequence()) {
			projectSessions.clear();
			for (const auto& session: sessions) {
				if (const auto project = session["project"].as<std::string>(""); !project.empty())
					projectSessions[project] = readSession(session);
			}
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
	out << YAML::Key << "textScalePercent" << YAML::Value << textScalePercent;
	out << YAML::Key << "snapEnabled" << YAML::Value << snapEnabled;
	out << YAML::Key << "snapStep" << YAML::Value << snapStep;
	out << YAML::Key << "snapMultiplier" << YAML::Value << snapMultiplier;
	out << YAML::Key << "snapRotationDegrees" << YAML::Value << snapRotationDegrees;
	out << YAML::Key << "snapAutoFromTilemap" << YAML::Value << snapAutoFromTilemap;
	out << YAML::Key << "showCameraGizmos" << YAML::Value << showCameraGizmos;
	out << YAML::Key << "graphSnap" << YAML::Value << graphSnap;
	out << YAML::Key << "showCameraPreview" << YAML::Value << showCameraPreview;
	out << YAML::Key << "showFrameChip" << YAML::Value << showFrameChip;
	out << YAML::Key << "showGrid" << YAML::Value << showGrid;
	out << YAML::Key << "showHud" << YAML::Value << showHud;
	out << YAML::Key << "editorAmbient" << YAML::Value << editorAmbient;
	out << YAML::Key << "gridOnGround" << YAML::Value << gridOnGround;
	out << YAML::Key << "showNavGraph" << YAML::Value << showNavGraph;
	out << YAML::Key << "showTriggerIcons" << YAML::Value << showTriggerIcons;
	out << YAML::Key << "playControlsCollapsed" << YAML::Value << playControlsCollapsed;
	out << YAML::Key << "gizmoWorldSpace" << YAML::Value << gizmoWorldSpace;
	out << YAML::Key << "cameraRotatePercent" << YAML::Value << cameraRotatePercent;
	out << YAML::Key << "cameraPanPercent" << YAML::Value << cameraPanPercent;
	out << YAML::Key << "cameraZoomPercent" << YAML::Value << cameraZoomPercent;
	out << YAML::Key << "cameraInvertY" << YAML::Value << cameraInvertY;
	out << YAML::Key << "cameraDeadZone" << YAML::Value << cameraDeadZone;
	out << YAML::Key << "autosaveIntervalSeconds" << YAML::Value << autosaveIntervalSeconds;
	out << YAML::Key << "frameRateLimit" << YAML::Value << frameRateLimit;
	out << YAML::Key << "backgroundFrameRateLimit" << YAML::Value << backgroundFrameRateLimit;
	out << YAML::Key << "idleWaitMs" << YAML::Value << idleWaitMs;
	out << YAML::Key << "mcpEnabled" << YAML::Value << mcpEnabled;
	out << YAML::Key << "mcpPort" << YAML::Value << mcpPort;
	out << YAML::Key << "layoutPreset" << YAML::Value << layoutPreset;
	out << YAML::Key << "layoutVersion" << YAML::Value << layoutVersion;
	if (!keybindingOverrides.empty()) {
		out << YAML::Key << "keybindings" << YAML::Value << YAML::BeginMap;
		for (const auto& [id, shortcut]: keybindingOverrides) out << YAML::Key << id << YAML::Value << shortcut;
		out << YAML::EndMap;
	}
	if (!consoleHistory.empty()) {
		out << YAML::Key << "consoleHistory" << YAML::Value << YAML::BeginSeq;
		for (const auto& line: consoleHistory) out << YAML::DoubleQuoted << line;
		out << YAML::EndSeq;
	}
	if (!recentProjects.empty()) {
		out << YAML::Key << "recentProjects" << YAML::Value << YAML::BeginSeq;
		for (const auto& path: recentProjects) out << path;
		out << YAML::EndSeq;
	}
	// Sessions of projects that left the recent list are dropped, which bounds the file.
	if (std::ranges::any_of(recentProjects, [this](const std::string& iProject) -> bool {
			return projectSessions.contains(iProject);
		})) {
		out << YAML::Key << "projectSessions" << YAML::Value << YAML::BeginSeq;
		for (const auto& project: recentProjects) {
			const auto it = projectSessions.find(project);
			if (it == projectSessions.end())
				continue;
			out << YAML::BeginMap;
			out << YAML::Key << "project" << YAML::Value << project;
			out << YAML::Key << "documents" << YAML::Value << YAML::BeginSeq;
			for (const auto& doc: it->second.documents) out << doc;
			out << YAML::EndSeq;
			out << YAML::Key << "active" << YAML::Value << it->second.activeDocument;
			out << YAML::Key << "selectedEntity" << YAML::Value << it->second.selectedEntity;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace owl::nest
