/**
 * @file VoxelWorld.h
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Chunk.h"
#include "math/vectors.h"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace owl::data::voxel {

/**
 * @brief
 *  A sparse, unbounded grid of chunks addressed by world block coordinates.
 *
 * The world owns its chunks (`shared<Chunk>`) in a hash map keyed by chunk
 * coordinate. Reads of absent chunks return air without allocating; writes
 * create the containing chunk on demand. The world holds no rendering or
 * collision state — it is the authoritative block store that the mesher,
 * renderer and gameplay layers query.
 */
class OWL_API VoxelWorld final {
public:
	VoxelWorld() = default;

	~VoxelWorld() = default;

	VoxelWorld(const VoxelWorld&) = default;

	VoxelWorld(VoxelWorld&&) = default;

	auto operator=(const VoxelWorld&) -> VoxelWorld& = default;

	auto operator=(VoxelWorld&&) -> VoxelWorld& = default;

	/**
	 * @brief
	 *  Read the block at world coordinates.
	 * @param[in] iWorld World block position.
	 * @return The block id, or `g_AirBlock` if the containing chunk is absent.
	 */
	[[nodiscard]] auto getBlock(const math::vec3i& iWorld) const -> BlockId;

	/**
	 * @brief
	 *  Write the block at world coordinates, creating the chunk if needed.
	 * @param[in] iWorld World block position.
	 * @param[in] iBlock The block id to store.
	 */
	void setBlock(const math::vec3i& iWorld, BlockId iBlock);

	/**
	 * @brief
	 *  Get the chunk at a chunk coordinate without creating it.
	 * @param[in] iCoord The chunk coordinate.
	 * @return The chunk, or `nullptr` if it does not exist.
	 */
	[[nodiscard]] auto getChunk(const math::vec3i& iCoord) const -> shared<Chunk>;

	/**
	 * @brief
	 *  Get the chunk at a chunk coordinate, creating an all-air chunk if absent.
	 * @param[in] iCoord The chunk coordinate.
	 * @return The existing or newly created chunk (never null).
	 */
	auto getOrCreateChunk(const math::vec3i& iCoord) -> shared<Chunk>;

	/**
	 * @brief
	 *  Whether a chunk exists at a chunk coordinate.
	 * @param[in] iCoord The chunk coordinate.
	 * @return True if the chunk is present.
	 */
	[[nodiscard]] auto hasChunk(const math::vec3i& iCoord) const -> bool;

	/**
	 * @brief
	 *  Remove the chunk at a chunk coordinate, if present.
	 * @param[in] iCoord The chunk coordinate.
	 * @return True if a chunk was removed, false if none existed.
	 */
	auto removeChunk(const math::vec3i& iCoord) -> bool;

	/**
	 * @brief
	 *  Number of resident chunks.
	 * @return The chunk count.
	 */
	[[nodiscard]] auto chunkCount() const noexcept -> size_t { return m_chunks.size(); }

	/**
	 * @brief
	 *  Drop every chunk, leaving an empty world.
	 */
	void clear() noexcept { m_chunks.clear(); }

	/**
	 * @brief
	 *  The coordinates of every resident chunk (unordered).
	 * @return A snapshot of the chunk coordinates.
	 */
	[[nodiscard]] auto chunkCoordinates() const -> std::vector<math::vec3i>;

	/**
	 * @brief
	 *  Invoke a visitor for every resident chunk.
	 * @param[in] iVisitor Callback receiving each chunk coordinate and chunk.
	 */
	void forEachChunk(const std::function<void(const math::vec3i&, const Chunk&)>& iVisitor) const;

private:
	/// Resident chunks keyed by a packed chunk coordinate (see `packChunkKey`).
	std::unordered_map<uint64_t, shared<Chunk>> m_chunks;
};

}// namespace owl::data::voxel
