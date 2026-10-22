/**
 * @file SceneScriptMigration.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "SceneScriptMigration.h"

#include "app/Application.h"
#include "core/SerializerImpl.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace owl::scene {

namespace {

thread_local std::filesystem::path g_sceneFile;

auto locateScript(const std::string& iScriptPath) -> std::filesystem::path {
	std::error_code error;
	if (app::Application::instanced()) {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories()) {
			if (const auto candidate = assetsPath / iScriptPath; exists(candidate, error))
				return candidate;
		}
	}
	for (auto folder = g_sceneFile.parent_path(); !folder.empty(); folder = folder.parent_path()) {
		if (const auto candidate = folder / iScriptPath; exists(candidate, error))
			return candidate;
		if (folder == folder.parent_path())
			break;
	}
	return {};
}

auto onlyDrivesTheScene(const std::string& iScriptPath) -> bool {
	const auto file = locateScript(iScriptPath);
	if (file.empty())
		return false;
	std::ifstream stream{file, std::ios::binary};
	const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
	return !text.empty() && !text.contains("entity_id");
}

}// namespace

SceneScriptMigrationSource::SceneScriptMigrationSource(const std::string& iSceneName) { g_sceneFile = iSceneName; }

SceneScriptMigrationSource::~SceneScriptMigrationSource() { g_sceneFile.clear(); }

auto moveCameraSceneScript(const core::Serializer& ioDocument) -> bool {
	auto root = ioDocument.getImpl()->document;
	const auto entities = root["Entities"];
	if (!entities || !entities.IsSequence() || root["SceneScript"])
		return true;
	for (auto entity: entities) {
		if (!entity.IsMap() || !entity["LuaScript"] || !entity["Camera"] || !entity["Camera"].IsMap() ||
			!entity["Camera"]["primary"] || !entity["Camera"]["primary"].as<bool>(false))
			continue;
		const auto script = entity["LuaScript"];
		if (!script.IsMap() || !script["scriptPath"] || !onlyDrivesTheScene(script["scriptPath"].as<std::string>("")))
			continue;
		root["SceneScript"] = YAML::Clone(script);
		entity.remove("LuaScript");
		break;
	}
	return true;
}

auto hasCameraScript(const core::YamlNode& iEntity) -> bool {
	if (!iEntity.isMap() || !iEntity["LuaScript"].isMap())
		return false;
	const auto camera = iEntity["Camera"];
	return camera.isMap() && camera["primary"].isScalar() && camera["primary"].as<bool>(false);
}

}// namespace owl::scene
