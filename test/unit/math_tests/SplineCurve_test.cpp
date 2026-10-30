/**
 * @file SplineCurve_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/SplineCurve.h>

#include <gtest/gtest.h>

#include <numbers>
#include <vector>

using namespace owl::math;

namespace {

auto line() -> SplineCurve {
	SplineCurve curve;
	curve.setInterpolation(SplineInterpolation::Linear);
	curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {3.f, 0.f, 0.f}}, {.position = {3.f, 4.f, 0.f}}});
	return curve;
}

// A Bezier quarter circle of radius 1 (handles at 0.5523 of the radius).
auto quarter() -> SplineCurve {
	constexpr float handle = 0.5523f;
	SplineCurve curve;
	curve.setInterpolation(SplineInterpolation::Bezier);
	curve.setResolution(64);
	curve.setPoints({{.position = {1.f, 0.f, 0.f}, .tangentIn = {}, .tangentOut = {0.f, handle, 0.f}},
					 {.position = {0.f, 1.f, 0.f}, .tangentIn = {handle, 0.f, 0.f}, .tangentOut = {}}});
	return curve;
}

}// namespace

TEST(SplineCurve, EmptyAndSinglePointCurvesAreStill) {
	SplineCurve curve;
	EXPECT_EQ(curve.getSegmentCount(), 0u);
	EXPECT_FLOAT_EQ(curve.getLength(), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.5f).x(), 0.f);
	EXPECT_FLOAT_EQ(curve.getNearestDistance({1.f, 1.f, 1.f}), 0.f);
	EXPECT_EQ(curve.insertAtParameter(0.f), 0u);
	curve.setPoints({{.position = {2.f, 1.f, 0.f}}});
	EXPECT_EQ(curve.getSegmentCount(), 0u);
	const auto sample = curve.sampleAtDistance(5.f);
	EXPECT_FLOAT_EQ(sample.position.x(), 2.f);
	EXPECT_FLOAT_EQ(sample.tangent.normSq(), 0.f);
	EXPECT_FLOAT_EQ(curve.getDerivative(0.f).normSq(), 0.f);
	EXPECT_EQ(curve.getPolyline().size(), 1u);
}

TEST(SplineCurve, LinearCurveIsMeasuredAndSampledByDistance) {
	const auto curve = line();
	EXPECT_EQ(curve.getSegmentCount(), 2u);
	EXPECT_NEAR(curve.getLength(), 7.f, 1e-4f);
	const auto middle = curve.sampleAtDistance(1.5f);
	EXPECT_NEAR(middle.position.x(), 1.5f, 1e-4f);
	EXPECT_NEAR(middle.tangent.x(), 1.f, 1e-4f);
	const auto upper = curve.sampleAtDistance(5.f);
	EXPECT_NEAR(upper.position.x(), 3.f, 1e-4f);
	EXPECT_NEAR(upper.position.y(), 2.f, 1e-4f);
	EXPECT_NEAR(upper.tangent.y(), 1.f, 1e-4f);
	// An open curve clamps.
	EXPECT_NEAR(curve.sampleAtDistance(-3.f).position.x(), 0.f, 1e-4f);
	EXPECT_NEAR(curve.sampleAtDistance(30.f).position.y(), 4.f, 1e-4f);
	EXPECT_NEAR(curve.sampleAtDistance(30.f).distance, 7.f, 1e-4f);
	EXPECT_NEAR(curve.getDistanceAtParameter(1.5f), 5.f, 1e-4f);
	EXPECT_NEAR(curve.getParameterAtDistance(5.f), 1.5f, 1e-4f);
	EXPECT_NEAR(curve.getNearestDistance({1.f, 2.f, 0.f}), 1.f, 1e-3f);
	EXPECT_NEAR(curve.getNearestDistance({5.f, 3.f, 0.f}), 6.f, 1e-3f);
}

TEST(SplineCurve, ClosedCurveWrapsTheDistance) {
	auto curve = line();
	curve.setClosed(true);
	EXPECT_EQ(curve.getSegmentCount(), 3u);
	EXPECT_NEAR(curve.getLength(), 12.f, 1e-3f);
	EXPECT_NEAR(curve.sampleAtDistance(13.5f).position.x(), 1.5f, 1e-3f);
	EXPECT_NEAR(curve.sampleAtDistance(-1.f).distance, 11.f, 1e-3f);
	EXPECT_NEAR(curve.evaluate(3.f).x(), 0.f, 1e-4f);
	EXPECT_NEAR(curve.evaluate(-0.5f).y(), 2.f, 1e-3f);
	EXPECT_TRUE(curve.isClosed());
}

TEST(SplineCurve, BezierHandlesShapeTheCurve) {
	const auto curve = quarter();
	EXPECT_NEAR(curve.getLength(), std::numbers::pi_v<float> * 0.5f, 2e-3f);
	const auto middle = curve.sampleAtDistance(curve.getLength() * 0.5f);
	EXPECT_NEAR(middle.position.norm(), 1.f, 1e-3f);
	EXPECT_NEAR(middle.position.x(), middle.position.y(), 1e-3f);
	EXPECT_NEAR(middle.tangent.x(), -middle.tangent.y(), 1e-3f);
	EXPECT_EQ(curve.getPolyline().size(), 65u);
}

TEST(SplineCurve, CatmullRomPassesThroughEveryPoint) {
	SplineCurve curve;
	curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {1.f, 1.f, 0.f}}, {.position = {2.f, 0.f, 0.f}}});
	EXPECT_EQ(curve.getInterpolation(), SplineInterpolation::CatmullRom);
	EXPECT_NEAR(curve.evaluate(1.f).y(), 1.f, 1e-5f);
	EXPECT_NEAR(curve.evaluate(2.f).x(), 2.f, 1e-5f);
	// The middle point is smooth: its derivative is horizontal.
	EXPECT_NEAR(curve.getDerivative(1.f).y(), 0.f, 1e-5f);
	const auto tangent = curve.getSmoothTangent(1);
	EXPECT_NEAR(tangent.x(), 2.f / 6.f, 1e-5f);
	EXPECT_FLOAT_EQ(curve.getSmoothTangent(9).normSq(), 0.f);
	curve.setResolution(0);
	EXPECT_EQ(curve.getResolution(), 1u);
	curve.setResolution(9999);
	EXPECT_EQ(curve.getResolution(), 256u);
}

TEST(SplineCurve, InsertingAPointKeepsABezierShape) {
	auto curve = quarter();
	const auto before = curve.evaluate(0.3f);
	const float length = curve.getLength();
	const auto index = curve.insertAtParameter(0.5f);
	EXPECT_EQ(index, 1u);
	ASSERT_EQ(curve.getPoints().size(), 3u);
	EXPECT_NEAR(curve.getLength(), length, 1e-3f);
	// Parameter 0.3 of the old segment is 0.6 of its first half.
	const auto after = curve.evaluate(0.6f);
	EXPECT_NEAR(after.x(), before.x(), 1e-4f);
	EXPECT_NEAR(after.y(), before.y(), 1e-4f);

	auto straight = line();
	EXPECT_EQ(straight.insertAtParameter(1.5f), 2u);
	EXPECT_NEAR(straight.getPoints()[2].position.y(), 2.f, 1e-4f);
	EXPECT_NEAR(straight.getLength(), 7.f, 1e-3f);
}

TEST(SplineCurve, ComparesItsDefinition) {
	const auto first = line();
	auto second = line();
	EXPECT_TRUE(first == second);
	second.setClosed(true);
	EXPECT_FALSE(first == second);
}
