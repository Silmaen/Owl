/**
 * @file LagCompensation_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/LagCompensation.h>

using namespace owl;
using namespace owl::network;

TEST(HitHistory, APositionIsInterpolatedBetweenSamples) {
	HitHistory history;
	history.record(1.0, 5, {0.f, 0.f, 0.f}, 1.f);
	history.record(2.0, 5, {10.f, 0.f, 0.f}, 1.f);
	const auto middle = history.getPositionAt(5, 1.5);
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->x(), 5.f);
	EXPECT_FLOAT_EQ(history.getPositionAt(5, 0.0)->x(), 0.f);
	EXPECT_FLOAT_EQ(history.getPositionAt(5, 9.0)->x(), 10.f);
	EXPECT_FALSE(history.getPositionAt(6, 1.0).has_value());
}

TEST(HitHistory, AShotIsJudgedWhereTheTargetWas) {
	HitHistory history;
	history.record(1.0, 5, {0.f, 0.f, 0.f}, 1.f);
	history.record(2.0, 5, {10.f, 0.f, 0.f}, 1.f);
	const math::vec3 origin{0.f, -10.f, 0.f};
	const math::vec3 up{0.f, 1.f, 0.f};
	const auto past = history.raycast(1.0, origin, up, 100.f);
	ASSERT_TRUE(past.has_value());
	EXPECT_EQ(past->entity, 5U);
	EXPECT_NEAR(past->distance, 9.f, 1e-4f);
	EXPECT_FALSE(history.raycast(2.0, origin, up, 100.f).has_value());
	EXPECT_FALSE(history.raycast(1.0, origin, up, 5.f).has_value());
	EXPECT_FALSE(history.raycast(1.0, origin, up, 100.f, 5).has_value());
}

TEST(HitHistory, TheNearestSphereIsHit) {
	HitHistory history;
	history.record(1.0, 1, {0.f, 20.f, 0.f}, 1.f);
	history.record(1.0, 2, {0.f, 10.f, 0.f}, 1.f);
	const auto hit = history.raycast(1.0, {0.f, 0.f, 0.f}, {0.f, 2.f, 0.f}, 100.f);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->entity, 2U);
	EXPECT_FALSE(history.raycast(1.0, {0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 100.f).has_value());
}

TEST(HitHistory, OldPositionsArePruned) {
	HitHistory history;
	history.record(1.0, 1, {0.f, 0.f, 0.f}, 1.f);
	history.record(2.0, 1, {1.f, 0.f, 0.f}, 1.f);
	history.record(3.0, 1, {2.f, 0.f, 0.f}, 1.f);
	history.record(1.0, 2, {0.f, 0.f, 0.f}, 1.f);
	history.prune(2.5);
	EXPECT_EQ(history.getEntityCount(), 1U);
	EXPECT_FLOAT_EQ(history.getPositionAt(1, 0.0)->x(), 1.f);
	history.forget(1);
	EXPECT_EQ(history.getEntityCount(), 0U);
}
