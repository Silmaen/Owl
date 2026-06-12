/**
 * @file VoxelRaycast_test.cpp
 * @author Silmaen
 * @date 10/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/voxel/VoxelRaycast.h>

using namespace owl;
using namespace owl::data::voxel;

namespace {
auto blockAt(const int32_t iX, const int32_t iY, const int32_t iZ) -> BlockPredicate {
	return [iX, iY, iZ](const int32_t x, const int32_t y, const int32_t z) -> bool {
		return x == iX && y == iY && z == iZ;
	};
}
}// namespace

TEST(VoxelRaycast, HitsBlockAheadWithEnteredNormal) {
	const auto hit = raycastVoxel(blockAt(5, 0, 0), math::vec3{0.5f, 0.5f, 0.5f}, math::vec3{1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->block, (math::vec3i{5, 0, 0}));
	EXPECT_EQ(hit->normal, (math::vec3i{-1, 0, 0}));
}

TEST(VoxelRaycast, MissesEmptySpace) {
	const auto hit = raycastVoxel([](int32_t, int32_t, int32_t) -> bool { return false; }, math::vec3{0.5f, 0.5f, 0.5f},
								  math::vec3{1.f, 0.f, 0.f}, 32.f);
	EXPECT_FALSE(hit.has_value());
}

TEST(VoxelRaycast, RespectsMaxDistance) {
	const auto hit = raycastVoxel(blockAt(5, 0, 0), math::vec3{0.5f, 0.5f, 0.5f}, math::vec3{1.f, 0.f, 0.f}, 3.f);
	EXPECT_FALSE(hit.has_value());
}

TEST(VoxelRaycast, NormalPointsUpWhenLookingDown) {
	const auto hit = raycastVoxel(blockAt(0, 0, 0), math::vec3{0.5f, 5.f, 0.5f}, math::vec3{0.f, -1.f, 0.f}, 10.f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->block, (math::vec3i{0, 0, 0}));
	EXPECT_EQ(hit->normal, (math::vec3i{0, 1, 0}));
}

TEST(VoxelRaycast, ZeroDirectionMisses) {
	const auto hit = raycastVoxel(blockAt(0, 0, 0), math::vec3{0.5f, 0.5f, 0.5f}, math::vec3{0.f, 0.f, 0.f}, 10.f);
	EXPECT_FALSE(hit.has_value());
}

TEST(VoxelRaycast, StopsAtNearestBlock) {
	const auto pred = [](const int32_t x, const int32_t y, const int32_t z) -> bool {
		return (x == 3 || x == 6) && y == 0 && z == 0;
	};
	const auto hit = raycastVoxel(pred, math::vec3{0.5f, 0.5f, 0.5f}, math::vec3{1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->block, (math::vec3i{3, 0, 0}));
}

TEST(VoxelRaycast, PlacementCellIsBlockPlusNormal) {
	const auto hit = raycastVoxel(blockAt(5, 0, 0), math::vec3{0.5f, 0.5f, 0.5f}, math::vec3{1.f, 0.f, 0.f}, 10.f);
	ASSERT_TRUE(hit.has_value());
	const math::vec3i placement{hit->block.x() + hit->normal.x(), hit->block.y() + hit->normal.y(),
								hit->block.z() + hit->normal.z()};
	EXPECT_EQ(placement, (math::vec3i{4, 0, 0}));
}
