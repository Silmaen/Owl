/**
 * @file VoxelRaycast.cpp
 * @author Silmaen
 * @date 10/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/voxel/VoxelRaycast.h"

#include <array>
#include <cmath>
#include <limits>

namespace owl::data::voxel {

auto raycastVoxel(const BlockPredicate& iHit, const math::vec3& iOrigin, const math::vec3& iDirection,
				  const float iMaxDistance) -> std::optional<VoxelRayHit> {
	const std::array<float, 3> dir{iDirection.x(), iDirection.y(), iDirection.z()};
	const float lengthSquared = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
	if (lengthSquared < 1e-12f)
		return std::nullopt;
	const float invLength = 1.f / std::sqrt(lengthSquared);
	const std::array<float, 3> unit{dir[0] * invLength, dir[1] * invLength, dir[2] * invLength};
	const std::array<float, 3> origin{iOrigin.x(), iOrigin.y(), iOrigin.z()};

	std::array<int32_t, 3> voxel{static_cast<int32_t>(std::floor(origin[0])),
								 static_cast<int32_t>(std::floor(origin[1])),
								 static_cast<int32_t>(std::floor(origin[2]))};
	constexpr float infinity = std::numeric_limits<float>::infinity();
	std::array<int32_t, 3> step{0, 0, 0};
	std::array<float, 3> tMax{infinity, infinity, infinity};
	std::array<float, 3> tDelta{infinity, infinity, infinity};
	for (size_t axis = 0; axis < 3; ++axis) {
		if (unit[axis] > 0.f) {
			step[axis] = 1;
			tMax[axis] = (static_cast<float>(voxel[axis] + 1) - origin[axis]) / unit[axis];
			tDelta[axis] = 1.f / unit[axis];
		} else if (unit[axis] < 0.f) {
			step[axis] = -1;
			tMax[axis] = (static_cast<float>(voxel[axis]) - origin[axis]) / unit[axis];
			tDelta[axis] = -1.f / unit[axis];
		}
	}

	if (iHit(voxel[0], voxel[1], voxel[2]))
		return VoxelRayHit{.block = {voxel[0], voxel[1], voxel[2]}, .normal = {0, 0, 0}};

	float travelled = 0.f;
	while (travelled <= iMaxDistance) {
		size_t axis = 0;
		if (tMax[1] < tMax[0])
			axis = 1;
		if (tMax[2] < tMax[axis])
			axis = 2;
		voxel[axis] += step[axis];
		travelled = tMax[axis];
		tMax[axis] += tDelta[axis];
		if (travelled > iMaxDistance)
			break;
		if (iHit(voxel[0], voxel[1], voxel[2])) {
			std::array<int32_t, 3> normal{0, 0, 0};
			normal[axis] = -step[axis];
			return VoxelRayHit{.block = {voxel[0], voxel[1], voxel[2]}, .normal = {normal[0], normal[1], normal[2]}};
		}
	}
	return std::nullopt;
}

}// namespace owl::data::voxel
