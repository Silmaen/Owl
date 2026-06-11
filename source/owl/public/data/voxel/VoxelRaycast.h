/**
 * @file VoxelRaycast.h
 * @author Silmaen
 * @date 10/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <functional>
#include <optional>

namespace owl::data::voxel {

/// Predicate answering whether the block at world coordinates `(x, y, z)` stops the ray (a targetable block).
using BlockPredicate = std::function<bool(int32_t, int32_t, int32_t)>;

/**
 * @brief
 *  The block a voxel ray struck, plus the face it entered through.
 */
struct VoxelRayHit {
	/// World-grid coordinates of the hit block.
	math::vec3i block;
	/// Outward unit normal of the face the ray entered (zero when the ray started inside a block).
	math::vec3i normal;
};

/**
 * @brief
 *  Cast a ray through the voxel grid and return the first block that satisfies the predicate.
 *
 * Uses Amanatides & Woo grid traversal (DDA): the ray visits every cell it
 * crosses in order, so the returned block is the nearest hit. Pure and
 * scene-free — the occupied set is supplied as a predicate — so it is
 * unit-testable without a renderer or a `VoxelWorld`. The `normal` identifies
 * the entered face, so `block + normal` is the empty cell a placement would
 * fill.
 * @param[in] iHit Predicate returning true for blocks that stop the ray.
 * @param[in] iOrigin Ray origin in world space.
 * @param[in] iDirection Ray direction (need not be normalized; a zero vector misses).
 * @param[in] iMaxDistance Maximum distance to travel, in blocks.
 * @return The nearest hit, or `std::nullopt` if no block is struck within range.
 */
[[nodiscard]] OWL_API auto raycastVoxel(const BlockPredicate& iHit, const math::vec3& iOrigin,
										const math::vec3& iDirection, float iMaxDistance) -> std::optional<VoxelRayHit>;

}// namespace owl::data::voxel
