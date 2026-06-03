/**
 * @file ChunkMesher.h
 * @author Silmaen
 * @date 03/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/voxel/Chunk.h"
#include "math/vectors.h"

#include <functional>
#include <vector>

namespace owl::data::voxel {

/**
 * @brief
 *  One vertex of a chunk mesh, in chunk-local block space.
 *
 * Positions are expressed in block units within `[0, g_ChunkSize]`; the chunk's
 * world placement is applied by the renderer through a model transform. UVs are
 * in tile units (a greedy-merged `w × h` quad spans `(0, 0)` to `(w, h)`) so the
 * renderer can tile the per-face atlas texture across the merged quad.
 */
struct VoxelVertex {
	/// Vertex position in chunk-local block space.
	math::vec3 position;
	/// Outward face normal (axis-aligned unit vector).
	math::vec3 normal;
	/// Tile-space texture coordinate (spans `(0, 0)`..`(w, h)` across a merged quad).
	math::vec2 uv;
	/// Atlas tile index for the face, taken from `BlockType::faceTexture`.
	uint32_t textureIndex = 0;
};

/**
 * @brief
 *  CPU geometry produced by meshing a chunk: an indexed triangle list.
 *
 * Holds no GPU resources — uploading to a vertex / index buffer is the
 * renderer's responsibility. Empty when the chunk has no visible faces.
 */
struct ChunkMesh {
	/// Vertices referenced by `indices`.
	std::vector<VoxelVertex> vertices;
	/// Triangle indices (three per triangle, six per quad).
	std::vector<uint32_t> indices;

	/**
	 * @brief
	 *  Whether the mesh has no geometry.
	 * @return True if there are no indices.
	 */
	[[nodiscard]] auto isEmpty() const noexcept -> bool { return indices.empty(); }

	/**
	 * @brief
	 *  Number of triangles in the mesh.
	 * @return The triangle count (`indices.size() / 3`).
	 */
	[[nodiscard]] auto triangleCount() const noexcept -> size_t { return indices.size() / 3; }

	/**
	 * @brief
	 *  Number of quads in the mesh (each quad is two triangles).
	 * @return The quad count (`indices.size() / 6`).
	 */
	[[nodiscard]] auto quadCount() const noexcept -> size_t { return indices.size() / 6; }
};

/**
 * @brief
 *  Builds a `ChunkMesh` from a `Chunk` using greedy meshing with hidden-face culling.
 *
 * A face between a block and its neighbour is emitted only when it is visible —
 * the neighbour is non-opaque and a different block — so interior and shared
 * faces are skipped. Coplanar visible faces of the same block type are merged
 * into the largest possible rectangle (greedy meshing), so a flat or solid
 * region collapses to a handful of quads instead of one per cell.
 */
class OWL_API ChunkMesher final {
public:
	/// Resolves the block id at chunk-local coordinates that may fall outside `[0, g_ChunkSize)` (neighbour lookup).
	using NeighborProvider = std::function<BlockId(int32_t iX, int32_t iY, int32_t iZ)>;

	/**
	 * @brief
	 *  Mesh a chunk, resolving out-of-bounds neighbours through a provider.
	 *
	 * The provider is queried only for coordinates on the one-block border
	 * outside the chunk (to cull faces against adjacent chunks); in-range
	 * coordinates are read from the chunk directly.
	 * @param[in] iChunk The chunk to mesh.
	 * @param[in] iRegistry The block registry resolving opacity and face textures.
	 * @param[in] iNeighbor Provider for border neighbour blocks.
	 * @return The generated mesh (empty when no face is visible).
	 */
	[[nodiscard]] static auto mesh(const Chunk& iChunk, const BlockRegistry& iRegistry,
								   const NeighborProvider& iNeighbor) -> ChunkMesh;

	/**
	 * @brief
	 *  Mesh a chunk in isolation, treating everything outside it as air.
	 * @param[in] iChunk The chunk to mesh.
	 * @param[in] iRegistry The block registry resolving opacity and face textures.
	 * @return The generated mesh (empty when no face is visible).
	 */
	[[nodiscard]] static auto mesh(const Chunk& iChunk, const BlockRegistry& iRegistry) -> ChunkMesh;
};

}// namespace owl::data::voxel
