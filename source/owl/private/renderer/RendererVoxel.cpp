/**
 * @file RendererVoxel.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererVoxel.h"

#include "data/voxel/ChunkMesher.h"
#include "math/matrixCreation.h"
#include "renderer/Renderer3D.h"

#include <array>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace owl::renderer {

namespace {
constexpr int32_t k_ChunkSize = static_cast<int32_t>(data::voxel::g_ChunkSize);

struct ChunkMeshes {
	Renderer3D::MeshHandle opaque;
	Renderer3D::MeshHandle transparent;
};

struct EntityMeshes {
	std::unordered_map<uint64_t, ChunkMeshes> chunks;
};

struct InternalData {
	std::unordered_map<int, EntityMeshes> entities;
	math::vec3 cameraPosition{0.f, 0.f, 0.f};
};

shared<InternalData> g_Data;

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

// Atlas cell rect inset by half a texel each side so the shader's frac(uv) tiling never bleeds the neighbour cell.
auto tileRectFor(const scene::Tileset& iTileset, const uint16_t iTileIndex) -> math::vec4 {
	const uint32_t cols = std::max(1u, iTileset.columns);
	const uint32_t rows = std::max(1u, iTileset.rows);
	if (iTileIndex >= cols * rows)
		return math::vec4{0.f, 0.f, 1.f, 1.f};
	const uint32_t col = iTileIndex % cols;
	const uint32_t row = iTileIndex / cols;
	const float halfU = 0.5f / (static_cast<float>(cols) * static_cast<float>(std::max(1u, iTileset.tileWidth)));
	const float halfV = 0.5f / (static_cast<float>(rows) * static_cast<float>(std::max(1u, iTileset.tileHeight)));
	return math::vec4{static_cast<float>(col) / static_cast<float>(cols) + halfU,
					  1.f - static_cast<float>(row + 1) / static_cast<float>(rows) + halfV,
					  1.f / static_cast<float>(cols) - 2.f * halfU, 1.f / static_cast<float>(rows) - 2.f * halfV};
}

auto uploadMesh(const data::voxel::ChunkMesh& iMesh, const math::vec3& iOrigin, const scene::Tileset& iTileset)
		-> Renderer3D::MeshHandle {
	if (iMesh.isEmpty())
		return nullptr;
	std::vector<Mesh3DVertex> vertices;
	vertices.reserve(iMesh.vertices.size());
	for (const auto& v: iMesh.vertices)
		vertices.push_back(Mesh3DVertex{.position = v.position + iOrigin,
										.normal = v.normal,
										.uv = v.uv,
										.textureIndex = k_AtlasSlot,
										.tileRect = tileRectFor(iTileset, static_cast<uint16_t>(v.textureIndex)),
										.ao = v.ao});
	return Renderer3D::createMesh(vertices, iMesh.indices, "voxel");
}

auto buildChunkMeshes(const data::voxel::Chunk& iChunk, const data::voxel::BlockRegistry& iRegistry,
					  const data::voxel::VoxelWorld& iWorld, const math::vec3i& iCoord, const scene::Tileset& iTileset)
		-> ChunkMeshes {
	const auto neighbor = [&iWorld, iCoord](const int32_t iX, const int32_t iY,
											const int32_t iZ) -> data::voxel::BlockId {
		return iWorld.getBlock(math::vec3i{iCoord.x() * k_ChunkSize + iX, iCoord.y() * k_ChunkSize + iY,
										   iCoord.z() * k_ChunkSize + iZ});
	};
	const data::voxel::ChunkMeshSet set = data::voxel::ChunkMesher::meshByKind(iChunk, iRegistry, neighbor);
	// Bake chunk origin into vertices so chunks share one model (avoids per-draw UBO last-write-wins on Vulkan).
	const math::vec3 origin{static_cast<float>(iCoord.x() * k_ChunkSize), static_cast<float>(iCoord.y() * k_ChunkSize),
							static_cast<float>(iCoord.z() * k_ChunkSize)};
	return ChunkMeshes{.opaque = uploadMesh(set.opaque, origin, iTileset),
					   .transparent = uploadMesh(set.transparent, origin, iTileset)};
}

auto chunkCenter(const math::vec3i& iCoord) -> math::vec3 {
	const float half = static_cast<float>(k_ChunkSize) * 0.5f;
	return math::vec3{static_cast<float>(iCoord.x() * k_ChunkSize) + half,
					  static_cast<float>(iCoord.y() * k_ChunkSize) + half,
					  static_cast<float>(iCoord.z() * k_ChunkSize) + half};
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
	if (g_Data)
		g_Data->entities.clear();
}

void RendererVoxel::beginScene(const Camera& iCamera, const VoxelConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	Renderer3D::beginScene(iCamera);
	Renderer3D::setLighting(iConfig.sunDirection, iConfig.ambient);
	if (g_Data) {
		const math::vec4 worldPos = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
		g_Data->cameraPosition = math::vec3{worldPos.x(), worldPos.y(), worldPos.z()};
	}
}

void RendererVoxel::endScene() {
	OWL_PROFILE_FUNCTION()

	Renderer3D::endScene();
}

void RendererVoxel::prepareWorld(scene::component::VoxelWorld& ioComponent, const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_Data || !g_GpuDrawEnabled)
		return;
	// No atlas yet (unconfigured, or not resolved before runtime starts): skip silently rather than warn per frame.
	if (!ioComponent.tileset || !ioComponent.tileset->texture)
		return;
	ioComponent.tileset->texture->setFilterMode(gpu::FilterMode::Nearest);

	auto& cache = g_Data->entities[iEntityId];
	std::unordered_set<uint64_t> live;
	for (const auto& coord: ioComponent.world.chunkCoordinates()) {
		const auto chunk = ioComponent.world.getChunk(coord);
		if (!chunk || chunk->isEmpty())
			continue;
		const uint64_t key = packKey(coord);
		live.insert(key);
		if (const auto it = cache.chunks.find(key); it == cache.chunks.end() || chunk->isDirty()) {
			cache.chunks[key] =
					buildChunkMeshes(*chunk, ioComponent.registry, ioComponent.world, coord, *ioComponent.tileset);
			chunk->markClean();
		}
	}
	// Drop cached meshes for chunks that were streamed out, so memory stays bounded as the camera moves.
	for (auto it = cache.chunks.begin(); it != cache.chunks.end();)
		it = live.contains(it->first) ? std::next(it) : cache.chunks.erase(it);
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

	const auto& cache = cacheIt->second;
	// All chunks share one model + atlas (origin baked in), so batch into one drawMeshes (state set once).
	const math::mat4 worldMat = iWorldTransform();
	std::vector<Renderer3D::MeshHandle> opaque;
	std::vector<std::pair<float, Renderer3D::MeshHandle>> transparent;
	opaque.reserve(cache.chunks.size());
	const math::vec3 camPos = g_Data->cameraPosition;
	for (const auto& coord: ioComponent.world.chunkCoordinates()) {
		const auto chunk = ioComponent.world.getChunk(coord);
		if (!chunk || chunk->isEmpty())
			continue;
		const auto it = cache.chunks.find(packKey(coord));
		if (it == cache.chunks.end())
			continue;
		if (it->second.opaque)
			opaque.push_back(it->second.opaque);
		if (it->second.transparent) {
			const math::vec3 local = chunkCenter(coord);
			const math::vec4 world = worldMat * math::vec4{local.x(), local.y(), local.z(), 1.f};
			const float dx = world.x() - camPos.x();
			const float dy = world.y() - camPos.y();
			const float dz = world.z() - camPos.z();
			transparent.emplace_back(dx * dx + dy * dy + dz * dz, it->second.transparent);
		}
	}
	Renderer3D::drawMeshes(opaque, worldMat, textures, /*iDepthWrite=*/true);
	if (!transparent.empty()) {
		// Back-to-front so alpha-over compositing is correct without per-fragment sorting.
		std::ranges::sort(transparent, [](const auto& iA, const auto& iB) -> bool { return iA.first > iB.first; });
		std::vector<Renderer3D::MeshHandle> sorted;
		sorted.reserve(transparent.size());
		for (auto& [distance, mesh]: transparent) sorted.push_back(std::move(mesh));
		Renderer3D::drawMeshes(sorted, worldMat, textures, /*iDepthWrite=*/false);
	}
}

}// namespace owl::renderer
