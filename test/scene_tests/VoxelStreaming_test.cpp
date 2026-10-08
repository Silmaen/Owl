/**
 * @file VoxelStreaming_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <core/Timestep.h>
#include <data/voxel/TerrainGenerator.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererVoxel.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>

using namespace owl;

namespace {

// Voxel meshes of the test application's engine context.
auto voxelCache() -> renderer::VoxelMeshCache& {
	return app::Application::get().getEngineContext().getVoxelMeshCache();
}

class VoxelStreamingTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "voxelStreaming",
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
		voxelCache().clear();
	}

	void TearDown() override {
		settle();
		voxelCache().clear();
		renderer::Renderer::setRenderStack(renderer::RenderStack{});
	}

	static void settle() { app::Application::get().getTaskScheduler().waitEmptyQueue(); }

	static auto step() -> core::Timestep {
		core::Timestep ts;
		ts.forceUpdate(std::chrono::milliseconds(16));
		return ts;
	}

	// Play-mode scene streaming a small procedural world (radius 1, height 1) around a camera at the origin.
	void makeScene() {
		m_scene = mkShared<scene::Scene>();
		m_camera = m_scene->createEntity("Camera");
		m_camera.getComponent<scene::component::Transform>().transform.translation() = math::vec3{8.f, 8.f, 8.f};
		auto& camera = m_camera.addComponent<scene::component::Camera>();
		camera.primary = true;
		camera.camera.setPerspective(1.2f, 0.1f, 1000.f);
		m_voxel = m_scene->createEntity("Voxel");
		m_voxel.addComponent<scene::component::RendererTag>().rendererName = "voxel_world";
		auto& world = m_voxel.addComponent<scene::component::VoxelWorld>();
		const auto add = [&](const char* iName, const uint16_t iTexture) -> data::voxel::BlockId {
			data::voxel::BlockType type;
			type.name = iName;
			type.setAllFaces(iTexture);
			return world.registry.registerBlock(type);
		};
		world.terrain.stone = add("stone", 1);
		world.terrain.grass = add("grass", 2);
		world.terrain.dirt = add("dirt", 3);
		world.terrain.sand = add("sand", 4);
		world.terrain.amplitude = 6;
		world.proceduralTerrain = true;
		world.streamRadius = 1;
		world.streamHeight = 1;
		auto tileset = mkShared<scene::Tileset>();
		tileset->columns = 4;
		tileset->rows = 4;
		tileset->tileWidth = 16;
		tileset->tileHeight = 16;
		tileset->tiles.resize(tileset->tileCount());
		tileset->texture = renderer::gpu::Texture2D::create(
				renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
		world.tileset = tileset;
		m_scene->onViewportResize({1280, 720});
		m_scene->onStartRuntime();
	}

	auto world() -> scene::component::VoxelWorld& { return m_voxel.getComponent<scene::component::VoxelWorld>(); }

	void frame() { m_scene->onUpdateRuntime(step()); }

	// Frames (each followed by the workers finishing their jobs) until generation and meshing are both idle.
	void runUntilIdle() {
		for (int i = 0; i < 200; ++i) {
			frame();
			settle();
			const auto stats = renderer::RendererVoxel::getStatistics(voxelCache());
			if (world().pendingChunks.empty() && stats.pendingJobCount == 0 && stats.readyMeshCount == 0 && i > 2)
				return;
		}
		FAIL() << "voxel streaming never settled";
	}

	inline static shared<app::Application> s_app;
	shared<scene::Scene> m_scene;
	scene::Entity m_camera;
	scene::Entity m_voxel;
};

}// namespace

TEST_F(VoxelStreamingTest, EveryStreamedChunkEndsMeshedAgainstItsFinalNeighbors) {
	makeScene();
	runUntilIdle();
	EXPECT_EQ(world().world.chunkCount(), 27u);
	const int entityId = static_cast<int>(static_cast<entt::entity>(m_voxel));
	world().world.forEachChunk([&](const math::vec3i& iCoord, const data::voxel::Chunk& iChunk) -> void {
		EXPECT_EQ(renderer::RendererVoxel::getMeshedRevision(voxelCache(), entityId, iCoord), iChunk.getRevision())
				<< iCoord.x() << "," << iCoord.y() << "," << iCoord.z();
	});
	EXPECT_GT(renderer::RendererVoxel::getStatistics(voxelCache()).cachedMeshCount, 0u);
	m_scene->onEndRuntime();
}

TEST_F(VoxelStreamingTest, ChunkGeneratedWithOldParametersIsDropped) {
	makeScene();
	frame();
	EXPECT_FALSE(world().pendingChunks.empty());
	world().terrain.seed += 1;
	settle();
	frame();
	EXPECT_EQ(world().world.chunkCount(), 0u);
	runUntilIdle();
	data::voxel::Chunk expected(math::vec3i{0, 0, 0});
	data::voxel::TerrainGenerator{world().terrain}.generateChunk(expected, math::vec3i{0, 0, 0});
	EXPECT_EQ(world().world.getChunk(math::vec3i{0, 0, 0})->blocks(), expected.blocks());
	m_scene->onEndRuntime();
}

TEST_F(VoxelStreamingTest, ChunkStreamedOutWhileGeneratingIsDropped) {
	makeScene();
	frame();
	m_camera.getComponent<scene::component::Transform>().transform.translation() =
			math::vec3{8.f + 16.f * 20.f, 8.f, 8.f};
	frame();
	settle();
	frame();
	world().world.forEachChunk([&](const math::vec3i& iCoord, const data::voxel::Chunk&) -> void {
		EXPECT_GE(iCoord.x(), 18) << "chunk " << iCoord.x() << " should have been dropped";
	});
	m_scene->onEndRuntime();
}

TEST_F(VoxelStreamingTest, PlayStartForgetsTheEditorPendingGeneration) {
	makeScene();
	world().pendingChunks.insert(42U);
	m_scene->onStartRuntime();
	EXPECT_TRUE(world().pendingChunks.empty());
	m_scene->onEndRuntime();
}
