/**
 * @file RendererVoxel.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererVoxel.h"

#include "app/Application.h"
#include "core/task/Scheduler.h"
#include "data/voxel/ChunkMesher.h"
#include "data/voxel/ChunkNeighborhood.h"
#include "math/matrixCreation.h"
#include "renderer/Renderer3D.h"
#include "renderer/utils/FrustumCullingPass.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace owl::renderer {

namespace {
constexpr int32_t k_ChunkSize = static_cast<int32_t>(data::voxel::g_ChunkSize);

using Clock = std::chrono::steady_clock;

// Atlas layout read by the vertex conversion (copied into each job, the tileset itself stays on the main thread).
struct TileGrid {
	uint32_t columns = 1;
	uint32_t rows = 1;
	uint32_t tileWidth = 1;
	uint32_t tileHeight = 1;
};

struct CpuMesh {
	std::vector<Mesh3DVertex> vertices;
	std::vector<uint32_t> indices;
};

struct MeshResult {
	int entityId = 0;
	math::vec3i coord{0, 0, 0};
	uint64_t revision = 0;
	CpuMesh opaque;
	CpuMesh transparent;
};

// Worker to main-thread hand-off: jobs push finished meshes under the mutex, `prepareWorld` drains them.
struct MeshSink {
	std::mutex mutex;
	std::vector<MeshResult> results;
	uint64_t meshingNs = 0;
};

struct ChunkEntry {
	math::vec3i coord{0, 0, 0};
	Renderer3D::MeshHandle opaque;
	Renderer3D::MeshHandle transparent;
	uint64_t meshedRevision = 0;
	uint64_t jobRevision = 0;
	uint64_t seenStamp = 0;
	Clock::time_point staleSince;
	bool stale = false;
};

struct EntityMeshes {
	std::unordered_map<uint64_t, ChunkEntry> chunks;
	std::vector<MeshResult> ready;
	math::vec3 cameraLocal{0.f, 0.f, 0.f};
};

struct Candidate {
	uint64_t key = 0;
	math::vec3i coord{0, 0, 0};
	float distance = 0.f;
};

struct InternalData {
	std::unordered_map<int, EntityMeshes> entities;
	shared<MeshSink> sink = mkShared<MeshSink>();
	std::vector<MeshResult> drained;
	std::vector<Candidate> candidates;
	std::vector<Renderer3D::MeshHandle> opaqueDraws;
	std::vector<std::pair<float, Renderer3D::MeshHandle>> transparentDraws;
	std::vector<Renderer3D::MeshHandle> sortedTransparent;
	math::vec3 cameraPosition{0.f, 0.f, 0.f};
	math::mat4 viewProjection = math::identity<float, 4>();
	uint32_t drawnMeshCount = 0;
	uint32_t jobsInFlight = 0;
	uint32_t uploadedThisFrame = 0;
	Clock::duration uploadTimeThisFrame{};
	uint64_t prepareStamp = 0;
	RendererVoxel::Statistics counters;
};

shared<InternalData> g_Data;

VoxelMeshingConfig g_Config;

// Enabled now that the viewport framebuffer carries a depth attachment and Renderer3D depth-tests its draws.
bool g_GpuDrawEnabled = true;

// Voxel meshes bind the atlas as the single texture (slot 1); slot 0 is Renderer3D's default white texture.
constexpr uint32_t k_AtlasSlot = 1;

auto packKey(const math::vec3i& iCoord) -> uint64_t {
	const auto enc = [](const int32_t iValue) -> uint64_t {
		return static_cast<uint64_t>(static_cast<int64_t>(iValue) + (1 << 20)) & 0x1FFFFF;
	};
	return enc(iCoord.x()) | (enc(iCoord.y()) << 21) | (enc(iCoord.z()) << 42);
}

auto toNs(const Clock::duration iDuration) -> uint64_t {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(iDuration).count());
}

// Atlas cell rect inset by half a texel each side so the shader's frac(uv) tiling never bleeds the neighbour cell.
auto tileRectFor(const TileGrid& iGrid, const uint16_t iTileIndex) -> math::vec4 {
	const uint32_t cols = std::max(1u, iGrid.columns);
	const uint32_t rows = std::max(1u, iGrid.rows);
	if (iTileIndex >= cols * rows)
		return math::vec4{0.f, 0.f, 1.f, 1.f};
	const uint32_t col = iTileIndex % cols;
	const uint32_t row = iTileIndex / cols;
	const float halfU = 0.5f / (static_cast<float>(cols) * static_cast<float>(std::max(1u, iGrid.tileWidth)));
	const float halfV = 0.5f / (static_cast<float>(rows) * static_cast<float>(std::max(1u, iGrid.tileHeight)));
	return math::vec4{static_cast<float>(col) / static_cast<float>(cols) + halfU,
					  1.f - static_cast<float>(row + 1) / static_cast<float>(rows) + halfV,
					  1.f / static_cast<float>(cols) - 2.f * halfU, 1.f / static_cast<float>(rows) - 2.f * halfV};
}

auto toCpuMesh(const data::voxel::ChunkMesh& iMesh, const math::vec3& iOrigin, const TileGrid& iGrid) -> CpuMesh {
	CpuMesh result;
	if (iMesh.isEmpty())
		return result;
	result.vertices.reserve(iMesh.vertices.size());
	for (const auto& v: iMesh.vertices)
		result.vertices.push_back(Mesh3DVertex{.position = v.position + iOrigin,
											   .normal = v.normal,
											   .uv = v.uv,
											   .textureIndex = k_AtlasSlot,
											   .tileRect = tileRectFor(iGrid, static_cast<uint16_t>(v.textureIndex)),
											   .ao = v.ao});
	result.indices = iMesh.indices;
	return result;
}

// Pure CPU work, run on a worker: mesh the captured neighbourhood and convert it to upload-ready vertices.
auto buildResult(const data::voxel::ChunkNeighborhood& iNeighborhood, const data::voxel::BlockRegistry& iRegistry,
				 const TileGrid& iGrid, const bool iAmbientOcclusion, const int iEntityId) -> MeshResult {
	const data::voxel::ChunkMeshSet set =
			data::voxel::ChunkMesher::meshByKind(iNeighborhood, iRegistry, iAmbientOcclusion);
	const math::vec3i& coord = iNeighborhood.getCoord();
	// Bake chunk origin into vertices so chunks share one model (avoids per-draw UBO last-write-wins on Vulkan).
	const math::vec3 origin{static_cast<float>(coord.x() * k_ChunkSize), static_cast<float>(coord.y() * k_ChunkSize),
							static_cast<float>(coord.z() * k_ChunkSize)};
	return MeshResult{.entityId = iEntityId,
					  .coord = coord,
					  .revision = iNeighborhood.getRevision(),
					  .opaque = toCpuMesh(set.opaque, origin, iGrid),
					  .transparent = toCpuMesh(set.transparent, origin, iGrid)};
}

auto upload(const CpuMesh& iMesh) -> Renderer3D::MeshHandle {
	if (iMesh.indices.empty())
		return nullptr;
	return Renderer3D::createMesh(iMesh.vertices, iMesh.indices, "voxel");
}

auto hasGeometry(const MeshResult& iResult) -> bool {
	return !iResult.opaque.indices.empty() || !iResult.transparent.indices.empty();
}

void install(ChunkEntry& ioEntry, const MeshResult& iResult) {
	const auto start = Clock::now();
	ioEntry.opaque = upload(iResult.opaque);
	ioEntry.transparent = upload(iResult.transparent);
	const auto end = Clock::now();
	ioEntry.meshedRevision = iResult.revision;
	auto& counters = g_Data->counters;
	if (hasGeometry(iResult)) {
		++g_Data->uploadedThisFrame;
		++counters.uploadedMeshCount;
		g_Data->uploadTimeThisFrame += end - start;
	}
	if (ioEntry.stale) {
		const uint64_t latency = toNs(end - ioEntry.staleSince);
		counters.latencyNsTotal += latency;
		counters.latencyNsMax = std::max(counters.latencyNsMax, latency);
		++counters.latencyCount;
		ioEntry.stale = false;
	}
}

auto hasUploadBudget() -> bool {
	if (g_Data->uploadedThisFrame == 0)
		return true;
	return g_Data->uploadedThisFrame < g_Config.maxUploadsPerFrame &&
		   std::chrono::duration<float, std::milli>(g_Data->uploadTimeThisFrame).count() < g_Config.uploadBudgetMs;
}

void drainSink() {
	{
		const std::lock_guard<std::mutex> lock{g_Data->sink->mutex};
		if (g_Data->sink->results.empty())
			return;
		g_Data->drained.swap(g_Data->sink->results);
	}
	for (auto& result: g_Data->drained) {
		if (g_Data->jobsInFlight > 0)
			--g_Data->jobsInFlight;
		auto& cache = g_Data->entities[result.entityId];
		if (const auto it = cache.chunks.find(packKey(result.coord));
			it != cache.chunks.end() && it->second.jobRevision == result.revision)
			it->second.jobRevision = 0;
		cache.ready.push_back(std::move(result));
	}
	g_Data->drained.clear();
}

void uploadReady(EntityMeshes& ioCache, const data::voxel::VoxelWorld& iWorld) {
	size_t kept = 0;
	for (size_t i = 0; i < ioCache.ready.size(); ++i) {
		auto& result = ioCache.ready[i];
		const auto it = ioCache.chunks.find(packKey(result.coord));
		const auto chunk = iWorld.getChunk(result.coord);
		if (it == ioCache.chunks.end() || !chunk || chunk->getRevision() != result.revision ||
			it->second.meshedRevision == result.revision) {
			++g_Data->counters.discardedMeshCount;
			continue;
		}
		if (hasGeometry(result) && !hasUploadBudget()) {
			if (kept != i)
				ioCache.ready[kept] = std::move(result);
			++kept;
			continue;
		}
		install(it->second, result);
	}
	ioCache.ready.resize(kept);
}

// Collect the chunks whose revision has neither a mesh nor a job, and drop the entries of unloaded chunks.
void scanWorld(EntityMeshes& ioCache, const data::voxel::VoxelWorld& iWorld) {
	const uint64_t stamp = ++g_Data->prepareStamp;
	const auto now = Clock::now();
	auto& candidates = g_Data->candidates;
	candidates.clear();
	iWorld.forEachChunk([&](const math::vec3i& iCoord, const data::voxel::Chunk& iChunk) -> void {
		const uint64_t key = packKey(iCoord);
		auto& entry = ioCache.chunks[key];
		entry.coord = iCoord;
		entry.seenStamp = stamp;
		const uint64_t revision = iChunk.getRevision();
		if (entry.meshedRevision == revision) {
			entry.stale = false;
			return;
		}
		if (!entry.stale) {
			entry.stale = true;
			entry.staleSince = now;
		}
		if (entry.jobRevision == revision)
			return;
		if (iChunk.isEmpty()) {
			entry.opaque.reset();
			entry.transparent.reset();
			entry.meshedRevision = revision;
			entry.stale = false;
			return;
		}
		const float dx =
				(static_cast<float>(iCoord.x()) + 0.5f) * static_cast<float>(k_ChunkSize) - ioCache.cameraLocal.x();
		const float dy =
				(static_cast<float>(iCoord.y()) + 0.5f) * static_cast<float>(k_ChunkSize) - ioCache.cameraLocal.y();
		const float dz =
				(static_cast<float>(iCoord.z()) + 0.5f) * static_cast<float>(k_ChunkSize) - ioCache.cameraLocal.z();
		candidates.push_back(Candidate{.key = key, .coord = iCoord, .distance = dx * dx + dy * dy + dz * dz});
	});
	std::erase_if(ioCache.chunks, [stamp](const auto& iItem) -> bool { return iItem.second.seenStamp != stamp; });
}

auto hasPendingNeighbor(const scene::component::VoxelWorld& iComponent, const math::vec3i& iCoord) -> bool {
	if (iComponent.pendingChunks.empty())
		return false;
	for (int32_t dy = -1; dy <= 1; ++dy) {
		for (int32_t dz = -1; dz <= 1; ++dz) {
			for (int32_t dx = -1; dx <= 1; ++dx) {
				if (iComponent.pendingChunks.contains(
							packKey(math::vec3i{iCoord.x() + dx, iCoord.y() + dy, iCoord.z() + dz})))
					return true;
			}
		}
	}
	return false;
}

void meshNow(EntityMeshes& ioCache, const scene::component::VoxelWorld& iComponent, const TileGrid& iGrid,
			 const int iEntityId) {
	for (const auto& candidate: g_Data->candidates) {
		const auto start = Clock::now();
		const MeshResult result =
				buildResult(data::voxel::ChunkNeighborhood::capture(iComponent.world, candidate.coord),
							iComponent.registry, iGrid, iComponent.ambientOcclusion, iEntityId);
		g_Data->counters.meshingNs += toNs(Clock::now() - start);
		install(ioCache.chunks[candidate.key], result);
	}
}

void dispatchJobs(EntityMeshes& ioCache, const scene::component::VoxelWorld& iComponent, const TileGrid& iGrid,
				  const int iEntityId) {
	auto& scheduler = app::Application::get().getTaskScheduler();
	shared<const data::voxel::BlockRegistry> registry;
	const bool ambientOcclusion = iComponent.ambientOcclusion;
	for (const auto& candidate: g_Data->candidates) {
		if (g_Data->jobsInFlight >= g_Config.maxJobsInFlight)
			break;
		if (hasPendingNeighbor(iComponent, candidate.coord))
			continue;
		if (!registry)
			registry = mkShared<const data::voxel::BlockRegistry>(iComponent.registry);
		auto neighborhood = mkShared<const data::voxel::ChunkNeighborhood>(
				data::voxel::ChunkNeighborhood::capture(iComponent.world, candidate.coord));
		ioCache.chunks[candidate.key].jobRevision = neighborhood->getRevision();
		++g_Data->jobsInFlight;
		scheduler.pushTask(core::task::Task{
				[neighborhood, registry, iGrid, ambientOcclusion, iEntityId, sink = g_Data->sink]() -> void {
					const auto start = Clock::now();
					MeshResult result = buildResult(*neighborhood, *registry, iGrid, ambientOcclusion, iEntityId);
					const uint64_t elapsed = toNs(Clock::now() - start);
					const std::lock_guard<std::mutex> lock{sink->mutex};
					sink->meshingNs += elapsed;
					sink->results.push_back(std::move(result));
				}});
	}
}
}// namespace

void RendererVoxel::init() {
	OWL_PROFILE_FUNCTION()

	g_Data = mkShared<InternalData>();
}

void RendererVoxel::shutdown() {
	OWL_PROFILE_FUNCTION()

	g_Data.reset();
}

void RendererVoxel::clearCache() {
	if (!g_Data)
		return;
	g_Data->entities.clear();
	const std::lock_guard<std::mutex> lock{g_Data->sink->mutex};
	const auto finished = static_cast<uint32_t>(g_Data->sink->results.size());
	g_Data->jobsInFlight -= std::min(g_Data->jobsInFlight, finished);
	g_Data->sink->results.clear();
}

void RendererVoxel::beginScene(const Camera& iCamera, const VoxelConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	Renderer3D::beginScene(iCamera);
	Renderer3D::setLighting(iConfig.sunDirection, iConfig.ambient);
	if (g_Data) {
		const math::vec4 worldPos = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
		g_Data->cameraPosition = math::vec3{worldPos.x(), worldPos.y(), worldPos.z()};
		g_Data->viewProjection = iCamera.getViewProjection();
		g_Data->drawnMeshCount = 0;
	}
}

void RendererVoxel::endScene() {
	OWL_PROFILE_FUNCTION()

	Renderer3D::endScene();
}

void RendererVoxel::beginPrepare() {
	if (!g_Data)
		return;
	g_Data->uploadedThisFrame = 0;
	g_Data->uploadTimeThisFrame = {};
}

void RendererVoxel::prepareWorld(scene::component::VoxelWorld& ioComponent, const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_Data || !g_GpuDrawEnabled)
		return;
	// No atlas yet (unconfigured, or not resolved before runtime starts): skip silently rather than warn per frame.
	if (!ioComponent.tileset || !ioComponent.tileset->texture)
		return;
	ioComponent.tileset->texture->setFilterMode(gpu::FilterMode::Nearest);

	drainSink();
	auto& cache = g_Data->entities[iEntityId];
	uploadReady(cache, ioComponent.world);
	scanWorld(cache, ioComponent.world);
	if (g_Data->candidates.empty())
		return;
	std::ranges::sort(g_Data->candidates, {}, &Candidate::distance);
	const TileGrid grid{.columns = ioComponent.tileset->columns,
						.rows = ioComponent.tileset->rows,
						.tileWidth = ioComponent.tileset->tileWidth,
						.tileHeight = ioComponent.tileset->tileHeight};
	if (g_Config.async && app::Application::instanced())
		dispatchJobs(cache, ioComponent, grid, iEntityId);
	else
		meshNow(cache, ioComponent, grid, iEntityId);
}

void RendererVoxel::drawVoxelWorld(scene::component::VoxelWorld& ioComponent, const math::Transform& iWorldTransform,
								   const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_Data || !g_GpuDrawEnabled)
		return;
	const auto cacheIt = g_Data->entities.find(iEntityId);
	if (cacheIt == g_Data->entities.end())
		return;
	if (!ioComponent.tileset || !ioComponent.tileset->texture)
		return;
	const std::array<shared<gpu::Texture2D>, 1> textures{ioComponent.tileset->texture};

	auto& cache = cacheIt->second;
	// All chunks share one model + atlas (origin baked in), so batch into one drawMeshes (state set once).
	const math::mat4 worldMat = iWorldTransform();
	const math::vec3 camPos = g_Data->cameraPosition;
	const math::vec4 camLocal = inverse(worldMat) * math::vec4{camPos.x(), camPos.y(), camPos.z(), 1.f};
	cache.cameraLocal = math::vec3{camLocal.x(), camLocal.y(), camLocal.z()};
	// Cull per chunk: planes from view-projection * model test each chunk's AABB in chunk-local (origin-baked) space.
	const std::array<math::vec4, 6> planes =
			utils::FrustumCullingPass::extractFrustumPlanes(g_Data->viewProjection * worldMat);
	auto& opaque = g_Data->opaqueDraws;
	auto& transparent = g_Data->transparentDraws;
	opaque.clear();
	transparent.clear();
	for (const auto& entry: cache.chunks | std::views::values) {
		if (!entry.opaque && !entry.transparent)
			continue;
		const math::vec3i& coord = entry.coord;
		const math::vec3 aabbMin{static_cast<float>(coord.x() * k_ChunkSize),
								 static_cast<float>(coord.y() * k_ChunkSize),
								 static_cast<float>(coord.z() * k_ChunkSize)};
		const math::vec3 aabbMax{aabbMin.x() + static_cast<float>(k_ChunkSize),
								 aabbMin.y() + static_cast<float>(k_ChunkSize),
								 aabbMin.z() + static_cast<float>(k_ChunkSize)};
		if (!utils::FrustumCullingPass::isAabbVisible(planes, aabbMin, aabbMax))
			continue;
		if (entry.opaque)
			opaque.push_back(entry.opaque);
		if (entry.transparent) {
			const float half = static_cast<float>(k_ChunkSize) * 0.5f;
			const math::vec4 world =
					worldMat * math::vec4{aabbMin.x() + half, aabbMin.y() + half, aabbMin.z() + half, 1.f};
			const float dx = world.x() - camPos.x();
			const float dy = world.y() - camPos.y();
			const float dz = world.z() - camPos.z();
			transparent.emplace_back(dx * dx + dy * dy + dz * dz, entry.transparent);
		}
	}
	g_Data->drawnMeshCount += static_cast<uint32_t>(opaque.size() + transparent.size());
	Renderer3D::drawMeshes(opaque, worldMat, textures, /*iDepthWrite=*/true);
	if (!transparent.empty()) {
		// Back-to-front so alpha-over compositing is correct without per-fragment sorting.
		std::ranges::sort(transparent, [](const auto& iA, const auto& iB) -> bool { return iA.first > iB.first; });
		auto& sorted = g_Data->sortedTransparent;
		sorted.clear();
		for (auto& mesh: transparent | std::views::values) sorted.push_back(std::move(mesh));
		Renderer3D::drawMeshes(sorted, worldMat, textures, /*iDepthWrite=*/false);
		sorted.clear();
	}
	opaque.clear();
	transparent.clear();
}

