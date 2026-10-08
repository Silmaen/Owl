/**
 * @file EngineContext.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "app/EngineContext.h"

#include "renderer/RendererVoxel.h"
#include "scene/ScreenTransition.h"
#include "scene/SettingsManager.h"

namespace owl::app {

EngineContext::EngineContext()
	: mp_screenTransition{mkUniq<scene::ScreenTransition>()}, mp_settings{mkUniq<scene::SettingsManager>()},
	  mp_voxelMeshCache{mkUniq<renderer::VoxelMeshCache>()} {}

EngineContext::~EngineContext() = default;

}// namespace owl::app
