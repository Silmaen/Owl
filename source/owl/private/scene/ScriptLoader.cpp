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

#include <filesystem>
#include <string>
#include <variant>

namespace owl::scene {

auto loadScriptInstance(const component::LuaScript& iScript, const uint64_t iEntityId, const std::string& iEntityName)
		-> uniq<script::ScriptInstance> {
	auto instance = mkUniq<script::ScriptInstance>();
	instance->setEntityName(iEntityName);
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
			OWL_CORE_ERROR("Scene: Cannot find script '{}' of entity '{}' ({}) in the pack or the asset folders. "
						   "Fix: set the LuaScript path relative to the project folder (e.g. `scripts/player.lua`).",
						   iScript.scriptPath, iEntityName, iEntityId)
			return nullptr;
		}
		loaded = instance->create(iScript.scriptPath, iEntityId);
	}
	if (!loaded) {
		OWL_CORE_ERROR("Scene: Failed to load script '{}' of entity '{}' ({}).", iScript.scriptPath, iEntityName,
					   iEntityId)
		return nullptr;
	}
	for (const auto& [name, type, value]: iScript.properties) {
		switch (type) {
			case script::ScriptPropertyType::Float:
				instance->setProperty(name, std::get<float>(value));
				break;
			case script::ScriptPropertyType::Int:
				instance->setProperty(name, std::get<int64_t>(value));
				break;
			case script::ScriptPropertyType::String:
				instance->setProperty(name, std::get<std::string>(value));
				break;
			case script::ScriptPropertyType::Bool:
				instance->setProperty(name, std::get<bool>(value));
				break;
		}
	}
	return instance;
}

}// namespace owl::scene
