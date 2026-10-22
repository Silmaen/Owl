/**
 * @file BoxCollision_tests.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/geometry/BoxCollision.h>

#include <vector>

using namespace owl;
using data::geometry::moveBoxAmong;

namespace {
const math::vec3 g_half{0.25f, 0.875f, 0.25f};
const math::box3f g_floor{math::vec3{-10.f, -0.1f, -10.f}, math::vec3{10.f, 0.f, 10.f}};
}// namespace

TEST(BoxCollision, AFallingBoxLandsOnTheFloor) {
	const std::vector boxes{g_floor};
	const auto moved = moveBoxAmong({0.f, 1.f, 0.f}, g_half, {0.f, -0.5f, 0.f}, boxes);
	EXPECT_TRUE(moved.onGround);
	EXPECT_NEAR(moved.position.y(), g_half.y(), 1e-3f);
	EXPECT_FALSE(moved.hitCeiling);
}

TEST(BoxCollision, AWallStopsAHorizontalMoveAndTheOtherAxisSlides) {
	const std::vector boxes{g_floor, math::box3f{math::vec3{1.f, 0.f, -5.f}, math::vec3{1.2f, 3.f, 5.f}}};
	const auto moved = moveBoxAmong({0.f, g_half.y() + 0.001f, 0.f}, g_half, {2.f, 0.f, 0.5f}, boxes, 0.35f);
	EXPECT_TRUE(moved.blocked);
	EXPECT_NEAR(moved.position.x(), 1.f - g_half.x(), 1e-3f);
	EXPECT_NEAR(moved.position.z(), 0.5f, 1e-5f);
}

TEST(BoxCollision, ALowStepIsClimbedAndAHighOneIsNot) {
	const std::vector low{g_floor, math::box3f{math::vec3{0.5f, 0.f, -1.f}, math::vec3{2.f, 0.3f, 1.f}}};
	const auto climbed = moveBoxAmong({0.f, g_half.y() + 0.001f, 0.f}, g_half, {0.4f, 0.f, 0.f}, low, 0.35f);
	EXPECT_FALSE(climbed.blocked);
	EXPECT_NEAR(climbed.position.y(), 0.3f + g_half.y(), 1e-3f);
	const std::vector high{g_floor, math::box3f{math::vec3{0.5f, 0.f, -1.f}, math::vec3{2.f, 0.5f, 1.f}}};
	const auto stopped = moveBoxAmong({0.f, g_half.y() + 0.001f, 0.f}, g_half, {0.4f, 0.f, 0.f}, high, 0.35f);
	EXPECT_TRUE(stopped.blocked);
	EXPECT_NEAR(stopped.position.y(), g_half.y() + 0.001f, 1e-5f);
}

TEST(BoxCollision, ACeilingStopsAJumpAndAnOverlappedBoxIsIgnored) {
	const std::vector ceiling{math::box3f{math::vec3{-5.f, 2.f, -5.f}, math::vec3{5.f, 2.2f, 5.f}}};
	const auto bumped = moveBoxAmong({0.f, 1.f, 0.f}, g_half, {0.f, 0.5f, 0.f}, ceiling);
	EXPECT_TRUE(bumped.hitCeiling);
	EXPECT_NEAR(bumped.position.y(), 2.f - g_half.y(), 1e-3f);
	const std::vector inside{math::box3f{math::vec3{-1.f, 0.f, -1.f}, math::vec3{1.f, 2.f, 1.f}}};
	const auto escaped = moveBoxAmong({0.f, 1.f, 0.f}, g_half, {3.f, 0.f, 0.f}, inside);
	EXPECT_NEAR(escaped.position.x(), 3.f, 1e-5f);
}
