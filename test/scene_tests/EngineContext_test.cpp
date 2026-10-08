/**
 * @file EngineContext_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <renderer/RendererVoxel.h>
#include <scene/Scene.h>
#include <scene/ScreenTransition.h>
#include <scene/SettingsManager.h>

#include <chrono>
#include <cstdint>

using namespace owl;

TEST(EngineContext, ContextsAreIndependent) {
	app::EngineContext first;
	app::EngineContext second;
	first.getScreenTransition().start(scene::ScreenTransition::Type::FadeOut, 0.5f);
	first.getSettings().set("lives", int64_t{3});
	EXPECT_TRUE(first.getScreenTransition().isActive());
	EXPECT_FALSE(second.getScreenTransition().isActive());
	EXPECT_TRUE(first.getSettings().has("lives"));
	EXPECT_FALSE(second.getSettings().has("lives"));
	EXPECT_NE(&first.getVoxelMeshCache(), &second.getVoxelMeshCache());
	EXPECT_EQ(renderer::RendererVoxel::getStatistics(first.getVoxelMeshCache()).cachedMeshCount, 0u);
}

TEST(EngineContext, SceneWithoutApplicationHasNoneUntilGivenOne) {
	core::Log::init(core::Log::Level::Off);
	const auto scene = mkShared<scene::Scene>();
	EXPECT_EQ(scene->getEngineContext(), nullptr);
	// Without context, a rendered runtime frame skips the transition overlay and the voxel meshes.
	scene->onStartRuntime();
	core::Timestep step;
	step.forceUpdate(std::chrono::milliseconds(16));
	scene->onUpdateRuntime(step, true);
	scene->onEndRuntime();
	app::EngineContext context;
	scene->setEngineContext(&context);
	EXPECT_EQ(scene->getEngineContext(), &context);
	EXPECT_EQ(scene::Scene::copy(scene)->getEngineContext(), &context);
	core::Log::invalidate();
}
