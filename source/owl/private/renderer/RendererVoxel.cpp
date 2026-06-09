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

struct EntityMeshes {
	std::unordered_map<uint64_t, Renderer3D::MeshHandle> chunks;
};

struct InternalData {
	std::unordered_map<int, EntityMeshes> entities;
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

auto buildChunkMesh(const data::voxel::Chunk& iChunk, const data::voxel::BlockRegistry& iRegistry,
					const data::voxel::VoxelWorld& iWorld, const math::vec3i& iCoord, const scene::Tileset& iTileset)
		-> Renderer3D::MeshHandle {
	const auto neighbor = [&iWorld, iCoord](const int32_t iX, const int32_t iY,
											const int32_t iZ) -> data::voxel::BlockId {
		return iWorld.getBlock(math::vec3i{iCoord.x() * k_ChunkSize + iX, iCoord.y() * k_ChunkSize + iY,
										   iCoord.z() * k_ChunkSize + iZ});
	};
	const data::voxel::ChunkMesh mesh = data::voxel::ChunkMesher::mesh(iChunk, iRegistry, neighbor);
	if (mesh.isEmpty())
		return nullptr;
	// Bake chunk origin into vertices so chunks share one model (avoids per-draw UBO last-write-wins on Vulkan).
	const math::vec3 origin{static_cast<float>(iCoord.x() * k_ChunkSize), static_cast<float>(iCoord.y() * k_ChunkSize),
							static_cast<float>(iCoord.z() * k_ChunkSize)};
	std::vector<Mesh3DVertex> vertices;
	vertices.reserve(mesh.vertices.size());
	for (const auto& v: mesh.vertices)
		vertices.push_back(Mesh3DVertex{.position = v.position + origin,
										.normal = v.normal,
										.uv = v.uv,
										.textureIndex = k_AtlasSlot,
										.tileRect = tileRectFor(iTileset, static_cast<uint16_t>(v.textureIndex))});
	return Renderer3D::createMesh(vertices, mesh.indices, "voxel");
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
					buildChunkMesh(*chunk, ioComponent.registry, ioComponent.world, coord, *ioComponent.tileset);
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
	std::vector<Renderer3D::MeshHandle> meshes;
	meshes.reserve(cache.chunks.size());
	for (const auto& coord: ioComponent.world.chunkCoordinates()) {
		const auto chunk = ioComponent.world.getChunk(coord);
		if (!chunk || chunk->isEmpty())
			continue;
		if (const auto it = cache.chunks.find(packKey(coord)); it != cache.chunks.end() && it->second)
			meshes.push_back(it->second);
	}
	Renderer3D::drawMeshes(meshes, worldMat, textures);
}

}// namespace owl::renderer
