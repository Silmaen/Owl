/**
 * @file RendererVoxelAsync_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/RendererVoxel.h>
#include <scene/component/VoxelWorld.h>

#include <cstdint>

using namespace owl;

namespace {

constexpr int k_Entity = 7;

auto packKey(const math::vec3i& iCoord) -> uint64_t {
	const auto enc = [](const int32_t iValue) -> uint64_t {
		return static_cast<uint64_t>(static_cast<int64_t>(iValue) + (1 << 20)) & 0x1FFFFF;
	};
	return enc(iCoord.x()) | (enc(iCoord.y()) << 21) | (enc(iCoord.z()) << 42);
}

class RendererVoxelAsyncTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "rendererVoxelAsync",
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
		m_previousConfig = renderer::RendererVoxel::getMeshingConfig();
		// A generous time budget: the default 2 ms is one upload on a slow (emulated, Debug) agent; budget tests set theirs.
		renderer::RendererVoxel::setMeshingConfig({.uploadBudgetMs = 1000.f});
		renderer::RendererVoxel::clearCache();
		m_start = renderer::RendererVoxel::getStatistics();
	}

	void TearDown() override {
		settle();
		renderer::RendererVoxel::clearCache();
		renderer::RendererVoxel::setMeshingConfig(m_previousConfig);
	}

	static auto makeWorld() -> scene::component::VoxelWorld {
		scene::component::VoxelWorld world;
		data::voxel::BlockType stone;
		stone.name = "stone";
		stone.setAllFaces(1);
		s_stone = world.registry.registerBlock(stone);
		auto tileset = mkShared<scene::Tileset>();
		tileset->columns = 4;
		tileset->rows = 4;
		tileset->tileWidth = 16;
		tileset->tileHeight = 16;
		tileset->tiles.resize(tileset->tileCount());
		tileset->texture = renderer::gpu::Texture2D::create(
				renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
		world.tileset = tileset;
		return world;
	}

	// One block in each of `iCount` chunks spaced two chunks apart (no shared border).
	static void addSeparateChunks(scene::component::VoxelWorld& ioWorld, const int32_t iCount) {
		for (int32_t c = 0; c < iCount; ++c) ioWorld.world.setBlock(math::vec3i{c * 32 + 8, 8, 8}, s_stone);
	}

	static void frame(scene::component::VoxelWorld& ioWorld) {
		renderer::RendererVoxel::beginPrepare();
		renderer::RendererVoxel::prepareWorld(ioWorld, k_Entity);
	}

	static void settle() { app::Application::get().getTaskScheduler().waitEmptyQueue(); }

	static auto stats() -> renderer::RendererVoxel::Statistics { return renderer::RendererVoxel::getStatistics(); }

	[[nodiscard]] auto discarded() const -> uint64_t { return stats().discardedMeshCount - m_start.discardedMeshCount; }

	static auto isMeshed(scene::component::VoxelWorld& iWorld, const math::vec3i& iCoord) -> bool {
		const auto revision = renderer::RendererVoxel::getMeshedRevision(k_Entity, iCoord);
		return revision.has_value() && *revision == iWorld.world.getChunk(iCoord)->getRevision();
	}

	inline static shared<app::Application> s_app;
	inline static data::voxel::BlockId s_stone = 0;
	renderer::VoxelMeshingConfig m_previousConfig;
	renderer::RendererVoxel::Statistics m_start;
};

}// namespace

TEST_F(RendererVoxelAsyncTest, MeshesOnWorkersThenUploads) {
	auto world = makeWorld();
	addSeparateChunks(world, 3);
	frame(world);
	EXPECT_EQ(stats().cachedMeshCount, 0u);
	EXPECT_EQ(stats().pendingJobCount, 3u);
	settle();
	frame(world);
	const auto after = stats();
	EXPECT_EQ(after.cachedMeshCount, 3u);
	EXPECT_EQ(after.pendingJobCount, 0u);
	EXPECT_EQ(after.uploadedThisFrame, 3u);
	EXPECT_GT(after.meshingNs, m_start.meshingNs);
	EXPECT_EQ(after.latencyCount - m_start.latencyCount, 3u);
	for (int32_t c = 0; c < 3; ++c) EXPECT_TRUE(isMeshed(world, math::vec3i{c * 2, 0, 0}));
}

TEST_F(RendererVoxelAsyncTest, EditDuringMeshingDiscardsTheResult) {
	auto world = makeWorld();
	addSeparateChunks(world, 1);
	frame(world);
	world.world.setBlock(math::vec3i{9, 8, 8}, s_stone);
	settle();
	frame(world);
	EXPECT_EQ(discarded(), 1u);
	EXPECT_FALSE(renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{0, 0, 0}).has_value());
	EXPECT_EQ(stats().pendingJobCount, 1u);
	settle();
	frame(world);
	EXPECT_TRUE(isMeshed(world, math::vec3i{0, 0, 0}));
	EXPECT_EQ(stats().cachedMeshCount, 1u);
}

TEST_F(RendererVoxelAsyncTest, OldMeshStaysDrawnUntilItsReplacementIsUploaded) {
	auto world = makeWorld();
	addSeparateChunks(world, 1);
	frame(world);
	settle();
	frame(world);
	const auto first = renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{0, 0, 0});
	ASSERT_TRUE(first.has_value());
	world.world.setBlock(math::vec3i{9, 8, 8}, s_stone);
	frame(world);
	EXPECT_EQ(stats().cachedMeshCount, 1u);
	EXPECT_EQ(renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{0, 0, 0}), first);
	settle();
	frame(world);
	EXPECT_TRUE(isMeshed(world, math::vec3i{0, 0, 0}));
	EXPECT_NE(renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{0, 0, 0}), first);
}

TEST_F(RendererVoxelAsyncTest, UnloadDuringMeshingDiscardsTheResult) {
	auto world = makeWorld();
	addSeparateChunks(world, 1);
	frame(world);
	world.world.removeChunk(math::vec3i{0, 0, 0});
	settle();
	frame(world);
	EXPECT_EQ(discarded(), 1u);
	EXPECT_EQ(stats().cachedMeshCount, 0u);
	EXPECT_EQ(stats().pendingJobCount, 0u);
	EXPECT_EQ(stats().readyMeshCount, 0u);
}

TEST_F(RendererVoxelAsyncTest, BorderEditRemeshesTheNeighborChunk) {
	auto world = makeWorld();
	world.world.setBlock(math::vec3i{15, 8, 8}, s_stone);
	world.world.setBlock(math::vec3i{16, 8, 8}, s_stone);
	frame(world);
	settle();
	frame(world);
	const auto ownBefore = renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{0, 0, 0});
	const auto neighborBefore = renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{1, 0, 0});
	ASSERT_TRUE(ownBefore.has_value());
	ASSERT_TRUE(neighborBefore.has_value());
	world.world.setBlock(math::vec3i{15, 8, 8}, data::voxel::g_AirBlock);
	world.world.setBlock(math::vec3i{14, 8, 8}, s_stone);
	world.world.markNeighborChunksDirty(math::vec3i{15, 8, 8});
	frame(world);
	EXPECT_EQ(stats().pendingJobCount, 2u);
	settle();
	frame(world);
	EXPECT_TRUE(isMeshed(world, math::vec3i{0, 0, 0}));
	EXPECT_TRUE(isMeshed(world, math::vec3i{1, 0, 0}));
	EXPECT_NE(renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{1, 0, 0}), neighborBefore);
}

TEST_F(RendererVoxelAsyncTest, UploadCountBudgetIsRespected) {
	renderer::RendererVoxel::setMeshingConfig({.maxUploadsPerFrame = 2, .uploadBudgetMs = 1000.f});
	auto world = makeWorld();
	addSeparateChunks(world, 6);
	frame(world);
	settle();
	for (const uint32_t expected: {2u, 4u, 6u}) {
		frame(world);
		EXPECT_LE(stats().uploadedThisFrame, 2u);
		EXPECT_EQ(stats().cachedMeshCount, expected);
	}
	EXPECT_EQ(stats().readyMeshCount, 0u);
}

TEST_F(RendererVoxelAsyncTest, UploadTimeBudgetStillLetsOneUploadThrough) {
	renderer::RendererVoxel::setMeshingConfig({.maxUploadsPerFrame = 100, .uploadBudgetMs = 0.f});
	auto world = makeWorld();
	addSeparateChunks(world, 3);
	frame(world);
	settle();
	for (const uint32_t expected: {1u, 2u, 3u}) {
		frame(world);
		EXPECT_EQ(stats().uploadedThisFrame, 1u);
		EXPECT_EQ(stats().cachedMeshCount, expected);
	}
}

TEST_F(RendererVoxelAsyncTest, JobsInFlightAreBounded) {
	renderer::RendererVoxel::setMeshingConfig({.maxJobsInFlight = 2});
	auto world = makeWorld();
	addSeparateChunks(world, 5);
	frame(world);
	EXPECT_EQ(stats().pendingJobCount, 2u);
	settle();
	frame(world);
	EXPECT_EQ(stats().cachedMeshCount, 2u);
	EXPECT_EQ(stats().pendingJobCount, 2u);
}

TEST_F(RendererVoxelAsyncTest, PendingNeighborGenerationHoldsMeshingBack) {
	auto world = makeWorld();
	addSeparateChunks(world, 1);
	world.pendingChunks.insert(packKey(math::vec3i{1, 1, 0}));
	frame(world);
	EXPECT_EQ(stats().pendingJobCount, 0u);
	world.pendingChunks.clear();
	frame(world);
	EXPECT_EQ(stats().pendingJobCount, 1u);
}

TEST_F(RendererVoxelAsyncTest, EmptiedChunkDropsItsMeshWithoutAJob) {
	auto world = makeWorld();
	addSeparateChunks(world, 1);
	frame(world);
	settle();
	frame(world);
	EXPECT_EQ(stats().cachedMeshCount, 1u);
	world.world.setBlock(math::vec3i{8, 8, 8}, data::voxel::g_AirBlock);
	frame(world);
	EXPECT_EQ(stats().cachedMeshCount, 0u);
	EXPECT_EQ(stats().pendingJobCount, 0u);
}

TEST_F(RendererVoxelAsyncTest, NearestChunksAreMeshedFirst) {
	renderer::RendererVoxel::setMeshingConfig({.maxJobsInFlight = 1});
	auto world = makeWorld();
	world.world.setBlock(math::vec3i{8 + 32 * 4, 8, 8}, s_stone);
	world.world.setBlock(math::vec3i{8, 8, 8}, s_stone);
	frame(world);
	settle();
	frame(world);
	EXPECT_TRUE(isMeshed(world, math::vec3i{0, 0, 0}));
	EXPECT_FALSE(renderer::RendererVoxel::getMeshedRevision(k_Entity, math::vec3i{8, 0, 0}).has_value());
}