auto RendererVoxel::getStatistics() -> Statistics {
	Statistics stats;
	if (!g_Data)
		return stats;
	stats = g_Data->counters;
	for (const auto& entity: g_Data->entities | std::views::values) {
		for (const auto& entry: entity.chunks | std::views::values)
			stats.cachedMeshCount += static_cast<uint32_t>(entry.opaque != nullptr) +
									 static_cast<uint32_t>(entry.transparent != nullptr);
		stats.readyMeshCount += static_cast<uint32_t>(entity.ready.size());
	}
	stats.drawnMeshCount = g_Data->drawnMeshCount;
	stats.pendingJobCount = g_Data->jobsInFlight;
	stats.uploadedThisFrame = g_Data->uploadedThisFrame;
	const std::lock_guard<std::mutex> lock{g_Data->sink->mutex};
	stats.meshingNs += g_Data->sink->meshingNs;
	return stats;
}

auto RendererVoxel::getMeshedRevision(const int iEntityId, const math::vec3i& iCoord) -> std::optional<uint64_t> {
	if (!g_Data)
		return std::nullopt;
	const auto entityIt = g_Data->entities.find(iEntityId);
	if (entityIt == g_Data->entities.end())
		return std::nullopt;
	const auto it = entityIt->second.chunks.find(packKey(iCoord));
	if (it == entityIt->second.chunks.end() || it->second.meshedRevision == 0)
		return std::nullopt;
	return it->second.meshedRevision;
}

void RendererVoxel::setMeshingConfig(const VoxelMeshingConfig& iConfig) { g_Config = iConfig; }

auto RendererVoxel::getMeshingConfig() -> const VoxelMeshingConfig& { return g_Config; }

}// namespace owl::renderer
