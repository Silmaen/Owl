/**
 * @file Chunk.h
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Block.h"
#include "math/vectors.h"

#include <string>
#include <string_view>
#include <vector>

namespace owl::data::voxel {

/// Edge length of a cubic chunk in blocks (chunks are `g_ChunkSize` cubed).
constexpr uint32_t g_ChunkSize = 16;

/// Number of blocks in one chunk (`g_ChunkSize` cubed).
constexpr uint32_t g_ChunkVolume = g_ChunkSize * g_ChunkSize * g_ChunkSize;

/**
 * @brief
 *  Flatten local block coordinates into a linear chunk storage index.
 *
 * Layout is Y-major then Z then X, so iterating X fastest walks contiguous
 * memory. Coordinates are assumed in range `[0, g_ChunkSize)`.
 * @param[in] iX Local x in `[0, g_ChunkSize)`.
 * @param[in] iY Local y in `[0, g_ChunkSize)`.
 * @param[in] iZ Local z in `[0, g_ChunkSize)`.
 * @return The linear index into the chunk block array.
 */
[[nodiscard]] constexpr auto localIndex(const uint32_t iX, const uint32_t iY, const uint32_t iZ) -> uint32_t {
	return (iY * g_ChunkSize + iZ) * g_ChunkSize + iX;
}

/**
 * @brief
 *  Floored integer division (rounds toward negative infinity).
 *
 * Plain C++ integer division truncates toward zero, which maps negative world
 * coordinates to the wrong chunk; this helper gives the correct tiling for the
 * whole integer range.
 * @param[in] iValue The dividend.
 * @param[in] iDivisor The divisor (assumed strictly positive).
 * @return `floor(iValue / iDivisor)`.
 */
[[nodiscard]] constexpr auto floorDiv(const int32_t iValue, const int32_t iDivisor) -> int32_t {
	const int32_t quotient = iValue / iDivisor;
	const int32_t remainder = iValue % iDivisor;
	return (remainder != 0 && (remainder < 0) != (iDivisor < 0)) ? quotient - 1 : quotient;
}

/**
 * @brief
 *  Convert world block coordinates to the coordinate of the containing chunk.
 * @param[in] iWorld World block position.
 * @return The chunk coordinate (each axis floor-divided by `g_ChunkSize`).
 */
[[nodiscard]] OWL_API auto worldToChunk(const math::vec3i& iWorld) -> math::vec3i;

/**
 * @brief
 *  Convert world block coordinates to local coordinates inside their chunk.
 * @param[in] iWorld World block position.
 * @return The local position, each component in `[0, g_ChunkSize)`.
 */
[[nodiscard]] OWL_API auto worldToLocal(const math::vec3i& iWorld) -> math::vec3i;

/**
 * @brief
 *  A fixed-size cubic block of voxels (`g_ChunkSize` cubed `BlockId` values).
 *
 * A chunk owns nothing but block ids and a dirty flag; metadata (textures,
 * opacity, collision) lives in the shared `BlockRegistry`. The chunk knows its
 * own coordinate purely for debugging and editor display — ownership and
 * neighbour resolution are the `VoxelWorld`'s responsibility.
 */
class OWL_API Chunk final {
public:
	Chunk();

	/**
	 * @brief
	 *  Construct an all-air chunk at a given chunk coordinate.
	 * @param[in] iCoord The chunk coordinate this chunk occupies.
	 */
	explicit Chunk(const math::vec3i& iCoord);

	~Chunk() = default;

	Chunk(const Chunk&) = default;

	Chunk(Chunk&&) = default;

	auto operator=(const Chunk&) -> Chunk& = default;

	auto operator=(Chunk&&) -> Chunk& = default;

	/**
	 * @brief
	 *  Read the block at local coordinates.
	 * @param[in] iX Local x.
	 * @param[in] iY Local y.
	 * @param[in] iZ Local z.
	 * @return The block id, or `g_AirBlock` for out-of-range coordinates.
	 */
	[[nodiscard]] auto getBlock(int32_t iX, int32_t iY, int32_t iZ) const -> BlockId;

	/**
	 * @brief
	 *  Read the packed metadata at local coordinates.
	 * @param[in] iX Local x.
	 * @param[in] iY Local y.
	 * @param[in] iZ Local z.
	 * @return The packed metadata, or `g_DefaultMeta` for out-of-range coordinates.
	 */
	[[nodiscard]] auto getMeta(int32_t iX, int32_t iY, int32_t iZ) const -> PackedMeta;

