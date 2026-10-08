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

#include <string>
#include <variant>

namespace owl::scene {

auto loadScriptInstance(const component::LuaScript& iScript, const uint64_t iEntityId) -> uniq<script::ScriptInstance> {
	auto instance = mkUniq<script::ScriptInstance>();
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
	if (!loaded)
		loaded = instance->create(iScript.scriptPath, iEntityId);
	if (!loaded) {
		OWL_CORE_ERROR("Scene: Failed to load script '{}'.", iScript.scriptPath)
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
