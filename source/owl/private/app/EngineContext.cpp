/**
 * @file EngineContext.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/EngineContext.h"

#include "renderer/MaterialLibrary.h"
#include "renderer/MeshCache.h"
#include "renderer/RendererVoxel.h"
#include "scene/HudLibrary.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"

namespace owl::app {

EngineContext::EngineContext()
	: mp_screenTransition{mkUniq<scene::ScreenTransition>()}, mp_settings{mkUniq<scene::SettingsManager>()},
	  mp_meshCache{mkUniq<renderer::MeshCache>()}, mp_materialLibrary{mkUniq<renderer::MaterialLibrary>()},
	  mp_voxelMeshCache{mkUniq<renderer::VoxelMeshCache>()}, mp_hudLibrary{mkUniq<scene::HudLibrary>(this)} {}

EngineContext::~EngineContext() = default;

auto EngineContext::loadActionMap(const std::filesystem::path& iProjectDirectory) -> bool {
	setActionMap({});
	if (iProjectDirectory.empty())
		return false;
	const auto file = iProjectDirectory / input::ActionMap::fileName;
	if (!exists(file))
		return false;
	auto map = input::ActionMap::loadFile(file);
	if (!map)
		return false;
	setActionMap(*map);
	return true;
}

auto EngineContext::loadActionMapFromText(const std::string_view iText) -> bool {
	setActionMap({});
	auto map = input::ActionMap::fromYaml(iText, input::ActionMap::fileName);
	if (!map)
		return false;
	setActionMap(*map);
	return true;
}

}// namespace owl::app