	/**
	 * @brief
	 *  Write the block (and its metadata) at local coordinates, marking the chunk dirty on change.
	 *
	 * Out-of-range coordinates are silently ignored. The dirty flag is only set
	 * when the stored id or metadata actually changes.
	 * @param[in] iX Local x.
	 * @param[in] iY Local y.
	 * @param[in] iZ Local z.
	 * @param[in] iBlock The block id to store.
	 * @param[in] iMeta The packed metadata to store (defaults to none).
	 */
	void setBlock(int32_t iX, int32_t iY, int32_t iZ, BlockId iBlock, PackedMeta iMeta = g_DefaultMeta);

	/**
	 * @brief
	 *  Write only the packed metadata at local coordinates, marking the chunk dirty on change.
	 * @param[in] iX Local x.
	 * @param[in] iY Local y.
	 * @param[in] iZ Local z.
	 * @param[in] iMeta The packed metadata to store.
	 */
	void setMeta(int32_t iX, int32_t iY, int32_t iZ, PackedMeta iMeta);

	/**
	 * @brief
	 *  Set every block in the chunk to one id and mark it dirty.
	 * @param[in] iBlock The block id to fill with.
	 */
	void fill(BlockId iBlock);

	/**
	 * @brief
	 *  Whether the chunk contains only air.
	 * @return True if no non-air block is present.
	 */
	[[nodiscard]] auto isEmpty() const -> bool;

	/**
	 * @brief
	 *  The chunk coordinate this chunk occupies.
	 * @return The chunk coordinate.
	 */
	[[nodiscard]] auto getCoord() const noexcept -> const math::vec3i& { return m_coord; }

	/**
	 * @brief
	 *  Set the chunk coordinate (does not touch block data or the dirty flag).
	 * @param[in] iCoord The new chunk coordinate.
	 */
	void setCoord(const math::vec3i& iCoord) noexcept { m_coord = iCoord; }

	/**
	 * @brief
	 *  Whether the chunk has unsaved / un-remeshed changes.
	 * @return True if the chunk was modified since the last `markClean`.
	 */
	[[nodiscard]] auto isDirty() const noexcept -> bool { return m_dirty; }

	/**
	 * @brief
	 *  Clear the dirty flag (call after remeshing or persisting the chunk).
	 */
	void markClean() noexcept { m_dirty = false; }

	/**
	 * @brief
	 *  Force the dirty flag on (e.g. after a neighbour edit invalidates the mesh).
	 */
	void markDirty() noexcept { m_dirty = true; }

	/**
	 * @brief
	 *  Direct read access to the linear block array for fast iteration (meshing).
	 * @return The block array, laid out per `localIndex`.
	 */
	[[nodiscard]] auto blocks() const noexcept -> const std::vector<BlockId>& { return m_blocks; }

	/**
	 * @brief
	 *  Direct read access to the linear metadata array for fast iteration (meshing).
	 * @return The packed-metadata array, laid out per `localIndex`.
	 */
	[[nodiscard]] auto metadata() const noexcept -> const std::vector<PackedMeta>& { return m_meta; }

	/**
	 * @brief
	 *  Run-length encode the block data to a compact string.
	 * @return The encoded block data (space-separated `<count>x<id>` runs).
	 */
	[[nodiscard]] auto encode() const -> std::string;

	/**
	 * @brief
	 *  Replace the block data from a run-length encoded string.
	 *
	 * On malformed input the chunk is left filled with air up to the point of
	 * failure. Always clears the dirty flag on return.
	 * @param[in] iEncoded The encoded block data produced by `encode`.
	 * @return True if the decoded run count matched the chunk volume exactly.
	 */
	auto decode(std::string_view iEncoded) -> bool;

private:
	/// Chunk coordinate (for debug / editor display only; not authoritative for ownership).
	math::vec3i m_coord{0, 0, 0};
	/// Linear block storage of `g_ChunkVolume` ids, laid out per `localIndex`.
	std::vector<BlockId> m_blocks;
	/// Linear metadata storage of `g_ChunkVolume` packed words, laid out per `localIndex` (parallel to `m_blocks`).
	std::vector<PackedMeta> m_meta;
	/// True when the chunk changed since the last `markClean`.
	bool m_dirty = false;
};

}// namespace owl::data::voxel
