/**
 * @file VectorCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/vectors.h>

using namespace owl::math;

// ---------- Default construction for various scalar types ----------

TEST(VectorCoverage, DefaultConstructDouble) {
	constexpr vec2d v;
	EXPECT_DOUBLE_EQ(v.x(), 0.0);
	EXPECT_DOUBLE_EQ(v.y(), 0.0);
}

TEST(VectorCoverage, DefaultConstructInt) {
	constexpr vec3i v;
	EXPECT_EQ(v.x(), 0);
	EXPECT_EQ(v.y(), 0);
	EXPECT_EQ(v.z(), 0);
}

TEST(VectorCoverage, DefaultConstructUint) {
	constexpr vec4ui v;
	EXPECT_EQ(v.x(), 0u);
	EXPECT_EQ(v.y(), 0u);
	EXPECT_EQ(v.z(), 0u);
	EXPECT_EQ(v.w(), 0u);
}

TEST(VectorCoverage, DefaultConstructUint8) {
	constexpr vec3ui8 v;
	EXPECT_EQ(v.x(), 0);
	EXPECT_EQ(v.y(), 0);
	EXPECT_EQ(v.z(), 0);
}

// ---------- Component constructors for less-exercised types ----------

TEST(VectorCoverage, Vec2DoubleComponentConstruct) {
	constexpr vec2d v(3.14, 2.71);
	EXPECT_DOUBLE_EQ(v.x(), 3.14);
	EXPECT_DOUBLE_EQ(v.y(), 2.71);
}

TEST(VectorCoverage, Vec2IntComponentConstruct) {
	constexpr vec2i v(7, -3);
	EXPECT_EQ(v.x(), 7);
	EXPECT_EQ(v.y(), -3);
}

TEST(VectorCoverage, Vec2UintComponentConstruct) {
	constexpr vec2ui v(100u, 200u);
	EXPECT_EQ(v.x(), 100u);
	EXPECT_EQ(v.y(), 200u);
}

TEST(VectorCoverage, Vec4DoubleComponentConstruct) {
	constexpr vec4d v(1.0, 2.0, 3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.x(), 1.0);
	EXPECT_DOUBLE_EQ(v.y(), 2.0);
	EXPECT_DOUBLE_EQ(v.z(), 3.0);
	EXPECT_DOUBLE_EQ(v.w(), 4.0);
}

TEST(VectorCoverage, Vec4IntComponentConstruct) {
	constexpr vec4i v(10, 20, 30, 40);
	EXPECT_EQ(v.x(), 10);
	EXPECT_EQ(v.y(), 20);
	EXPECT_EQ(v.z(), 30);
	EXPECT_EQ(v.w(), 40);
}

TEST(VectorCoverage, Vec4UintComponentConstruct) {
	constexpr vec4ui v(5u, 10u, 15u, 20u);
	EXPECT_EQ(v.x(), 5u);
	EXPECT_EQ(v.y(), 10u);
	EXPECT_EQ(v.z(), 15u);
	EXPECT_EQ(v.w(), 20u);
}

// ---------- Copy construction ----------

TEST(VectorCoverage, CopyConstructDouble) {
	constexpr vec3d v1(1.5, 2.5, 3.5);
	constexpr vec3d v2(v1);
	EXPECT_DOUBLE_EQ(v2.x(), 1.5);
	EXPECT_DOUBLE_EQ(v2.y(), 2.5);
	EXPECT_DOUBLE_EQ(v2.z(), 3.5);
}

TEST(VectorCoverage, CopyConstructInt) {
	constexpr vec2i v1(42, -7);
	constexpr vec2i v2(v1);
	EXPECT_EQ(v2.x(), 42);
	EXPECT_EQ(v2.y(), -7);
}

// ---------- Comparison operators ----------

TEST(VectorCoverage, EqualityDouble) {
	constexpr vec2d a(1.0, 2.0);
	constexpr vec2d b(1.0, 2.0);
	constexpr vec2d c(1.0, 3.0);
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(VectorCoverage, InequalityInt) {
	constexpr vec3i a(1, 2, 3);
	constexpr vec3i b(1, 2, 3);
	constexpr vec3i c(1, 2, 4);
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a != c);
}

TEST(VectorCoverage, InequalityUint) {
	constexpr vec2ui a(10u, 20u);
	constexpr vec2ui b(10u, 20u);
	constexpr vec2ui c(10u, 30u);
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a != c);
}

TEST(VectorCoverage, InequalityDouble) {
	constexpr vec4d a(1.0, 2.0, 3.0, 4.0);
	constexpr vec4d b(1.0, 2.0, 3.0, 4.0);
	constexpr vec4d c(1.0, 2.0, 3.0, 5.0);
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a != c);
}

// ---------- Const subscript operator ----------

TEST(VectorCoverage, ConstSubscriptDouble) {
	constexpr vec3d v(10.0, 20.0, 30.0);
	EXPECT_DOUBLE_EQ(v[0], 10.0);
	EXPECT_DOUBLE_EQ(v[1], 20.0);
	EXPECT_DOUBLE_EQ(v[2], 30.0);
}

TEST(VectorCoverage, ConstSubscriptInt) {
	constexpr vec4i v(1, 2, 3, 4);
	EXPECT_EQ(v[0], 1);
	EXPECT_EQ(v[1], 2);
	EXPECT_EQ(v[2], 3);
	EXPECT_EQ(v[3], 4);
}

// ---------- Const component accessors ----------

TEST(VectorCoverage, ConstAccessorsDouble) {
	constexpr vec4d v(1.1, 2.2, 3.3, 4.4);
	EXPECT_DOUBLE_EQ(v.x(), 1.1);
	EXPECT_DOUBLE_EQ(v.y(), 2.2);
	EXPECT_DOUBLE_EQ(v.z(), 3.3);
	EXPECT_DOUBLE_EQ(v.w(), 4.4);
}

TEST(VectorCoverage, ConstAccessorsInt) {
	constexpr vec2i v(5, 6);
	EXPECT_EQ(v.x(), 5);
	EXPECT_EQ(v.y(), 6);
}

TEST(VectorCoverage, ConstAccessorsUint) {
	constexpr vec4ui v(11u, 22u, 33u, 44u);
	EXPECT_EQ(v.x(), 11u);
	EXPECT_EQ(v.y(), 22u);
	EXPECT_EQ(v.z(), 33u);
	EXPECT_EQ(v.w(), 44u);
}

// ---------- Compound assignment operators ----------

TEST(VectorCoverage, PlusEqualsDouble) {
	vec2d v(1.0, 2.0);
	v += vec2d(10.0, 20.0);
	EXPECT_DOUBLE_EQ(v.x(), 11.0);
	EXPECT_DOUBLE_EQ(v.y(), 22.0);
}

TEST(VectorCoverage, PlusEqualsInt) {
	vec3i v(1, 2, 3);
	v += vec3i(10, 20, 30);
	EXPECT_EQ(v.x(), 11);
	EXPECT_EQ(v.y(), 22);
	EXPECT_EQ(v.z(), 33);
}

TEST(VectorCoverage, PlusEqualsUint) {
	vec4ui v(1u, 2u, 3u, 4u);
	v += vec4ui(10u, 20u, 30u, 40u);
	EXPECT_EQ(v.x(), 11u);
	EXPECT_EQ(v.y(), 22u);
	EXPECT_EQ(v.z(), 33u);
	EXPECT_EQ(v.w(), 44u);
}

TEST(VectorCoverage, MinusEqualsDouble) {
	vec2d v(10.0, 20.0);
	v -= vec2d(1.0, 2.0);
	EXPECT_DOUBLE_EQ(v.x(), 9.0);
	EXPECT_DOUBLE_EQ(v.y(), 18.0);
}

TEST(VectorCoverage, MinusEqualsInt) {
	vec3i v(10, 20, 30);
	v -= vec3i(1, 2, 3);
	EXPECT_EQ(v.x(), 9);
	EXPECT_EQ(v.y(), 18);
	EXPECT_EQ(v.z(), 27);
}

TEST(VectorCoverage, TimesEqualsDouble) {
	vec2d v(3.0, 4.0);
	v *= 2.0;
	EXPECT_DOUBLE_EQ(v.x(), 6.0);
	EXPECT_DOUBLE_EQ(v.y(), 8.0);
}

TEST(VectorCoverage, TimesEqualsInt) {
	vec3i v(3, 4, 5);
	v *= 3;
	EXPECT_EQ(v.x(), 9);
	EXPECT_EQ(v.y(), 12);
	EXPECT_EQ(v.z(), 15);
}

TEST(VectorCoverage, DivEqualsDouble) {
	vec2d v(10.0, 20.0);
	v /= 5.0;
	EXPECT_DOUBLE_EQ(v.x(), 2.0);
	EXPECT_DOUBLE_EQ(v.y(), 4.0);
}

TEST(VectorCoverage, DivEqualsInt) {
	vec3i v(12, 24, 36);
	v /= 3;
	EXPECT_EQ(v.x(), 4);
	EXPECT_EQ(v.y(), 8);
	EXPECT_EQ(v.z(), 12);
}

TEST(VectorCoverage, DivEqualsUint) {
	vec4ui v(20u, 40u, 60u, 80u);
	v /= 10u;
	EXPECT_EQ(v.x(), 2u);
	EXPECT_EQ(v.y(), 4u);
	EXPECT_EQ(v.z(), 6u);
	EXPECT_EQ(v.w(), 8u);
}

// ---------- Arithmetic operators ----------

TEST(VectorCoverage, AdditionDouble) {
	constexpr vec3d a(1.0, 2.0, 3.0);
	constexpr vec3d b(10.0, 20.0, 30.0);
	const vec3d c = a + b;
	EXPECT_DOUBLE_EQ(c.x(), 11.0);
	EXPECT_DOUBLE_EQ(c.y(), 22.0);
	EXPECT_DOUBLE_EQ(c.z(), 33.0);
}

TEST(VectorCoverage, AdditionInt) {
	constexpr vec2i a(3, 7);
	constexpr vec2i b(10, 20);
	const vec2i c = a + b;
	EXPECT_EQ(c.x(), 13);
	EXPECT_EQ(c.y(), 27);
}

TEST(VectorCoverage, SubtractionDouble) {
	constexpr vec3d a(10.0, 20.0, 30.0);
	constexpr vec3d b(1.0, 2.0, 3.0);
	const vec3d c = a - b;
	EXPECT_DOUBLE_EQ(c.x(), 9.0);
	EXPECT_DOUBLE_EQ(c.y(), 18.0);
	EXPECT_DOUBLE_EQ(c.z(), 27.0);
}

TEST(VectorCoverage, SubtractionInt) {
	constexpr vec2i a(10, 20);
	constexpr vec2i b(3, 7);
	const vec2i c = a - b;
	EXPECT_EQ(c.x(), 7);
	EXPECT_EQ(c.y(), 13);
}

TEST(VectorCoverage, ScalarMultiplyDouble) {
	constexpr vec3d v(1.0, 2.0, 3.0);
	const vec3d r = v * 5.0;
	EXPECT_DOUBLE_EQ(r.x(), 5.0);
	EXPECT_DOUBLE_EQ(r.y(), 10.0);
	EXPECT_DOUBLE_EQ(r.z(), 15.0);
}

TEST(VectorCoverage, ScalarMultiplyInt) {
	constexpr vec2i v(3, 4);
	const vec2i r = v * 10;
	EXPECT_EQ(r.x(), 30);
	EXPECT_EQ(r.y(), 40);
}

TEST(VectorCoverage, ScalarLeftMultiplyDouble) {
	constexpr vec3d v(1.0, 2.0, 3.0);
	const vec3d r = 5.0 * v;
	EXPECT_DOUBLE_EQ(r.x(), 5.0);
	EXPECT_DOUBLE_EQ(r.y(), 10.0);
	EXPECT_DOUBLE_EQ(r.z(), 15.0);
}

TEST(VectorCoverage, ScalarLeftMultiplyInt) {
	constexpr vec2i v(3, 4);
	const vec2i r = 10 * v;
	EXPECT_EQ(r.x(), 30);
	EXPECT_EQ(r.y(), 40);
}

TEST(VectorCoverage, ScalarDivisionDouble) {
	constexpr vec3d v(10.0, 20.0, 30.0);
	const vec3d r = v / 5.0;
	EXPECT_DOUBLE_EQ(r.x(), 2.0);
	EXPECT_DOUBLE_EQ(r.y(), 4.0);
	EXPECT_DOUBLE_EQ(r.z(), 6.0);
}

TEST(VectorCoverage, ScalarDivisionInt) {
	constexpr vec2i v(12, 24);
	const vec2i r = v / 3;
	EXPECT_EQ(r.x(), 4);
	EXPECT_EQ(r.y(), 8);
}

TEST(VectorCoverage, ScalarDivisionUint) {
	constexpr vec4ui v(20u, 40u, 60u, 80u);
	const vec4ui r = v / 10u;
	EXPECT_EQ(r.x(), 2u);
	EXPECT_EQ(r.y(), 4u);
	EXPECT_EQ(r.z(), 6u);
	EXPECT_EQ(r.w(), 8u);
}

// ---------- Unary negation ----------

TEST(VectorCoverage, UnaryNegationDouble) {
	constexpr vec3d v(1.0, -2.0, 3.0);
	constexpr vec3d neg = -v;
	EXPECT_DOUBLE_EQ(neg.x(), -1.0);
	EXPECT_DOUBLE_EQ(neg.y(), 2.0);
	EXPECT_DOUBLE_EQ(neg.z(), -3.0);
}

TEST(VectorCoverage, UnaryNegationInt) {
	constexpr vec2i v(5, -3);
	constexpr vec2i neg = -v;
	EXPECT_EQ(neg.x(), -5);
	EXPECT_EQ(neg.y(), 3);
}

// ---------- Dot product ----------

TEST(VectorCoverage, DotProductDouble) {
	constexpr vec3d a(1.0, 2.0, 3.0);
	constexpr vec3d b(4.0, 5.0, 6.0);
	const double dot = a * b;
	EXPECT_DOUBLE_EQ(dot, 32.0);
}

TEST(VectorCoverage, DotProductInt) {
	constexpr vec2i a(3, 4);
	constexpr vec2i b(5, 6);
	const int32_t dot = a * b;
	EXPECT_EQ(dot, 39);
}

TEST(VectorCoverage, DotProductUint) {
	constexpr vec3ui a(1u, 2u, 3u);
	constexpr vec3ui b(4u, 5u, 6u);
	const uint32_t dot = a * b;
	EXPECT_EQ(dot, 32u);
}

// ---------- Cross product ----------

TEST(VectorCoverage, CrossProductDouble) {
	constexpr vec3d a(1.0, 0.0, 0.0);
	constexpr vec3d b(0.0, 1.0, 0.0);
	const vec3d c = a ^ b;
	EXPECT_DOUBLE_EQ(c.x(), 0.0);
	EXPECT_DOUBLE_EQ(c.y(), 0.0);
	EXPECT_DOUBLE_EQ(c.z(), 1.0);
}

TEST(VectorCoverage, CrossProductInt) {
	constexpr vec3i a(1, 2, 3);
	constexpr vec3i b(3, 4, 1);
	const vec3i c = a ^ b;
	EXPECT_EQ(c.x(), -10);
	EXPECT_EQ(c.y(), 8);
	EXPECT_EQ(c.z(), -2);
}

TEST(VectorCoverage, CrossProductSelfAssignDouble) {
	vec3d v(1.0, 0.0, 0.0);
	v ^= vec3d(0.0, 1.0, 0.0);
	EXPECT_DOUBLE_EQ(v.x(), 0.0);
	EXPECT_DOUBLE_EQ(v.y(), 0.0);
	EXPECT_DOUBLE_EQ(v.z(), 1.0);
}

// ---------- Norm ----------

TEST(VectorCoverage, NormDouble) {
	constexpr vec3d v(3.0, 4.0, 0.0);
	EXPECT_DOUBLE_EQ(v.norm(), 5.0);
}

TEST(VectorCoverage, NormSqDouble) {
	constexpr vec2d v(3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.normSq(), 25.0);
}

TEST(VectorCoverage, NormSignedInt) {
	constexpr vec3i v(3, -4, 1);
	EXPECT_EQ(v.norm(), 8);
}

TEST(VectorCoverage, NormUnsignedInt) {
	constexpr vec3ui v(3u, 4u, 1u);
	EXPECT_EQ(v.norm(), 8u);
}

TEST(VectorCoverage, NormSignedInt2) {
	constexpr vec2i v(5, -3);
	EXPECT_EQ(v.norm(), 8);
}

TEST(VectorCoverage, NormUnsignedInt2) {
	constexpr vec2ui v(5u, 3u);
	EXPECT_EQ(v.norm(), 8u);
}

TEST(VectorCoverage, NormUint8) {
	constexpr vec3ui8 v(3, 4, 1);
	EXPECT_EQ(v.norm(), 8);
}

TEST(VectorCoverage, NormSqInt) {
	constexpr vec2i v(3, 4);
	EXPECT_EQ(v.normSq(), 25);
}

// ---------- Normalization ----------

TEST(VectorCoverage, NormalizeDouble) {
	vec3d v(3.0, 4.0, 0.0);
	v.normalize();
	EXPECT_NEAR(v.norm(), 1.0, 1e-10);
}

TEST(VectorCoverage, NormalizedDouble) {
	constexpr vec2d v(3.0, 4.0);
	const vec2d n = v.normalized();
	EXPECT_NEAR(n.norm(), 1.0, 1e-10);
	EXPECT_NEAR(n.x(), 0.6, 1e-10);
	EXPECT_NEAR(n.y(), 0.8, 1e-10);
}

TEST(VectorCoverage, NormalizeZeroDouble) {
	vec3d v(0.0, 0.0, 0.0);
	v.normalize();
	EXPECT_DOUBLE_EQ(v.x(), 0.0);
	EXPECT_DOUBLE_EQ(v.y(), 0.0);
	EXPECT_DOUBLE_EQ(v.z(), 0.0);
}

TEST(VectorCoverage, NormalizeSignedInt) {
	vec3i v(4, -5, 1);
	v.normalize();
	EXPECT_EQ(v.norm(), 1);
	EXPECT_EQ(v.y(), -1);
}

TEST(VectorCoverage, NormalizeUnsignedInt) {
	vec3ui v(4u, 5u, 1u);
	v.normalize();
	EXPECT_EQ(v.norm(), 1u);
	EXPECT_EQ(v.y(), 1u);
}

TEST(VectorCoverage, NormalizeSignedInt2d) {
	vec2i v(3, -7);
	v.normalize();
	EXPECT_EQ(v.norm(), 1);
	EXPECT_EQ(v.y(), -1);
	EXPECT_EQ(v.x(), 0);
}

TEST(VectorCoverage, NormalizeUnsignedInt2d) {
	vec2ui v(3u, 7u);
	v.normalize();
	EXPECT_EQ(v.norm(), 1u);
	EXPECT_EQ(v.y(), 1u);
	EXPECT_EQ(v.x(), 0u);
}

// ---------- Surface and ratio ----------

TEST(VectorCoverage, SurfaceDouble) {
	constexpr vec2d v(3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.surface(), 12.0);
}

TEST(VectorCoverage, SurfaceInt) {
	constexpr vec2i v(3, 4);
	EXPECT_EQ(v.surface(), 12);
}

TEST(VectorCoverage, RatioDouble) {
	constexpr vec2d v(16.0, 9.0);
	EXPECT_NEAR(v.ratio(), 16.f / 9.f, 0.001f);
}

TEST(VectorCoverage, RatioInt) {
	constexpr vec2i v(16, 8);
	EXPECT_NEAR(v.ratio(), 2.0f, 0.001f);
}

// ---------- Data pointer ----------

TEST(VectorCoverage, DataPointerDouble) {
	vec3d v(1.0, 2.0, 3.0);
	const double* d = v.data();
	EXPECT_DOUBLE_EQ(d[0], 1.0);
	EXPECT_DOUBLE_EQ(d[1], 2.0);
	EXPECT_DOUBLE_EQ(d[2], 3.0);
	v.data()[0] = 99.0;
	EXPECT_DOUBLE_EQ(v.x(), 99.0);
}

TEST(VectorCoverage, DataPointerInt) {
	const vec2i v(5, 6);
	const int32_t* d = v.data();
	EXPECT_EQ(d[0], 5);
	EXPECT_EQ(d[1], 6);
}

// ---------- Iterators ----------

TEST(VectorCoverage, IteratorsDouble) {
	const vec3d v(10.0, 20.0, 30.0);
	double sum = 0.0;
	for (const auto& val: v) sum += val;
	EXPECT_DOUBLE_EQ(sum, 60.0);
}

TEST(VectorCoverage, MutableIteratorsInt) {
	vec3i v(1, 2, 3);
	for (auto& val: v) val *= 10;
	EXPECT_EQ(v.x(), 10);
	EXPECT_EQ(v.y(), 20);
	EXPECT_EQ(v.z(), 30);
}

// ---------- Cross-dimension copy ----------

TEST(VectorCoverage, CrossDimCopyDouble) {
	constexpr vec4d v4(1.0, 2.0, 3.0, 4.0);
	constexpr vec2d v2(v4);
	EXPECT_DOUBLE_EQ(v2.x(), 1.0);
	EXPECT_DOUBLE_EQ(v2.y(), 2.0);

	constexpr vec2d v2b(5.0, 6.0);
	constexpr vec4d v4b(v2b);
	EXPECT_DOUBLE_EQ(v4b.x(), 5.0);
	EXPECT_DOUBLE_EQ(v4b.y(), 6.0);
	EXPECT_DOUBLE_EQ(v4b.z(), 0.0);
	EXPECT_DOUBLE_EQ(v4b.w(), 0.0);
}

TEST(VectorCoverage, CrossDimAssignInt) {
	constexpr vec4i v4(1, 2, 3, 4);
	vec2i v2;
	v2 = v4;
	EXPECT_EQ(v2.x(), 1);
	EXPECT_EQ(v2.y(), 2);
}

// ---------- Mutable component accessors ----------

TEST(VectorCoverage, MutableAccessorsDouble) {
	vec4d v(0.0, 0.0, 0.0, 0.0);
	v.x() = 1.1;
	v.y() = 2.2;
	v.z() = 3.3;
	v.w() = 4.4;
	EXPECT_DOUBLE_EQ(v.x(), 1.1);
	EXPECT_DOUBLE_EQ(v.y(), 2.2);
	EXPECT_DOUBLE_EQ(v.z(), 3.3);
	EXPECT_DOUBLE_EQ(v.w(), 4.4);
}

TEST(VectorCoverage, MutableAccessorsInt) {
	vec2i v(0, 0);
	v.x() = 42;
	v.y() = -7;
	EXPECT_EQ(v.x(), 42);
	EXPECT_EQ(v.y(), -7);
}

// ---------- RGBA accessors for vec4 ----------

TEST(VectorCoverage, RgbaAccessorsDouble) {
	vec4d v(1.0, 2.0, 3.0, 4.0);
	EXPECT_DOUBLE_EQ(v.r(), 1.0);
	EXPECT_DOUBLE_EQ(v.g(), 2.0);
	EXPECT_DOUBLE_EQ(v.b(), 3.0);
	EXPECT_DOUBLE_EQ(v.a(), 4.0);
	v.r() = 10.0;
	v.g() = 20.0;
	v.b() = 30.0;
	v.a() = 40.0;
	EXPECT_DOUBLE_EQ(v.x(), 10.0);
	EXPECT_DOUBLE_EQ(v.y(), 20.0);
	EXPECT_DOUBLE_EQ(v.z(), 30.0);
	EXPECT_DOUBLE_EQ(v.w(), 40.0);
}

TEST(VectorCoverage, RgbaAccessorsInt) {
	vec4i v(1, 2, 3, 4);
	EXPECT_EQ(v.r(), 1);
	EXPECT_EQ(v.g(), 2);
	EXPECT_EQ(v.b(), 3);
	EXPECT_EQ(v.a(), 4);
}

TEST(VectorCoverage, RgbaAccessorsUint8) {
	vec4ui8 v(255, 128, 64, 32);
	EXPECT_EQ(v.r(), 255);
	EXPECT_EQ(v.g(), 128);
	EXPECT_EQ(v.b(), 64);
	EXPECT_EQ(v.a(), 32);
}

// ---------- Subscript operator (mutable) ----------

TEST(VectorCoverage, MutableSubscriptDouble) {
	vec3d v(0.0, 0.0, 0.0);
	v[0] = 1.5;
	v[1] = 2.5;
	v[2] = 3.5;
	EXPECT_DOUBLE_EQ(v.x(), 1.5);
	EXPECT_DOUBLE_EQ(v.y(), 2.5);
	EXPECT_DOUBLE_EQ(v.z(), 3.5);
}

TEST(VectorCoverage, MutableSubscriptUint) {
	vec2ui v(0u, 0u);
	v[0] = 100u;
	v[1] = 200u;
	EXPECT_EQ(v.x(), 100u);
	EXPECT_EQ(v.y(), 200u);
}
