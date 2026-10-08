/**
 * @file VoxelStreamingBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <app/Application.h>
#include <renderer/CameraEditor.h>
#include <renderer/RenderStack.h>
#include <renderer/Renderer.h>
#include <renderer/RendererVoxel.h>
#include <scene/Entity.h>
#include <scene/component/components.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace owl::bench {

namespace {

// Voxel meshes of the test application's engine context.
auto voxelCache() -> renderer::VoxelMeshCache& {
	return app::Application::get().getEngineContext().getVoxelMeshCache();
}

constexpr uint32_t k_MaxSettleFrames = 1500;
constexpr uint32_t k_SettledFrames = 5;
constexpr float k_ChunkSize = 16.f;
// Frames are paced at 60 Hz like a v-synced game loop, so the workers get the rest of each frame.
constexpr std::chrono::microseconds k_FramePeriod{16667};

auto threadCpuNs() -> double {
	timespec ts{};
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	return static_cast<double>(ts.tv_sec) * 1.0e9 + static_cast<double>(ts.tv_nsec);
}

auto processCpuNs() -> double {
	timespec ts{};
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
	return static_cast<double>(ts.tv_sec) * 1.0e9 + static_cast<double>(ts.tv_nsec);
}

auto makeStep() -> core::Timestep {
	core::Timestep step;
	step.forceUpdate(std::chrono::microseconds(16667));
	return step;
}

auto addBlock(scene::component::VoxelWorld& ioWorld, const char* iName, const data::voxel::BlockRenderKind iKind,
			  const uint16_t iTexture) -> data::voxel::BlockId {
	data::voxel::BlockType type;
	type.name = iName;
	type.renderKind = iKind;
	type.solid = iKind != data::voxel::BlockRenderKind::Water;
	type.setAllFaces(iTexture);
	return ioWorld.registry.registerBlock(type);
}

struct StreamingScene {
	shared<scene::Scene> scene;
	scene::Entity camera;
	scene::Entity voxel;
};

auto makeStreamingScene() -> StreamingScene {
	StreamingScene result;
	result.scene = mkShared<scene::Scene>();
	result.camera = result.scene->createEntity("Camera");
	result.camera.getComponent<scene::component::Transform>().transform.translation() = math::vec3{8.f, 24.f, 8.f};
	auto& camera = result.camera.addComponent<scene::component::Camera>();
	camera.primary = true;
	camera.camera.setPerspective(1.2f, 0.1f, 1000.f);

	result.voxel = result.scene->createEntity("Voxel");
	result.voxel.addComponent<scene::component::RendererTag>().rendererName = "voxel_world";
	auto& world = result.voxel.addComponent<scene::component::VoxelWorld>();
	world.terrain.stone = addBlock(world, "stone", data::voxel::BlockRenderKind::Opaque, 1);
	world.terrain.grass = addBlock(world, "grass", data::voxel::BlockRenderKind::Opaque, 2);
	world.terrain.dirt = addBlock(world, "dirt", data::voxel::BlockRenderKind::Opaque, 3);
	world.terrain.sand = addBlock(world, "sand", data::voxel::BlockRenderKind::Opaque, 4);
	world.terrain.water = addBlock(world, "water", data::voxel::BlockRenderKind::Water, 5);
	world.proceduralTerrain = true;
	auto tileset = mkShared<scene::Tileset>();
	tileset->columns = 4;
	tileset->rows = 4;
	tileset->tileWidth = 16;
	tileset->tileHeight = 16;
	tileset->tiles.resize(tileset->tileCount());
	tileset->texture = renderer::gpu::Texture2D::create(
			renderer::gpu::Texture::Specification{.size = {64, 64}, .format = renderer::gpu::ImageFormat::Rgba8});
	world.tileset = tileset;
	result.scene->onViewportResize({1280, 720});
	return result;
}

// Per-frame main-thread cost of a streaming scenario.
struct FrameLog {
	std::vector<double> cpuNs;
	double wallNs = 0.0;
	double processNs = 0.0;
};

auto percentile(std::vector<double> iValues, const double iRatio) -> double {
	if (iValues.empty())
		return 0.0;
	std::ranges::sort(iValues);
	const auto index = static_cast<size_t>(iRatio * static_cast<double>(iValues.size() - 1));
	return iValues[index];
}

void report(Runner& ioRunner, const std::string& iName, const FrameLog& iLog, const StreamingScene& iScene,
			const renderer::RendererVoxel::Statistics& iStart) {
	const auto& cpu = iLog.cpuNs;
	const double total = std::accumulate(cpu.begin(), cpu.end(), 0.0);
	const auto over = [&](const double iMs) -> double {
		return static_cast<double>(
				std::ranges::count_if(cpu, [&](const double iNs) -> bool { return iNs > iMs * 1e6; }));
	};
	ioRunner.metric(iName + "/frames", static_cast<double>(cpu.size()), "frames");
	ioRunner.metric(iName + "/main_p50_ms", percentile(cpu, 0.5) / 1e6, "ms (main-thread CPU per frame)");
	ioRunner.metric(iName + "/main_p99_ms", percentile(cpu, 0.99) / 1e6, "ms");
	ioRunner.metric(iName + "/main_max_ms", percentile(cpu, 1.0) / 1e6, "ms");
	ioRunner.metric(iName + "/frames_over_2ms", over(2.0), "frames");
	ioRunner.metric(iName + "/frames_over_5ms", over(5.0), "frames");
	ioRunner.metric(iName + "/main_total_ms", total / 1e6, "ms (main-thread CPU, whole scenario)");
	ioRunner.metric(iName + "/process_cpu_ms", iLog.processNs / 1e6, "ms (all threads)");
	ioRunner.metric(iName + "/wall_ms", iLog.wallNs / 1e6, "ms");
	const auto& world = iScene.voxel.getComponent<scene::component::VoxelWorld>();
	ioRunner.metric(iName + "/resident_chunks", static_cast<double>(world.world.chunkCount()), "chunks");
	const auto stats = renderer::RendererVoxel::getStatistics(voxelCache());
	ioRunner.metric(iName + "/cached_meshes", static_cast<double>(stats.cachedMeshCount), "meshes");
	ioRunner.metric(iName + "/uploaded_meshes", static_cast<double>(stats.uploadedMeshCount - iStart.uploadedMeshCount),
					"meshes");
	ioRunner.metric(iName + "/discarded_results",
					static_cast<double>(stats.discardedMeshCount - iStart.discardedMeshCount), "results");
	ioRunner.metric(iName + "/meshing_cpu_ms", static_cast<double>(stats.meshingNs - iStart.meshingNs) / 1e6,
					"ms (meshing work, any thread)");
	const uint64_t latencies = stats.latencyCount - iStart.latencyCount;
	if (latencies > 0) {
		ioRunner.metric(iName + "/latency_mean_ms",
						static_cast<double>(stats.latencyNsTotal - iStart.latencyNsTotal) /
								static_cast<double>(latencies) / 1e6,
						"ms (chunk needs a mesh -> uploaded)");
		ioRunner.metric(iName + "/latency_max_ms", static_cast<double>(stats.latencyNsMax) / 1e6, "ms (since init)");
	}
}

template<typename Frame>
void runFrames(FrameLog& ioLog, const uint32_t iFrames, Frame&& iFrame) {
	auto& scheduler = app::Application::get().getTaskScheduler();
	const auto step = makeStep();
	auto deadline = std::chrono::steady_clock::now();
	for (uint32_t f = 0; f < iFrames; ++f) {
		const double start = threadCpuNs();
		iFrame(f);
		scheduler.frame(step);
		ioLog.cpuNs.push_back(threadCpuNs() - start);
		deadline += k_FramePeriod;
		std::this_thread::sleep_until(deadline);
	}
}

// Run frames until no chunk generation is pending and the cached mesh count stopped moving.
template<typename Frame>
void runUntilSettled(FrameLog& ioLog, const StreamingScene& iScene, Frame&& iFrame) {
	auto& scheduler = app::Application::get().getTaskScheduler();
	const auto step = makeStep();
	const auto& world = iScene.voxel.getComponent<scene::component::VoxelWorld>();
	uint32_t stable = 0;
	uint32_t lastMeshes = 0;
	auto deadline = std::chrono::steady_clock::now();
	for (uint32_t f = 0; f < k_MaxSettleFrames && stable < k_SettledFrames; ++f) {
		const double start = threadCpuNs();
		iFrame(f);
		scheduler.frame(step);
		ioLog.cpuNs.push_back(threadCpuNs() - start);
		deadline += k_FramePeriod;
		std::this_thread::sleep_until(deadline);
		const auto stats = renderer::RendererVoxel::getStatistics(voxelCache());
		const uint32_t meshes = stats.cachedMeshCount;
		const bool idle = world.pendingChunks.empty() && stats.pendingJobCount == 0 && stats.readyMeshCount == 0;
		stable = idle && meshes == lastMeshes ? stable + 1 : 0;
		lastMeshes = meshes;
	}
}

void setVoxelStack() {
	renderer::RendererStackConfig config;
	config.entries.push_back({.typeKey = "Renderer2D", .name = "world", .defaultConfig = {}});
	config.entries.push_back({.typeKey = "RendererVoxel", .name = "voxel_world", .defaultConfig = {}});
	renderer::Renderer::setRenderStack(
			renderer::RenderStack::buildFromConfig(config, renderer::EnabledRenderersConfig{}));
	voxelCache().clear();
}

void runRuntimeWalk(Runner& ioRunner) {
	const std::string name = "voxel/streaming/runtime_walk";
	if (!ioRunner.wants(name))
		return;
	setVoxelStack();
	auto streaming = makeStreamingScene();
	streaming.scene->onStartRuntime();
	const auto step = makeStep();
	const auto loadStats = renderer::RendererVoxel::getStatistics(voxelCache());
	FrameLog load;
	const auto loadStart = std::chrono::steady_clock::now();
	const double loadProcess = processCpuNs();
	runUntilSettled(load, streaming, [&](uint32_t) -> void { streaming.scene->onUpdateRuntime(step); });
	load.processNs = processCpuNs() - loadProcess;
	load.wallNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - loadStart).count();
	report(ioRunner, name + "/initial_load", load, streaming, loadStats);

	// Walk 32 chunks along +X at one chunk every 16 frames (60 blocks per second at 60 fps), then settle.
	constexpr uint32_t walkFrames = 32 * 16;
	auto& translation = streaming.camera.getComponent<scene::component::Transform>().transform.translation();
	const auto walkStats = renderer::RendererVoxel::getStatistics(voxelCache());
	FrameLog walk;
	const auto walkStart = std::chrono::steady_clock::now();
	const double walkProcess = processCpuNs();
	runFrames(walk, walkFrames, [&](uint32_t) -> void {
		translation.x() += k_ChunkSize / 16.f;
		streaming.scene->onUpdateRuntime(step);
	});
	runUntilSettled(walk, streaming, [&](uint32_t) -> void { streaming.scene->onUpdateRuntime(step); });
	walk.processNs = processCpuNs() - walkProcess;
	walk.wallNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - walkStart).count();
	report(ioRunner, name + "/walk", walk, streaming, walkStats);
	streaming.scene->onEndRuntime();
	voxelCache().clear();
	renderer::Renderer::setRenderStack(renderer::RenderStack{});
}

void runEditorLoad(Runner& ioRunner) {
	const std::string name = "voxel/streaming/editor_load";
	if (!ioRunner.wants(name))
		return;
	setVoxelStack();
	auto streaming = makeStreamingScene();
	renderer::CameraEditor camera{45.f, 1.778f, 0.1f, 1000.f};
	camera.setViewportSize({1280, 720});
	const auto step = makeStep();
	const auto loadStats = renderer::RendererVoxel::getStatistics(voxelCache());
	FrameLog load;
	const auto start = std::chrono::steady_clock::now();
	const double process = processCpuNs();
	runUntilSettled(load, streaming, [&](uint32_t) -> void { streaming.scene->onUpdateEditor(step, camera); });
	load.processNs = processCpuNs() - process;
	load.wallNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
	report(ioRunner, name, load, streaming, loadStats);
	voxelCache().clear();
	renderer::Renderer::setRenderStack(renderer::RenderStack{});
}

}// namespace

void runVoxelStreamingBenches(Runner& ioRunner) {
	if (!ioRunner.wants("voxel/streaming"))
		return;
	runRuntimeWalk(ioRunner);
	runEditorLoad(ioRunner);
}

}// namespace owl::bench
