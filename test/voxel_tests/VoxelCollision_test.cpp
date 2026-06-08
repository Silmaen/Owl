/**
 * @file VoxelCollision_test.cpp
 * @author Silmaen
 * @date 08/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/voxel/VoxelCollision.h>

using namespace owl;
using namespace owl::data::voxel;

namespace {
constexpr math::vec3 g_Half{0.4f, 0.9f, 0.4f};
auto noSolid() -> SolidPredicate {
	return [](int32_t, int32_t, int32_t) -> bool { return false; };
}
}// namespace

TEST(VoxelCollision, FreeMove) {
	const auto r = moveAabb(noSolid(), math::vec3{0.f, 5.f, 0.f}, g_Half, math::vec3{1.f, 0.f, 2.f});
	EXPECT_NEAR(r.position.x(), 1.f, 0.01f);
	EXPECT_NEAR(r.position.y(), 5.f, 0.01f);
	EXPECT_NEAR(r.position.z(), 2.f, 0.01f);
	EXPECT_FALSE(r.onGround);
	EXPECT_FALSE(r.hitCeiling);
}

TEST(VoxelCollision, FloorGrounds) {
	const SolidPredicate floor = [](int32_t, int32_t y, int32_t) -> bool { return y < 0; };// solid below y=0
	const auto r = moveAabb(floor, math::vec3{0.f, 5.f, 0.f}, g_Half, math::vec3{0.f, -10.f, 0.f});
	EXPECT_TRUE(r.onGround);
	EXPECT_NEAR(r.position.y(), g_Half.y(), 0.02f);// feet rest at y = 0
}

TEST(VoxelCollision, WallStopsHorizontal) {
	const SolidPredicate wall = [](int32_t x, int32_t, int32_t) -> bool { return x >= 2; };
	const auto r = moveAabb(wall, math::vec3{0.f, 5.f, 0.f}, g_Half, math::vec3{10.f, 0.f, 0.f});
	EXPECT_NEAR(r.position.x(), 2.f - g_Half.x(), 0.02f);// flush against the wall face at x=2
	EXPECT_FALSE(r.onGround);
}

TEST(VoxelCollision, Ceiling) {
	const SolidPredicate ceil = [](int32_t, int32_t y, int32_t) -> bool { return y >= 3; };
	const auto r = moveAabb(ceil, math::vec3{0.f, 1.f, 0.f}, g_Half, math::vec3{0.f, 10.f, 0.f});
	EXPECT_TRUE(r.hitCeiling);
	EXPECT_NEAR(r.position.y(), 3.f - g_Half.y(), 0.02f);
}

TEST(VoxelCollision, SlidesAlongWall) {
	const SolidPredicate wall = [](int32_t x, int32_t, int32_t) -> bool { return x >= 2; };
	const auto r = moveAabb(wall, math::vec3{0.f, 5.f, 0.f}, g_Half, math::vec3{10.f, 0.f, 5.f});
	EXPECT_NEAR(r.position.x(), 2.f - g_Half.x(), 0.02f);// X blocked
	EXPECT_NEAR(r.position.z(), 5.f, 0.02f);// Z slides freely
}

TEST(VoxelCollision, ThinFloorNoTunnel) {
	// A single-block-thick floor at the y=0 layer; a fast fall must not tunnel through it (sub-stepping).
	const SolidPredicate thinFloor = [](int32_t, int32_t y, int32_t) -> bool { return y == 0; };
	const auto r = moveAabb(thinFloor, math::vec3{0.f, 6.f, 0.f}, g_Half, math::vec3{0.f, -12.f, 0.f});
	EXPECT_TRUE(r.onGround);
	EXPECT_NEAR(r.position.y(), 1.f + g_Half.y(), 0.05f);// feet rest on top of the y=0 block (top face at y=1)
}
