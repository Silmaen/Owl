/**
 * @file RendererSet_test.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer3D.h>
#include <renderer/RendererSet.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace owl::renderer;
using namespace owl::renderer::gpu;

TEST(RendererSet, LayerTypesSelectTheirFamily) {
	EXPECT_EQ(RendererSet::forLayerType("Renderer2D"), RendererSet::base());
	EXPECT_EQ(RendererSet::forLayerType("Unknown"), RendererSet::base());
	EXPECT_TRUE(RendererSet::forLayerType("RendererRaycast").raycast);
	EXPECT_TRUE(RendererSet::forLayerType("RendererIsometric").isometric);
	EXPECT_TRUE(RendererSet::forLayerType("RendererMesh").renderer3D);
	EXPECT_TRUE(RendererSet::forLayerType("RendererVoxel").renderer3D);
	auto set = RendererSet::forLayerType("RendererRaycast");
	set |= RendererSet::forLayerType("RendererVoxel");
	EXPECT_EQ(set, (RendererSet{.raycast = true, .isometric = false, .renderer3D = true}));
}

TEST(RendererSet, NamesRoundTrip) {
	EXPECT_TRUE(RendererSet::base().getNames().empty());
	for (const auto& set: {RendererSet{}, RendererSet::base(), RendererSet::forLayerType("RendererIsometric")})
		EXPECT_EQ(RendererSet::fromNames(set.getNames()), set);
	const std::vector<std::string> names{"Renderer3D", "Nonsense"};
	EXPECT_EQ(RendererSet::fromNames(names), RendererSet::forLayerType("RendererMesh"));
}

TEST(RendererSet, EveryEngineShaderFolderBelongsToAFamily) {
	const auto folders = RendererSet{}.getShaderFolders();
	const auto shaders = owl::test::getRootPath() / "engine_assets" / "shaders";
	ASSERT_TRUE(exists(shaders));
	for (const auto& entry: std::filesystem::directory_iterator(shaders)) {
		const auto name = entry.path().filename().string();
		// The frustum culling pass is a CPU helper at runtime; its compute shader serves its tests only.
		if (name == "frustum_culling")
			continue;
		EXPECT_NE(std::ranges::find(folders, name), folders.end()) << name;
	}
	const auto base = RendererSet::base().getShaderFolders();
	EXPECT_NE(std::ranges::find(base, "renderer2D"), base.end());
	EXPECT_EQ(std::ranges::find(base, "renderer3D"), base.end());
}

TEST(RendererSet, RendererInitialisesTheFamiliesOfTheSetOnly) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	RenderLayerFactory::clear();
	RenderCommand::create(RenderAPI::Type::Null);
	Renderer::initContext();
	Renderer::initShaders({}, RendererSet::forLayerType("RendererRaycast"));
	EXPECT_EQ(Renderer::getState(), Renderer::State::Running);
	EXPECT_EQ(Renderer::getRendererSet(), RendererSet::forLayerType("RendererRaycast"));
	EXPECT_TRUE(RenderLayerFactory::hasType("Renderer2D"));
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererRaycast"));
	EXPECT_FALSE(RenderLayerFactory::hasType("RendererMesh"));
	EXPECT_FALSE(RenderLayerFactory::hasType("RendererIsometric"));
	EXPECT_FALSE(Renderer3D::isReady());
	Renderer::shutdown();
	EXPECT_EQ(Renderer::getState(), Renderer::State::Stopped);

	Renderer::initContext();
	Renderer::initShaders();
	EXPECT_TRUE(Renderer3D::isReady());
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererMesh"));
	Renderer::shutdown();
	EXPECT_FALSE(Renderer3D::isReady());
	RenderLayerFactory::clear();
	RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
