/**
 * @file VoxelRuntimeRender_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <renderer/CameraEditor.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererVoxel.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>

using namespace owl;

namespace {

class VoxelRuntimeRenderTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "voxelRuntimeRender",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override {
		renderer::RendererStackConfig config;
		config.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = {}});
		config.entries.push_back({.typeKey = "RendererVoxel", .name = "voxel_world", .defaultConfig = {}});
		renderer::Renderer::setRenderStack(
				renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
		renderer::RendererVoxel::clearCache();
	}

	void TearDown() override {
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
		renderer::RendererVoxel::clearCache();
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}

	inline static shared<app::Application> s_app;
};

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

// Runner-like scene: an authored voxel world in front of a primary perspective camera.
auto makeVoxelScene() -> shared<scene::Scene> {
	auto sc = mkShared<scene::Scene>();
	auto cam = sc->createEntity("Camera");
	cam.getComponent<scene::component::Transform>().transform.translation() = math::vec3{8.f, 8.f, 40.f};
	auto& camera = cam.addComponent<scene::component::Camera>();
	camera.primary = true;
	camera.camera.setPerspective(0.785f, 0.1f, 1000.f);

	auto voxel = sc->createEntity("Voxel");
	voxel.addComponent<scene::component::RendererTag>().rendererName = "voxel_world";
	auto& world = voxel.addComponent<scene::component::VoxelWorld>();
	data::voxel::BlockType stone;
	stone.name = "stone";
	stone.setAllFaces(1);
	const auto stoneId = world.registry.registerBlock(stone);
	world.world.setBlock(math::vec3i{8, 8, 8}, stoneId);
	auto tileset = mkShared<scene::Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	world.tileset = tileset;
	sc->onViewportResize({1280, 720});
	return sc;
}

}// namespace

TEST_F(VoxelRuntimeRenderTest, RuntimeFrameMeshesAndDrawsVoxelWorld) {
	const auto sc = makeVoxelScene();
	sc->onStartRuntime();
	EXPECT_EQ(renderer::RendererVoxel::getStatistics().cachedMeshCount, 0u);
	sc->onUpdateRuntime(makeStep(16));
	const auto stats = renderer::RendererVoxel::getStatistics();
	EXPECT_GE(stats.cachedMeshCount, 1u);
	EXPECT_GE(stats.drawnMeshCount, 1u);
	sc->onEndRuntime();
}

TEST_F(VoxelRuntimeRenderTest, PausedFrameKeepsDrawingVoxelWorld) {
	const auto sc = makeVoxelScene();
	sc->onStartRuntime();
	sc->onRenderRuntime();
	const auto stats = renderer::RendererVoxel::getStatistics();
	EXPECT_GE(stats.cachedMeshCount, 1u);
	EXPECT_GE(stats.drawnMeshCount, 1u);
	sc->onEndRuntime();
}

TEST_F(VoxelRuntimeRenderTest, EditorFrameUsesTheSamePreparation) {
	const auto sc = makeVoxelScene();
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({1280, 720});
	sc->onUpdateEditor(makeStep(16), camera);
	EXPECT_GE(renderer::RendererVoxel::getStatistics().cachedMeshCount, 1u);
}

TEST_F(VoxelRuntimeRenderTest, BackgroundUpdateDoesNotMesh) {
	const auto sc = makeVoxelScene();
	sc->onStartRuntime();
	sc->onUpdateRuntime(makeStep(16), /*iRender=*/false);
	EXPECT_EQ(renderer::RendererVoxel::getStatistics().cachedMeshCount, 0u);
	sc->onEndRuntime();
}
