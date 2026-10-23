/**
 * @file BoxCollision.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/box.h"
#include "math/vectors.h"

#include <span>

namespace owl::data::geometry {

/**
 * @brief
 *  Result of moving an axis-aligned box among fixed boxes.
 */
struct OWL_API BoxMoveResult {
	/// Centre of the box after the move.
	math::vec3 position{0.f, 0.f, 0.f};
	/// Whether the box rests on a box below it.
	bool onGround = false;
	/// Whether the box hit a box above it.
	bool hitCeiling = false;
	/// Whether a horizontal move was stopped by a box.
	bool blocked = false;
};

/**
 * @brief
 *  Move an axis-aligned box through fixed axis-aligned boxes, one axis at a time (vertical first), stopping at their
 *  faces; a horizontal move blocked by a box whose top is at most `iStepHeight` above the bottom of the moving box
 *  climbs onto it when there is room.
 *
 * A box the moving box already overlaps before the move is ignored, so a body placed inside an obstacle walks out.
 * @param[in] iCenter Centre of the moving box.
 * @param[in] iHalfExtents Half size of the moving box.
 * @param[in] iDelta Displacement wanted.
 * @param[in] iObstacles Fixed boxes.
 * @param[in] iStepHeight Highest step climbed, 0 for none.
 * @return Where the box ends and what it touched.
 */
[[nodiscard]] OWL_API auto moveBoxAmong(const math::vec3& iCenter, const math::vec3& iHalfExtents,
										const math::vec3& iDelta, std::span<const math::box3f> iObstacles,
										float iStepHeight = 0.f) -> BoxMoveResult;

}// namespace owl::data::geometry
