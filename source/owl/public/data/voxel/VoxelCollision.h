/**
 * @file VoxelCollision.h
 * @author Silmaen
 * @date 08/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <functional>

namespace owl::data::voxel {

/// Predicate answering whether the block at world coordinates `(x, y, z)` is solid (blocks movement).
using SolidPredicate = std::function<bool(int32_t, int32_t, int32_t)>;

/**
 * @brief
 *  Result of moving an axis-aligned bounding box through the voxel grid.
 */
struct AabbMoveResult {
	/// Resolved box centre after collision response.
	math::vec3 position;
	/// True if a solid block was hit directly below while moving down (the box rests on ground).
	bool onGround = false;
	/// True if a solid block was hit directly above while moving up.
	bool hitCeiling = false;
};

/**
 * @brief
 *  Move an AABB by a delta through the voxel grid, resolving collisions per axis.
 *
 * The three axes are applied independently (X, then Y, then Z): each is advanced
 * by its delta and, if the box then overlaps a solid block, snapped back flush
 * against the blocking face. Independent axes give natural wall-sliding and let
 * the caller detect grounding (a downward Y hit) for jumping. Pure and
 * scene-free — the solid set is supplied as a predicate — so it is unit-testable
 * without a renderer or a `VoxelWorld`.
 * @param[in] iSolid Predicate returning true for solid blocks.
 * @param[in] iCenter The box centre before the move.
 * @param[in] iHalfExtents Half the box size on each axis (must be positive).
 * @param[in] iDelta The desired displacement this step.
 * @return The resolved centre plus ground / ceiling contact flags.
 */
[[nodiscard]] OWL_API auto moveAabb(const SolidPredicate& iSolid, const math::vec3& iCenter,
									const math::vec3& iHalfExtents, const math::vec3& iDelta) -> AabbMoveResult;

}// namespace owl::data::voxel
