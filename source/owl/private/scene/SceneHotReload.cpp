/**
 * @file SceneHotReload.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/Scene.h"

#include "ScriptLoader.h"
#include "app/Application.h"
#include "platform/FileWatcher.h"
#include "scene/TilemapAsset.h"
#include "scene/Tileset.h"
#include "scene/component/ID.h"
#include "scene/component/LuaScript.h"
#include "scene/component/RaycastDoor.h"
#include "scene/component/RaycastPushWall.h"
#include "scene/component/Tag.h"
#include "scene/component/Tilemap.h"
#include "scene/component/VoxelWorld.h"

#include <cstdint>
#include <string>
#include <variant>

namespace owl::scene {

namespace {

auto isAssetFile(const std::filesystem::path& iRelative, const std::filesystem::path& iFile) -> bool {
	if (iRelative.empty())
		return false;
	if (app::Application::instanced()) {
		for (const auto& [title, assetsPath]: app::Application::get().getAssetDirectories())
			if (const auto candidate = assetsPath / iRelative;
				exists(candidate) && platform::isSameFile(candidate, iFile))
				return true;
	}
	return exists(iRelative) && platform::isSameFile(iRelative, iFile);
}

void carryProperties(const component::LuaScript& iScript, const script::ScriptInstance& iOld,
					 const script::ScriptInstance& iNew) {
	for (const auto& [name, type, value]: iScript.properties) {
		switch (type) {
			case script::ScriptPropertyType::Float:
				iNew.setProperty(name, iOld.getPropertyFloat(name).value_or(std::get<float>(value)));
				break;
			case script::ScriptPropertyType::Int:
				iNew.setProperty(name, iOld.getPropertyInt(name).value_or(std::get<int64_t>(value)));
				break;
			case script::ScriptPropertyType::String:
				iNew.setProperty(name, iOld.getPropertyString(name).value_or(std::get<std::string>(value)));
				break;
			case script::ScriptPropertyType::Bool:
				iNew.setProperty(name, iOld.getPropertyBool(name).value_or(std::get<bool>(value)));
				break;
		}
	}
}

}// namespace

auto Scene::onAssetFileChanged(const std::filesystem::path& iFile) -> bool {
	OWL_PROFILE_FUNCTION()

	const auto extension = iFile.extension().string();
	bool used = false;
	if (extension == Tileset::fileExtension()) {
		const auto drop = [&](const std::filesystem::path& iPath, shared<Tileset>& ioTileset) -> void {
			if (ioTileset && isAssetFile(iPath, iFile)) {
				ioTileset.reset();
				used = true;
			}
		};
		for (const auto view = registry.view<component::Tilemap>(); const auto entity: view)
			if (auto& tilemap = view.get<component::Tilemap>(entity); tilemap.asset)
				drop(tilemap.asset->tilesetPath, tilemap.asset->tileset);
		for (const auto view = registry.view<component::RaycastDoor>(); const auto entity: view) {
			auto& door = view.get<component::RaycastDoor>(entity);
			drop(door.tilesetPath, door.tileset);
		}
		for (const auto view = registry.view<component::RaycastPushWall>(); const auto entity: view) {
			auto& push = view.get<component::RaycastPushWall>(entity);
			drop(push.tilesetPath, push.tileset);
		}
		for (const auto view = registry.view<component::VoxelWorld>(); const auto entity: view) {
			auto& voxel = view.get<component::VoxelWorld>(entity);
			drop(voxel.tilesetPath, voxel.tileset);
		}
	} else if (extension == ".owltilemap") {
		for (const auto view = registry.view<component::Tilemap>(); const auto entity: view) {
			if (auto& tilemap = view.get<component::Tilemap>(entity);
				tilemap.asset && isAssetFile(tilemap.tilemapPath, iFile)) {
				tilemap.asset.reset();
				used = true;
			}
		}
	} else if (extension == ".lua") {
		for (const auto view = registry.view<component::LuaScript>(); const auto entity: view) {
			auto& luaScript = view.get<component::LuaScript>(entity);
			if (!luaScript.instance || !isAssetFile(luaScript.scriptPath, iFile))
				continue;
			used = true;
			const auto* tag = registry.try_get<component::Tag>(entity);
			auto fresh =
					loadScriptInstance(*this, luaScript, static_cast<uint64_t>(registry.get<component::ID>(entity).id),
									   tag != nullptr ? tag->tag : std::string{});
			if (!fresh) {
				OWL_CORE_ERROR("Scene: Reload of script '{}' failed, the previous version keeps running.",
							   luaScript.scriptPath)
				continue;
			}
			carryProperties(luaScript, *luaScript.instance, *fresh);
			luaScript.instance = std::move(fresh);
			luaScript.instance->onCreate();
			OWL_CORE_INFO("Scene: Script '{}' reloaded.", luaScript.scriptPath)
		}
	}
	if (used && extension != ".lua")
		invalidateTilemapAssets();
	return used;
}

}// namespace owl::scene
