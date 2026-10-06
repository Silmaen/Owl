/**
 * @file ChunkNeighborhood.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/ChunkNeighborhood.h"

#include "data/voxel/VoxelWorld.h"

#include <cstdint>

namespace owl::data::voxel {

namespace {
constexpr int32_t k_Size = static_cast<int32_t>(g_ChunkSize);
constexpr int32_t k_Padded = static_cast<int32_t>(g_PaddedChunkSize);
constexpr uint32_t k_PaddedVolume = g_PaddedChunkSize * g_PaddedChunkSize * g_PaddedChunkSize;

auto paddedIndex(const int32_t iX, const int32_t iY, const int32_t iZ) -> size_t {
	const auto padded = static_cast<size_t>(k_Padded);
	return (static_cast<size_t>(iY + 1) * padded + static_cast<size_t>(iZ + 1)) * padded + static_cast<size_t>(iX + 1);
}

// Local range of a neighbour that falls inside the padded grid: its last cell, all of it, or its first cell.
auto localRange(const int32_t iOffset) -> std::pair<int32_t, int32_t> {
	if (iOffset < 0)
		return {k_Size - 1, k_Size - 1};
	if (iOffset > 0)
		return {0, 0};
	return {0, k_Size - 1};
}

void copyNeighbor(const Chunk& iChunk, const int32_t iDx, const int32_t iDy, const int32_t iDz,
				  std::vector<BlockId>& oPadded) {
	const auto [x0, x1] = localRange(iDx);
	const auto [y0, y1] = localRange(iDy);
	const auto [z0, z1] = localRange(iDz);
	const auto& blocks = iChunk.blocks();
	for (int32_t y = y0; y <= y1; ++y) {
		for (int32_t z = z0; z <= z1; ++z) {
			for (int32_t x = x0; x <= x1; ++x)
				oPadded[paddedIndex(x + iDx * k_Size, y + iDy * k_Size, z + iDz * k_Size)] = blocks[localIndex(
						static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z))];
		}
	}
}
}// namespace

ChunkNeighborhood::ChunkNeighborhood() : m_padded(k_PaddedVolume, g_AirBlock) {}

auto ChunkNeighborhood::capture(const VoxelWorld& iWorld, const math::vec3i& iCoord) -> ChunkNeighborhood {
	ChunkNeighborhood result;
	for (int32_t dy = -1; dy <= 1; ++dy) {
		for (int32_t dz = -1; dz <= 1; ++dz) {
			for (int32_t dx = -1; dx <= 1; ++dx) {
				const auto chunk = iWorld.getChunk(math::vec3i{iCoord.x() + dx, iCoord.y() + dy, iCoord.z() + dz});
				if (!chunk)
					continue;
				if (dx == 0 && dy == 0 && dz == 0) {
					result.m_revision = chunk->getRevision();
					result.m_chunk = *chunk;
				}
				copyNeighbor(*chunk, dx, dy, dz, result.m_padded);
			}
		}
	}
	result.m_chunk.setCoord(iCoord);
	return result;
}

auto ChunkNeighborhood::getBlock(const int32_t iX, const int32_t iY, const int32_t iZ) const -> BlockId {
	if (iX < -1 || iY < -1 || iZ < -1 || iX > k_Size || iY > k_Size || iZ > k_Size)
		return g_AirBlock;
	return m_padded[paddedIndex(iX, iY, iZ)];
}

}// namespace owl::data::voxel
