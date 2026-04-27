/**
 * @file Curve_test.cpp
 * @author Silmaen
 * @date 26/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "testHelper.h"

#include <math/Curve.h>

using owl::math::Curve;
using owl::math::CurveInterpolation;
using owl::math::Keyframe;

TEST(Curve, EmptyEvaluateReturnsZero) {
	const Curve curve;
	EXPECT_TRUE(curve.empty());
	EXPECT_EQ(curve.keyCount(), 0u);
	EXPECT_FLOAT_EQ(curve.evaluate(0.f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(-10.f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(10.f), 0.f);
}

TEST(Curve, SingleKeyEvaluatesToKeyValue) {
	Curve curve;
	curve.addKey({0.5f, 2.5f});
	EXPECT_EQ(curve.keyCount(), 1u);
	EXPECT_FLOAT_EQ(curve.evaluate(0.f), 2.5f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.5f), 2.5f);
	EXPECT_FLOAT_EQ(curve.evaluate(1.f), 2.5f);
}

TEST(Curve, LinearInterpolatesAtMidpoint) {
	Curve curve;
	curve.setInterpolation(CurveInterpolation::Linear);
	curve.addKey({0.f, 0.f});
	curve.addKey({1.f, 4.f});
	EXPECT_FLOAT_EQ(curve.evaluate(0.f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.5f), 2.f);
	EXPECT_FLOAT_EQ(curve.evaluate(1.f), 4.f);
}

TEST(Curve, BeforeFirstKeyClampsToFirstValue) {
	Curve curve;
	curve.addKey({1.f, 7.f});
	curve.addKey({2.f, 12.f});
	EXPECT_FLOAT_EQ(curve.evaluate(-5.f), 7.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.99f), 7.f);
}

TEST(Curve, AfterLastKeyClampsToLastValue) {
	Curve curve;
	curve.addKey({1.f, 7.f});
	curve.addKey({2.f, 12.f});
	EXPECT_FLOAT_EQ(curve.evaluate(2.f), 12.f);
	EXPECT_FLOAT_EQ(curve.evaluate(99.f), 12.f);
}

TEST(Curve, ConstantHoldsLeftKeyValue) {
	Curve curve;
	curve.setInterpolation(CurveInterpolation::Constant);
	curve.addKey({0.f, 0.f});
	curve.addKey({1.f, 10.f});
	EXPECT_FLOAT_EQ(curve.evaluate(0.f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.25f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.99f), 0.f);
	EXPECT_FLOAT_EQ(curve.evaluate(1.f), 10.f);
}

TEST(Curve, SmoothDoesNotOvershootMonotonicData) {
	Curve curve;
	curve.setInterpolation(CurveInterpolation::Smooth);
	curve.addKey({0.f, 0.f});
	curve.addKey({1.f, 1.f});
	for (float t = 0.f; t <= 1.f; t += 0.1f) {
		const float v = curve.evaluate(t);
		EXPECT_GE(v, 0.f) << "t=" << t;
		EXPECT_LE(v, 1.f) << "t=" << t;
	}
	EXPECT_FLOAT_EQ(curve.evaluate(0.5f), 0.5f);
}

TEST(Curve, AddKeyOutOfOrderIsSorted) {
	Curve curve;
	curve.setInterpolation(CurveInterpolation::Linear);
	curve.addKey({1.f, 10.f});
	curve.addKey({0.f, 0.f});
	curve.addKey({0.5f, 4.f});
	ASSERT_EQ(curve.keyCount(), 3u);
	EXPECT_FLOAT_EQ(curve.key(0).time, 0.f);
	EXPECT_FLOAT_EQ(curve.key(1).time, 0.5f);
	EXPECT_FLOAT_EQ(curve.key(2).time, 1.f);
	EXPECT_FLOAT_EQ(curve.evaluate(0.25f), 2.f);
}

TEST(Curve, AddKeyReplacesExistingTime) {
	Curve curve;
	curve.addKey({0.5f, 1.f});
	curve.addKey({0.5f, 99.f});
	ASSERT_EQ(curve.keyCount(), 1u);
	EXPECT_FLOAT_EQ(curve.key(0).value, 99.f);
}

TEST(Curve, RemoveKeyReducesCount) {
	Curve curve;
	curve.addKey({0.f, 0.f});
	curve.addKey({1.f, 1.f});
	curve.removeKey(0);
	EXPECT_EQ(curve.keyCount(), 1u);
	EXPECT_FLOAT_EQ(curve.key(0).time, 1.f);
}

TEST(Curve, RemoveKeyOutOfRangeIsNoop) {
	Curve curve;
	curve.addKey({0.f, 0.f});
	curve.removeKey(99);
	EXPECT_EQ(curve.keyCount(), 1u);
}

TEST(Curve, SetKeyReorderingTimeResorts) {
	Curve curve;
	curve.addKey({0.f, 0.f});
	curve.addKey({0.5f, 5.f});
	curve.addKey({1.f, 10.f});
	curve.setKey(0, {2.f, 20.f});
	ASSERT_EQ(curve.keyCount(), 3u);
	EXPECT_FLOAT_EQ(curve.key(0).time, 0.5f);
	EXPECT_FLOAT_EQ(curve.key(1).time, 1.f);
	EXPECT_FLOAT_EQ(curve.key(2).time, 2.f);
	EXPECT_FLOAT_EQ(curve.key(2).value, 20.f);
}

TEST(Curve, ClearEmptiesContent) {
	Curve curve;
	curve.addKey({0.f, 0.f});
	curve.addKey({1.f, 1.f});
	curve.clear();
	EXPECT_TRUE(curve.empty());
	EXPECT_EQ(curve.keyCount(), 0u);
}

TEST(Curve, KeyByKeyComparison) {
	Curve a;
	Curve b;
	a.addKey({0.f, 0.f});
	a.addKey({1.f, 1.f});
	b.addKey({0.f, 0.f});
	b.addKey({1.f, 1.f});
	ASSERT_EQ(a.keyCount(), b.keyCount());
	for (size_t idx = 0; idx < a.keyCount(); ++idx) {
		EXPECT_FLOAT_EQ(a.key(idx).time, b.key(idx).time);
		EXPECT_FLOAT_EQ(a.key(idx).value, b.key(idx).value);
	}
	EXPECT_EQ(a.getInterpolation(), b.getInterpolation());
	b.setInterpolation(CurveInterpolation::Smooth);
	EXPECT_NE(a.getInterpolation(), b.getInterpolation());
}
