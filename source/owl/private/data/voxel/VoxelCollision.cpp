/**
 * @file VoxelCollision.cpp
 * @author Silmaen
 * @date 08/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/VoxelCollision.h"

#include <algorithm>
#include <cmath>

namespace owl::data::voxel {

namespace {
constexpr float g_Epsilon = 1e-4f;

auto overlapsSolid(const SolidPredicate& iSolid, const math::vec3& iCenter, const math::vec3& iHalf) -> bool {
	const float minX = iCenter.x() - iHalf.x();
	const float minY = iCenter.y() - iHalf.y();
	const float minZ = iCenter.z() - iHalf.z();
	const float maxX = iCenter.x() + iHalf.x();
	const float maxY = iCenter.y() + iHalf.y();
	const float maxZ = iCenter.z() + iHalf.z();
	const auto x0 = static_cast<int32_t>(std::floor(minX + g_Epsilon));
	const auto x1 = static_cast<int32_t>(std::floor(maxX - g_Epsilon));
	const auto y0 = static_cast<int32_t>(std::floor(minY + g_Epsilon));
	const auto y1 = static_cast<int32_t>(std::floor(maxY - g_Epsilon));
	const auto z0 = static_cast<int32_t>(std::floor(minZ + g_Epsilon));
	const auto z1 = static_cast<int32_t>(std::floor(maxZ - g_Epsilon));
	for (int32_t y = y0; y <= y1; ++y)
		for (int32_t z = z0; z <= z1; ++z)
			for (int32_t x = x0; x <= x1; ++x)
				if (iSolid(x, y, z))
					return true;
	return false;
}
}// namespace

auto moveAabb(const SolidPredicate& iSolid, const math::vec3& iCenter, const math::vec3& iHalfExtents,
			  const math::vec3& iDelta) -> AabbMoveResult {
	AabbMoveResult result{.position = iCenter, .onGround = false, .hitCeiling = false};
	// Sub-step shorter than a block so a fast move can't tunnel; each sub-step resolves one axis (gives wall-sliding).
	constexpr float kMaxStep = 0.9f;
	const float longest = std::max({std::abs(iDelta.x()), std::abs(iDelta.y()), std::abs(iDelta.z())});
	const auto steps = std::max(1, static_cast<int32_t>(std::ceil(longest / kMaxStep)));
	const math::vec3 step = iDelta / static_cast<float>(steps);
	for (int32_t s = 0; s < steps; ++s) {
		for (size_t axis = 0; axis < 3; ++axis) {
			if (step[axis] == 0.f)
				continue;
			result.position[axis] += step[axis];
			if (!overlapsSolid(iSolid, result.position, iHalfExtents))
				continue;
			const float half = iHalfExtents[axis];
			if (step[axis] > 0.f) {
				result.position[axis] = std::floor(result.position[axis] + half) - half - g_Epsilon;
				if (axis == 1U)
					result.hitCeiling = true;
			} else {
				result.position[axis] = std::floor(result.position[axis] - half) + 1.f + half + g_Epsilon;
				if (axis == 1U)
					result.onGround = true;
			}
		}
	}
	return result;
}

}// namespace owl::data::voxel
