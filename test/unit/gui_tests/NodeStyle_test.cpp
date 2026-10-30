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

TEST(NodeStyle, LinkSegmentsGoBackwardsThroughWaypointsWithoutLooping) {
	const math::vec2f from{400.f, 0.f};
	const math::vec2f to{0.f, 0.f};
	const std::vector<math::vec2f> waypoints{{450.f, 200.f}, {-50.f, 200.f}};
	const auto segments = getNodeLinkSegments(from, to, waypoints, 1.f);
	ASSERT_EQ(segments.size(), 3u);
	// The pins keep their sides: the output leaves to the right, the input is entered from the left.
	EXPECT_GT(segments[0][1].x(), from.x());
	EXPECT_LT(segments[2][2].x(), to.x());
	// The leftward middle segment leaves and enters its waypoints heading left, not to the right.
	EXPECT_LT(segments[1][1].x(), waypoints[0].x());
	EXPECT_GT(segments[1][2].x(), waypoints[1].x());
	// The tangent is continuous through a waypoint.
	const auto in = segments[0][3] - segments[0][2];
	const auto out = segments[1][1] - segments[1][0];
	EXPECT_NEAR(in.x() * out.y() - in.y() * out.x(), 0.f, 1e-2f);
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

TEST(NodeStyle, LinkColorsAreNamedAndTheGridSnapRounds) {
	ASSERT_FALSE(getLinkColors().empty());
	for (const auto& entry: getLinkColors()) {
		const auto color = findLinkColor(entry.name);
		ASSERT_TRUE(color.has_value());
		EXPECT_EQ(*color, entry.color);
	}
	EXPECT_FALSE(findLinkColor("").has_value());
	EXPECT_FALSE(findLinkColor("Chartreuse").has_value());
	EXPECT_EQ(snapToNodeGrid({7.f, 9.f}), (math::vec2f{0.f, 16.f}));
	EXPECT_EQ(snapToNodeGrid({-25.f, 40.f}), (math::vec2f{-32.f, 48.f}));
}
