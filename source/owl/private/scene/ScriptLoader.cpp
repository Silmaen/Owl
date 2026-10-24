/**
 * @file ScriptLoader.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "ScriptLoader.h"

#include "app/Application.h"
#include "platform/FileUtils.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace owl::scene {

namespace {

auto fileBytes(const std::filesystem::path& iFile) -> std::vector<uint8_t> {
	const std::string text = platform::fileToString(iFile);
	return {text.begin(), text.end()};
}

// The modules of `require` are read like the scripts: the open pack, then the asset folders, then the working folder.
auto readModule(const std::string& iPath) -> std::optional<std::vector<uint8_t>> {
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (app.packContains(iPath))
			if (auto data = app.loadFromPack(iPath))
				return data;
		for (const auto& [title, assetsPath]: app.getAssetDirectories())
			if (const auto resolved = assetsPath / iPath; exists(resolved))
				return fileBytes(resolved);
	}
	if (exists(std::filesystem::path{iPath}))
		return fileBytes(iPath);
	return std::nullopt;
}

}// namespace

auto loadScriptInstance(Scene& ioScene, const component::LuaScript& iScript, const uint64_t iEntityId,
						const std::string& iEntityName) -> uniq<script::ScriptInstance> {
	auto instance = mkUniq<script::ScriptInstance>();
	instance->setEntityName(iEntityName);
	instance->setScene(&ioScene);
	instance->setModuleReader(readModule);
	bool loaded = false;
	if (app::Application::instanced()) {
		const auto& app = app::Application::get();
		if (app.packContains(iScript.scriptPath))
			if (const auto data = app.loadFromPack(iScript.scriptPath))
				loaded = instance->createFromBuffer(*data, iScript.scriptPath, iEntityId);
		if (!loaded) {
			for (const auto& [title, assetsPath]: app.getAssetDirectories()) {
				if (const auto resolved = assetsPath / iScript.scriptPath; exists(resolved)) {
					loaded = instance->create(resolved.string(), iEntityId);
					break;
				}
			}
		}
	}
	if (!loaded) {
		if (!exists(std::filesystem::path{iScript.scriptPath})) {
			OWL_CORE_ERROR("Scene: Cannot find script '{}' of '{}' ({}) in the pack or the asset folders. "
						   "Fix: set the LuaScript path relative to the project folder (e.g. `scripts/player.lua`).",
						   iScript.scriptPath, iEntityName, iEntityId)
			return nullptr;
		}
		loaded = instance->create(iScript.scriptPath, iEntityId);
	}
	if (!loaded) {
		OWL_CORE_ERROR("Scene: Failed to load script '{}' of '{}' ({}).", iScript.scriptPath, iEntityName, iEntityId)
		return nullptr;
	}
	for (const auto& declared: instance->getDeclaredProperties()) {
		if (std::ranges::none_of(iScript.properties,
								 [&declared](const auto& iListed) -> bool { return iListed.name == declared.name; }))
			instance->setProperty(declared);
	}
	for (const auto& property: iScript.properties) instance->setProperty(property);
	return instance;
}

}// namespace owl::scene
