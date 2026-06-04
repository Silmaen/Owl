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

#include <unordered_map>

namespace owl::renderer {

namespace {
constexpr int32_t k_ChunkSize = static_cast<int32_t>(data::voxel::g_ChunkSize);

struct EntityMeshes {
	std::unordered_map<uint64_t, Renderer3D::MeshHandle> chunks;
};

struct InternalData {
	std::unordered_map<int, EntityMeshes> entities;
	std::unordered_map<std::string, shared<gpu::Texture2D>> textures;
};

shared<InternalData> g_Data;

// Gated off until the depth-aware-renderer PR wires the 3D draw into the Vulkan frame (it currently hangs the GPU).
bool g_GpuDrawEnabled = false;

auto packKey(const math::vec3i& iCoord) -> uint64_t {
	const auto enc = [](const int32_t iValue) -> uint64_t {
		return static_cast<uint64_t>(static_cast<int64_t>(iValue) + (1 << 20)) & 0x1FFFFF;
	};
	return enc(iCoord.x()) | (enc(iCoord.y()) << 21) | (enc(iCoord.z()) << 42);
}

auto resolveTexture(const std::string& iPath) -> shared<gpu::Texture2D> {
	if (const auto it = g_Data->textures.find(iPath); it != g_Data->textures.end())
		return it->second;
	auto texture = gpu::Texture2D::createFromSerialized("nam:" + iPath);
	if (texture)
		texture->setFilterMode(gpu::FilterMode::Nearest);
	g_Data->textures.emplace(iPath, texture);
	return texture;
}

auto buildChunkMesh(const data::voxel::Chunk& iChunk, const data::voxel::BlockRegistry& iRegistry,
					const data::voxel::VoxelWorld& iWorld, const math::vec3i& iCoord) -> Renderer3D::MeshHandle {
	const auto neighbor = [&iWorld, iCoord](const int32_t iX, const int32_t iY,
											const int32_t iZ) -> data::voxel::BlockId {
		return iWorld.getBlock(math::vec3i{iCoord.x() * k_ChunkSize + iX, iCoord.y() * k_ChunkSize + iY,
										   iCoord.z() * k_ChunkSize + iZ});
	};
	const data::voxel::ChunkMesh mesh = data::voxel::ChunkMesher::mesh(iChunk, iRegistry, neighbor);
	if (mesh.isEmpty())
		return nullptr;
	std::vector<Mesh3DVertex> vertices;
	vertices.reserve(mesh.vertices.size());
	for (const auto& v: mesh.vertices)
		vertices.push_back(Mesh3DVertex{.position = v.position,
										.normal = v.normal,
										.uv = v.uv,
										.textureIndex = v.textureIndex + 1});
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
	if (g_Data) {
		g_Data->entities.clear();
		g_Data->textures.clear();
	}
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

void RendererVoxel::drawVoxelWorld(scene::component::VoxelWorld& ioComponent, const math::Transform& iWorldTransform,
								   const int iEntityId) {
	OWL_PROFILE_FUNCTION()

	if (!g_Data || !g_GpuDrawEnabled)
		return;
	std::vector<shared<gpu::Texture2D>> textures;
	textures.reserve(ioComponent.blockTextures.size());
	for (const auto& path: ioComponent.blockTextures) textures.push_back(resolveTexture(path.generic_string()));

	auto& cache = g_Data->entities[iEntityId];
	const math::mat4 worldMat = iWorldTransform();
	for (const auto& coord: ioComponent.world.chunkCoordinates()) {
		const auto chunk = ioComponent.world.getChunk(coord);
		if (!chunk || chunk->isEmpty())
			continue;
		const uint64_t key = packKey(coord);
		auto it = cache.chunks.find(key);
		if (it == cache.chunks.end() || chunk->isDirty()) {
			cache.chunks[key] = buildChunkMesh(*chunk, ioComponent.registry, ioComponent.world, coord);
			chunk->markClean();
			it = cache.chunks.find(key);
		}
		if (!it->second)
			continue;
		const math::vec3 origin{static_cast<float>(coord.x() * k_ChunkSize),
								static_cast<float>(coord.y() * k_ChunkSize),
								static_cast<float>(coord.z() * k_ChunkSize)};
		Renderer3D::drawMesh(it->second, math::translate(worldMat, origin), textures);
	}
}

}// namespace owl::renderer
