/**
 * @file BoxCollision.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/geometry/BoxCollision.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::data::geometry {

namespace {

/// Gap left between the moving box and the face it stops at.
constexpr float g_skin = 1e-4f;

auto overlaps(const math::vec3& iCenter, const math::vec3& iHalf, const math::box3f& iBox) -> bool {
	for (size_t axis = 0; axis < 3; ++axis) {
		if (iCenter[axis] + iHalf[axis] <= iBox.min()[axis] || iCenter[axis] - iHalf[axis] >= iBox.max()[axis])
			return false;
	}
	return true;
}

auto overlapsAny(const math::vec3& iCenter, const math::vec3& iHalf, std::span<const math::box3f> iObstacles,
				 const std::vector<bool>& iIgnored) -> bool {
	for (size_t index = 0; index < iObstacles.size(); ++index) {
		if (!iIgnored[index] && overlaps(iCenter, iHalf, iObstacles[index]))
			return true;
	}
	return false;
}

void moveOnce(math::vec3& ioPosition, const math::vec3& iHalfExtents, const math::vec3& iDelta,
			  const std::span<const math::box3f> iObstacles, const std::vector<bool>& iIgnored, const float iStepHeight,
			  BoxMoveResult& ioResult) {
	math::vec3& position = ioPosition;
	const std::vector<bool>& ignored = iIgnored;
	BoxMoveResult& result = ioResult;
	for (const size_t axis: {size_t{1}, size_t{0}, size_t{2}}) {
		if (iDelta[axis] == 0.f)
			continue;
		const math::vec3 before = position;
		position[axis] += iDelta[axis];
		for (size_t index = 0; index < iObstacles.size(); ++index) {
			const auto& box = iObstacles[index];
			if (ignored[index] || !overlaps(position, iHalfExtents, box))
				continue;
			if (axis != 1 && iStepHeight > 0.f) {
				const float lift = box.max().y() - (before.y() - iHalfExtents.y());
				math::vec3 raised = position;
				raised.y() = before.y() + lift + g_skin;
				if (lift > 0.f && lift <= iStepHeight && !overlapsAny(raised, iHalfExtents, iObstacles, ignored)) {
					position = raised;
					continue;
				}
			}
			if (iDelta[axis] > 0.f) {
				position[axis] = box.min()[axis] - iHalfExtents[axis] - g_skin;
				if (axis == 1)
					result.hitCeiling = true;
			} else {
				position[axis] = box.max()[axis] + iHalfExtents[axis] + g_skin;
				if (axis == 1)
					result.onGround = true;
			}
			if (axis != 1)
				result.blocked = true;
		}
	}
}

}// namespace

auto moveBoxAmong(const math::vec3& iCenter, const math::vec3& iHalfExtents, const math::vec3& iDelta,
				  const std::span<const math::box3f> iObstacles, const float iStepHeight) -> BoxMoveResult {
	BoxMoveResult result;
	math::vec3 position = iCenter;
	std::vector<bool> ignored(iObstacles.size(), false);
	for (size_t index = 0; index < iObstacles.size(); ++index)
		ignored[index] = overlaps(position, iHalfExtents, iObstacles[index]);
	// Sub-steps shorter than half the thinnest half extent: a fast box never jumps over a thin wall.
	const float reach = std::max(0.5f * std::min({iHalfExtents.x(), iHalfExtents.y(), iHalfExtents.z()}), 1e-3f);
	const float longest = std::max({std::abs(iDelta.x()), std::abs(iDelta.y()), std::abs(iDelta.z())});
	const auto steps = static_cast<uint32_t>(std::clamp(std::ceil(longest / reach), 1.f, 256.f));
	const math::vec3 step = iDelta * (1.f / static_cast<float>(steps));
	for (uint32_t index = 0; index < steps; ++index) {
		math::vec3 wanted = step;
		if (result.onGround && wanted.y() < 0.f)
			wanted.y() = 0.f;
		if (result.hitCeiling && wanted.y() > 0.f)
			wanted.y() = 0.f;
		moveOnce(position, iHalfExtents, wanted, iObstacles, ignored, iStepHeight, result);
	}
	result.position = position;
	return result;
}

}// namespace owl::data::geometry
