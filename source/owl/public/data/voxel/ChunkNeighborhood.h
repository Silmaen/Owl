/**
 * @file ChunkNeighborhood.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Chunk.h"
#include "math/vectors.h"

#include <cstdint>
#include <vector>

namespace owl::data::voxel {

class VoxelWorld;

/// Edge length of the padded block grid of a `ChunkNeighborhood` (the chunk plus one cell on every side).
constexpr uint32_t g_PaddedChunkSize = g_ChunkSize + 2;

/**
 * @brief
 *  Immutable copy of one chunk and of the one-cell shell of its 26 neighbours.
 *
 * `ChunkMesher` reads the chunk's blocks and metadata plus every block within one cell of the chunk (face culling
 * across the border, edge and corner cells of ambient occlusion). A neighbourhood captures exactly that on the
 * thread that owns the world, so a worker can mesh it while the world keeps changing. It also records the source
 * chunk revision, which lets a late result be recognised as stale (see `Chunk::getRevision`).
 */
class OWL_API ChunkNeighborhood final {
public:
	/**
	 * @brief
	 *  Default constructor: an all-air neighbourhood at chunk coordinate (0, 0, 0).
	 */
	ChunkNeighborhood();

	~ChunkNeighborhood() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	ChunkNeighborhood(const ChunkNeighborhood&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	ChunkNeighborhood(ChunkNeighborhood&&) = default;

	/**
	 * @brief
	 *  Copy assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(const ChunkNeighborhood&) -> ChunkNeighborhood& = default;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(ChunkNeighborhood&&) -> ChunkNeighborhood& = default;

	/**
	 * @brief
	 *  Copy a chunk and its border shell out of a world.
	 *
	 * Absent chunks (the centre or any neighbour) read as air, exactly like `VoxelWorld::getBlock`. Cost: one copy
	 * of the chunk plus 1 736 border reads, no per-block hash lookup.
	 * @param[in] iWorld The world to read.
	 * @param[in] iCoord The chunk coordinate to capture.
	 * @return The captured neighbourhood.
	 */
	[[nodiscard]] static auto capture(const VoxelWorld& iWorld, const math::vec3i& iCoord) -> ChunkNeighborhood;

	/**
	 * @brief
	 *  The captured chunk (blocks and metadata).
	 * @return The chunk copy.
	 */
	[[nodiscard]] auto getChunk() const noexcept -> const Chunk& { return m_chunk; }

	/**
	 * @brief
	 *  The chunk coordinate that was captured.
	 * @return The chunk coordinate.
	 */
	[[nodiscard]] auto getCoord() const noexcept -> const math::vec3i& { return m_chunk.getCoord(); }

	/**
	 * @brief
	 *  Revision of the source chunk at capture time (0 if the chunk was absent).
	 * @return The captured revision.
	 */
	[[nodiscard]] auto getRevision() const noexcept -> uint64_t { return m_revision; }

	/**
	 * @brief
	 *  Block at chunk-local coordinates, inside the chunk or in its one-cell border.
	 * @param[in] iX Local x in `[-1, g_ChunkSize]`.
	 * @param[in] iY Local y in `[-1, g_ChunkSize]`.
	 * @param[in] iZ Local z in `[-1, g_ChunkSize]`.
	 * @return The block id, or `g_AirBlock` outside the padded range.
	 */
	[[nodiscard]] auto getBlock(int32_t iX, int32_t iY, int32_t iZ) const -> BlockId;

private:
	/// Copy of the captured chunk (its coordinate is the captured one).
	Chunk m_chunk;
	/// Padded `g_PaddedChunkSize` cubed block grid, chunk-local coordinate `c` stored at `c + 1`.
	std::vector<BlockId> m_padded;
	/// Revision of the source chunk at capture time.
	uint64_t m_revision = 0;
};

}// namespace owl::data::voxel
