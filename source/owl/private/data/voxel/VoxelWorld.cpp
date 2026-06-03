/**
 * @file VoxelWorld.cpp
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/VoxelWorld.h"

namespace owl::data::voxel {

namespace {
constexpr int32_t k_CoordBias = 1 << 20;
constexpr uint64_t k_CoordMask = 0x1FFFFF;

auto packChunkKey(const math::vec3i& iCoord) -> uint64_t {
	const auto enc = [](const int32_t iValue) -> uint64_t {
		return static_cast<uint64_t>(static_cast<int64_t>(iValue) + k_CoordBias) & k_CoordMask;
	};
	return enc(iCoord.x()) | (enc(iCoord.y()) << 21) | (enc(iCoord.z()) << 42);
}
}// namespace

auto VoxelWorld::getBlock(const math::vec3i& iWorld) const -> BlockId {
	const auto it = m_chunks.find(packChunkKey(worldToChunk(iWorld)));
	if (it == m_chunks.end())
		return g_AirBlock;
	const math::vec3i local = worldToLocal(iWorld);
	return it->second->getBlock(local.x(), local.y(), local.z());
}

void VoxelWorld::setBlock(const math::vec3i& iWorld, const BlockId iBlock) {
	const auto chunk = getOrCreateChunk(worldToChunk(iWorld));
	const math::vec3i local = worldToLocal(iWorld);
	chunk->setBlock(local.x(), local.y(), local.z(), iBlock);
}

auto VoxelWorld::getChunk(const math::vec3i& iCoord) const -> shared<Chunk> {
	const auto it = m_chunks.find(packChunkKey(iCoord));
	return it == m_chunks.end() ? nullptr : it->second;
}

auto VoxelWorld::getOrCreateChunk(const math::vec3i& iCoord) -> shared<Chunk> {
	const uint64_t key = packChunkKey(iCoord);
	if (const auto it = m_chunks.find(key); it != m_chunks.end())
		return it->second;
	auto chunk = mkShared<Chunk>(iCoord);
	m_chunks.emplace(key, chunk);
	return chunk;
}

auto VoxelWorld::hasChunk(const math::vec3i& iCoord) const -> bool { return m_chunks.contains(packChunkKey(iCoord)); }

auto VoxelWorld::removeChunk(const math::vec3i& iCoord) -> bool { return m_chunks.erase(packChunkKey(iCoord)) > 0; }

auto VoxelWorld::chunkCoordinates() const -> std::vector<math::vec3i> {
	std::vector<math::vec3i> coords;
	coords.reserve(m_chunks.size());
	for (const auto& chunk: m_chunks | std::views::values) coords.push_back(chunk->getCoord());
	return coords;
}

void VoxelWorld::forEachChunk(const std::function<void(const math::vec3i&, const Chunk&)>& iVisitor) const {
	for (const auto& chunk: m_chunks | std::views::values) iVisitor(chunk->getCoord(), *chunk);
}

}// namespace owl::data::voxel
