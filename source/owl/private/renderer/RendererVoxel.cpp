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
#include "scene/component/VoxelWorld.h"

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

}// namespace

struct VoxelMeshCache::Data {
	std::unordered_map<int, EntityMeshes> entities;
	shared<MeshSink> sink = mkShared<MeshSink>();
	std::vector<MeshResult> drained;
	std::vector<Candidate> candidates;
	uint32_t jobsInFlight = 0;
	uint32_t uploadedThisFrame = 0;
	Clock::duration uploadTimeThisFrame{};
	uint64_t prepareStamp = 0;
	RendererVoxel::Statistics counters;
};

namespace {

using CacheData = VoxelMeshCache::Data;

// Per-frame draw state of the renderer (one camera per frame, whatever the cache).
struct FrameData {
	std::vector<Renderer3D::MeshHandle> opaqueDraws;
	std::vector<std::pair<float, Renderer3D::MeshHandle>> transparentDraws;
	std::vector<Renderer3D::MeshHandle> sortedTransparent;
	math::vec3 cameraPosition{0.f, 0.f, 0.f};
	math::mat4 viewProjection = math::identity<float, 4>();
	uint32_t drawnMeshCount = 0;
};

auto frameData() -> FrameData& {
	static FrameData data;
	return data;
}

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

auto upload(const CpuMesh& iMesh, const gpu::PipelineState& iState) -> Renderer3D::MeshHandle {
	if (iMesh.indices.empty())
		return nullptr;
	return Renderer3D::createMesh(iMesh.vertices, iMesh.indices, "voxel", iState);
}

auto hasGeometry(const MeshResult& iResult) -> bool {
	return !iResult.opaque.indices.empty() || !iResult.transparent.indices.empty();
}

void install(CacheData& ioData, ChunkEntry& ioEntry, const MeshResult& iResult) {
	const auto start = Clock::now();
	ioEntry.opaque = upload(iResult.opaque, Renderer3D::opaqueMeshState);
	ioEntry.transparent = upload(iResult.transparent, Renderer3D::transparentMeshState);
	const auto end = Clock::now();
	ioEntry.meshedRevision = iResult.revision;
	auto& counters = ioData.counters;
	if (hasGeometry(iResult)) {
		++ioData.uploadedThisFrame;
		++counters.uploadedMeshCount;
		ioData.uploadTimeThisFrame += end - start;
	}
	if (ioEntry.stale) {
		const uint64_t latency = toNs(end - ioEntry.staleSince);
		counters.latencyNsTotal += latency;
		counters.latencyNsMax = std::max(counters.latencyNsMax, latency);
		++counters.latencyCount;
		ioEntry.stale = false;
	}
}

auto hasUploadBudget(CacheData& ioData) -> bool {
	if (ioData.uploadedThisFrame == 0)
		return true;
	return ioData.uploadedThisFrame < g_Config.maxUploadsPerFrame &&
		   std::chrono::duration<float, std::milli>(ioData.uploadTimeThisFrame).count() < g_Config.uploadBudgetMs;
}

void drainSink(CacheData& ioData) {
	{
		const std::lock_guard<std::mutex> lock{ioData.sink->mutex};
		if (ioData.sink->results.empty())
			return;
		ioData.drained.swap(ioData.sink->results);
	}
	for (auto& result: ioData.drained) {
		if (ioData.jobsInFlight > 0)
			--ioData.jobsInFlight;
		auto& cache = ioData.entities[result.entityId];
		if (const auto it = cache.chunks.find(packKey(result.coord));
			it != cache.chunks.end() && it->second.jobRevision == result.revision)
			it->second.jobRevision = 0;
		cache.ready.push_back(std::move(result));
	}
	ioData.drained.clear();
}

void uploadReady(CacheData& ioData, EntityMeshes& ioCache, const data::voxel::VoxelWorld& iWorld) {
	size_t kept = 0;
	for (size_t i = 0; i < ioCache.ready.size(); ++i) {
		auto& result = ioCache.ready[i];
		const auto it = ioCache.chunks.find(packKey(result.coord));
		const auto chunk = iWorld.getChunk(result.coord);
		if (it == ioCache.chunks.end() || !chunk || chunk->getRevision() != result.revision ||
			it->second.meshedRevision == result.revision) {
			++ioData.counters.discardedMeshCount;
			continue;
		}
		if (hasGeometry(result) && !hasUploadBudget(ioData)) {
			if (kept != i)
				ioCache.ready[kept] = std::move(result);
			++kept;
			continue;
		}
		install(ioData, it->second, result);
	}
	ioCache.ready.resize(kept);
}

// Collect the chunks whose revision has neither a mesh nor a job, and drop the entries of unloaded chunks.
void scanWorld(CacheData& ioData, EntityMeshes& ioCache, const data::voxel::VoxelWorld& iWorld) {
	const uint64_t stamp = ++ioData.prepareStamp;
	const auto now = Clock::now();
	auto& candidates = ioData.candidates;
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

void meshNow(CacheData& ioData, EntityMeshes& ioCache, const scene::component::VoxelWorld& iComponent,
			 const TileGrid& iGrid, const int iEntityId) {
	for (const auto& candidate: ioData.candidates) {
		const auto start = Clock::now();
		const MeshResult result =
				buildResult(data::voxel::ChunkNeighborhood::capture(iComponent.world, candidate.coord),
							iComponent.registry, iGrid, iComponent.ambientOcclusion, iEntityId);
		ioData.counters.meshingNs += toNs(Clock::now() - start);
		install(ioData, ioCache.chunks[candidate.key], result);
	}
}

void dispatchJobs(CacheData& ioData, EntityMeshes& ioCache, const scene::component::VoxelWorld& iComponent,
				  const TileGrid& iGrid, const int iEntityId) {
	auto& scheduler = app::Application::get().getTaskScheduler();
	shared<const data::voxel::BlockRegistry> registry;
	const bool ambientOcclusion = iComponent.ambientOcclusion;
	for (const auto& candidate: ioData.candidates) {
		if (ioData.jobsInFlight >= g_Config.maxJobsInFlight)
			break;
		if (hasPendingNeighbor(iComponent, candidate.coord))
			continue;
		if (!registry)
			registry = mkShared<const data::voxel::BlockRegistry>(iComponent.registry);
		auto neighborhood = mkShared<const data::voxel::ChunkNeighborhood>(
				data::voxel::ChunkNeighborhood::capture(iComponent.world, candidate.coord));
		ioCache.chunks[candidate.key].jobRevision = neighborhood->getRevision();
		++ioData.jobsInFlight;
		scheduler.pushTask(core::task::Task{
				[neighborhood, registry, iGrid, ambientOcclusion, iEntityId, sink = ioData.sink]() -> void {
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

VoxelMeshCache::VoxelMeshCache() : mp_data{mkUniq<Data>()} {}

VoxelMeshCache::~VoxelMeshCache() = default;

void VoxelMeshCache::clear() {
	mp_data->entities.clear();
	const std::lock_guard<std::mutex> lock{mp_data->sink->mutex};
	const auto finished = static_cast<uint32_t>(mp_data->sink->results.size());
	mp_data->jobsInFlight -= std::min(mp_data->jobsInFlight, finished);
	mp_data->sink->results.clear();
}

void RendererVoxel::beginScene(const Camera& iCamera, const VoxelConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	Renderer3D::beginScene(iCamera);
	Renderer3D::setLighting(iConfig.sunDirection, iConfig.ambient);
	const math::vec4 worldPos = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	frameData().cameraPosition = math::vec3{worldPos.x(), worldPos.y(), worldPos.z()};
	frameData().viewProjection = iCamera.getViewProjection();
	frameData().drawnMeshCount = 0;
}

void RendererVoxel::endScene() {
	OWL_PROFILE_FUNCTION()

	Renderer3D::endScene();
}

void RendererVoxel::beginPrepare(VoxelMeshCache& ioCache) {
	ioCache.mp_data->uploadedThisFrame = 0;
	ioCache.mp_data->uploadTimeThisFrame = {};
}

void RendererVoxel::prepareWorld(VoxelMeshCache& ioCache, scene::component::VoxelWorld& ioComponent,
								 const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_GpuDrawEnabled)
		return;
	// No atlas yet (unconfigured, or not resolved before runtime starts): skip silently rather than warn per frame.
	if (!ioComponent.tileset || !ioComponent.tileset->texture)
		return;
	ioComponent.tileset->texture->setFilterMode(gpu::FilterMode::Nearest);

	auto& data = *ioCache.mp_data;
	drainSink(data);
	auto& cache = data.entities[iEntityId];
	uploadReady(data, cache, ioComponent.world);
	scanWorld(data, cache, ioComponent.world);
	if (ioCache.mp_data->candidates.empty())
		return;
	std::ranges::sort(ioCache.mp_data->candidates, {}, &Candidate::distance);
	const TileGrid grid{.columns = ioComponent.tileset->columns,
						.rows = ioComponent.tileset->rows,
						.tileWidth = ioComponent.tileset->tileWidth,
						.tileHeight = ioComponent.tileset->tileHeight};
	if (g_Config.async && app::Application::instanced())
		dispatchJobs(data, cache, ioComponent, grid, iEntityId);
	else
		meshNow(data, cache, ioComponent, grid, iEntityId);
}

void RendererVoxel::drawVoxelWorld(VoxelMeshCache& ioCache, scene::component::VoxelWorld& ioComponent,
								   const math::Transform& iWorldTransform, const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_GpuDrawEnabled)
		return;
	const auto cacheIt = ioCache.mp_data->entities.find(iEntityId);
	if (cacheIt == ioCache.mp_data->entities.end())
		return;
	if (!ioComponent.tileset || !ioComponent.tileset->texture)
		return;
	const std::array<shared<gpu::Texture2D>, 1> textures{ioComponent.tileset->texture};

	auto& cache = cacheIt->second;
	// All chunks share one model + atlas (origin baked in), so batch into one drawMeshes (state set once).
	const math::mat4 worldMat = iWorldTransform();
	const math::vec3 camPos = frameData().cameraPosition;
	const math::vec4 camLocal = inverse(worldMat) * math::vec4{camPos.x(), camPos.y(), camPos.z(), 1.f};
	cache.cameraLocal = math::vec3{camLocal.x(), camLocal.y(), camLocal.z()};
	// Cull per chunk: planes from view-projection * model test each chunk's AABB in chunk-local (origin-baked) space.
	const std::array<math::vec4, 6> planes =
			utils::FrustumCullingPass::extractFrustumPlanes(frameData().viewProjection * worldMat);
	auto& opaque = frameData().opaqueDraws;
	auto& transparent = frameData().transparentDraws;
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
	frameData().drawnMeshCount += static_cast<uint32_t>(opaque.size() + transparent.size());
	Renderer3D::drawMeshes(opaque, worldMat, textures);
	if (!transparent.empty()) {
		// Back-to-front so alpha-over compositing is correct without per-fragment sorting.
		std::ranges::sort(transparent, [](const auto& iA, const auto& iB) -> bool { return iA.first > iB.first; });
		auto& sorted = frameData().sortedTransparent;
		sorted.clear();
		for (auto& mesh: transparent | std::views::values) sorted.push_back(std::move(mesh));
		Renderer3D::drawMeshes(sorted, worldMat, textures);
		sorted.clear();
	}
	opaque.clear();
	transparent.clear();
}

auto RendererVoxel::getStatistics(const VoxelMeshCache& iCache) -> Statistics {
	Statistics stats;
	stats = iCache.mp_data->counters;
	for (const auto& entity: iCache.mp_data->entities | std::views::values) {
		for (const auto& entry: entity.chunks | std::views::values)
			stats.cachedMeshCount += static_cast<uint32_t>(entry.opaque != nullptr) +
									 static_cast<uint32_t>(entry.transparent != nullptr);
		stats.readyMeshCount += static_cast<uint32_t>(entity.ready.size());
	}
	stats.drawnMeshCount = frameData().drawnMeshCount;
	stats.pendingJobCount = iCache.mp_data->jobsInFlight;
	stats.uploadedThisFrame = iCache.mp_data->uploadedThisFrame;
	const std::lock_guard<std::mutex> lock{iCache.mp_data->sink->mutex};
	stats.meshingNs += iCache.mp_data->sink->meshingNs;
	return stats;
}

auto RendererVoxel::getMeshedRevision(const VoxelMeshCache& iCache, const int iEntityId, const math::vec3i& iCoord)
		-> std::optional<uint64_t> {
	const auto entityIt = iCache.mp_data->entities.find(iEntityId);
	if (entityIt == iCache.mp_data->entities.end())
		return std::nullopt;
	const auto it = entityIt->second.chunks.find(packKey(iCoord));
	if (it == entityIt->second.chunks.end() || it->second.meshedRevision == 0)
		return std::nullopt;
	return it->second.meshedRevision;
}

void RendererVoxel::setMeshingConfig(const VoxelMeshingConfig& iConfig) { g_Config = iConfig; }

auto RendererVoxel::getMeshingConfig() -> const VoxelMeshingConfig& { return g_Config; }

}// namespace owl::renderer
