/**
 * @file NodeStyle_test.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/widgets/NodeStyle.h>

#include <gtest/gtest.h>

#include <vector>

using namespace owl;
using namespace owl::gui::widgets;

TEST(NodeStyle, LinkCurveLeavesAndEntersHorizontally) {
	const math::vec2f from{0.f, 0.f};
	const math::vec2f to{200.f, 100.f};
	const auto curve = getNodeLinkCurve(from, to, 1.f);
	EXPECT_EQ(curve[0], from);
	EXPECT_EQ(curve[3], to);
	EXPECT_FLOAT_EQ(curve[1].y(), from.y());
	EXPECT_FLOAT_EQ(curve[2].y(), to.y());
	EXPECT_GT(curve[1].x(), from.x());
	EXPECT_LT(curve[2].x(), to.x());
	const auto backwards = getNodeLinkCurve(to, from, 1.f);
	EXPECT_GT(backwards[1].x() - to.x(), 90.f);
}

TEST(NodeStyle, LinkDistanceIsZeroOnTheCurve) {
	const math::vec2f from{0.f, 0.f};
	const math::vec2f to{200.f, 0.f};
	EXPECT_LT(getNodeLinkDistance(from, to, {100.f, 0.f}, 1.f), 1.f);
	EXPECT_NEAR(getNodeLinkDistance(from, to, {100.f, 50.f}, 1.f), 50.f, 1.f);
	EXPECT_FLOAT_EQ(getNodeCanvasBackground().w(), 1.f);
}

TEST(NodeStyle, LinkSegmentsPassThroughEveryWaypoint) {
	const math::vec2f from{0.f, 0.f};
	const math::vec2f to{300.f, 0.f};
	const std::vector<math::vec2f> waypoints{{100.f, 80.f}, {200.f, -80.f}};
	const auto segments = getNodeLinkSegments(from, to, waypoints, 1.f);
	ASSERT_EQ(segments.size(), 3u);
	EXPECT_EQ(segments[0][0], from);
	EXPECT_EQ(segments[0][3], waypoints[0]);
	EXPECT_EQ(segments[1][0], waypoints[0]);
	EXPECT_EQ(segments[1][3], waypoints[1]);
	EXPECT_EQ(segments[2][3], to);
	EXPECT_EQ(getNodeLinkSegments(from, to, {}, 1.f).size(), 1u);
}

TEST(NodeStyle, LinkDistanceAndInsertIndexFollowTheWaypoints) {
	const math::vec2f from{0.f, 0.f};
	const math::vec2f to{300.f, 0.f};
	const std::vector<math::vec2f> waypoints{{100.f, 100.f}, {200.f, 100.f}};
	EXPECT_LT(getNodeLinkDistance(from, to, waypoints, {150.f, 100.f}, 1.f), 1.f);
	EXPECT_GT(getNodeLinkDistance(from, to, waypoints, {150.f, 0.f}, 1.f), 50.f);
	EXPECT_EQ(getNodeLinkInsertIndex(from, to, waypoints, {150.f, 100.f}, 1.f), 1u);
	EXPECT_EQ(getNodeLinkInsertIndex(from, to, waypoints, {20.f, 10.f}, 1.f), 0u);
	EXPECT_EQ(getNodeLinkInsertIndex(from, to, waypoints, {280.f, 10.f}, 1.f), 2u);
}
