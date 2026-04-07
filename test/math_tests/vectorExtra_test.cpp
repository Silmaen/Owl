/**
 * @file vectorExtra_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/vectors.h>

using namespace owl::math;

TEST(math, vectorDefaultConstruction) {
	constexpr vec3 v;
	EXPECT_NEAR(v.x(), 0.f, 0.0001f);
	EXPECT_NEAR(v.y(), 0.f, 0.0001f);
	EXPECT_NEAR(v.z(), 0.f, 0.0001f);
}

TEST(math, vectorCopyConstruction) {
	constexpr vec3 v1{1, 2, 3};
	constexpr vec3 v2(v1);
	EXPECT_NEAR(v2.x(), 1.f, 0.0001f);
	EXPECT_NEAR(v2.y(), 2.f, 0.0001f);
	EXPECT_NEAR(v2.z(), 3.f, 0.0001f);
}

TEST(math, vectorMoveConstruction) {
	vec3 v1{1, 2, 3};
	const vec3 v2(std::move(v1));
	EXPECT_NEAR(v2.x(), 1.f, 0.0001f);
	EXPECT_NEAR(v2.y(), 2.f, 0.0001f);
	EXPECT_NEAR(v2.z(), 3.f, 0.0001f);
}

TEST(math, vectorCrossDimCopy) {
	constexpr vec4 v4{1, 2, 3, 4};
	constexpr vec2 v2(v4);
	EXPECT_NEAR(v2.x(), 1.f, 0.0001f);
	EXPECT_NEAR(v2.y(), 2.f, 0.0001f);

	constexpr vec2 v2b{5, 6};
	constexpr vec4 v4b(v2b);
	EXPECT_NEAR(v4b.x(), 5.f, 0.0001f);
	EXPECT_NEAR(v4b.y(), 6.f, 0.0001f);
	EXPECT_NEAR(v4b.z(), 0.f, 0.0001f);
	EXPECT_NEAR(v4b.w(), 0.f, 0.0001f);
}

TEST(math, vectorAssignment) {
	constexpr vec3 v1{1, 2, 3};
	vec3 v2;
	v2 = v1;
	EXPECT_NEAR(v2.x(), 1.f, 0.0001f);
	vec3 v3;
	v3 = vec3{4, 5, 6};
	EXPECT_NEAR(v3.x(), 4.f, 0.0001f);
}

TEST(math, vectorCompoundArithmetic) {
	vec3 v{1, 2, 3};
	v += vec3{10, 20, 30};
	EXPECT_NEAR(v.x(), 11.f, 0.0001f);
	EXPECT_NEAR(v.y(), 22.f, 0.0001f);
	v -= vec3{1, 2, 3};
	EXPECT_NEAR(v.x(), 10.f, 0.0001f);
	EXPECT_NEAR(v.y(), 20.f, 0.0001f);
	v *= 2.f;
	EXPECT_NEAR(v.x(), 20.f, 0.0001f);
	v /= 4.f;
	EXPECT_NEAR(v.x(), 5.f, 0.0001f);
}

TEST(math, vectorUnaryMinus) {
	constexpr vec3 v{1, -2, 3};
	constexpr vec3 neg = -v;
	EXPECT_NEAR(neg.x(), -1.f, 0.0001f);
	EXPECT_NEAR(neg.y(), 2.f, 0.0001f);
	EXPECT_NEAR(neg.z(), -3.f, 0.0001f);
}

TEST(math, vectorDotProduct) {
	constexpr vec3 v1{1, 2, 3};
	constexpr vec3 v2{4, 5, 6};
	const float dot = v1 * v2;
	EXPECT_NEAR(dot, 32.f, 0.0001f);
}

TEST(math, vectorCrossProductSelf) {
	vec3 v{1, 0, 0};
	v ^= vec3{0, 1, 0};
	EXPECT_NEAR(v.x(), 0.f, 0.0001f);
	EXPECT_NEAR(v.y(), 0.f, 0.0001f);
	EXPECT_NEAR(v.z(), 1.f, 0.0001f);
}

TEST(math, vectorSurface) {
	constexpr vec2 v{3, 4};
	EXPECT_NEAR(v.surface(), 12.f, 0.0001f);
}

TEST(math, vectorRatio) {
	constexpr vec2 v{16, 9};
	EXPECT_NEAR(v.ratio(), 16.f / 9.f, 0.001f);
}

TEST(math, vectorRatio_uint) {
	constexpr vec2ui v{1920, 1080};
	EXPECT_NEAR(v.ratio(), 1920.f / 1080.f, 0.001f);
}

TEST(math, vectorData) {
	vec3 v{1, 2, 3};
	const float* d = v.data();
	EXPECT_NEAR(d[0], 1.f, 0.0001f);
	EXPECT_NEAR(d[1], 2.f, 0.0001f);
	v.data()[0] = 99.f;
	EXPECT_NEAR(v.x(), 99.f, 0.0001f);
}

TEST(math, vectorIterators) {
	const vec3 v{10, 20, 30};
	float sum = 0;
	for (const auto& val: v) sum += val;
	EXPECT_NEAR(sum, 60.f, 0.0001f);
}

TEST(math, vectorMutableIterators) {
	vec3 v{1, 2, 3};
	for (auto& val: v) val *= 10.f;
	EXPECT_NEAR(v.x(), 10.f, 0.0001f);
	EXPECT_NEAR(v.y(), 20.f, 0.0001f);
	EXPECT_NEAR(v.z(), 30.f, 0.0001f);
}

TEST(math, vectorNormalizeZero) {
	vec3 v{0, 0, 0};
	v.normalize();
	EXPECT_NEAR(v.norm(), 0.f, 0.0001f);
}

TEST(math, vector4Components) {
	vec4 v{1, 2, 3, 4};
	v.r() = 10;
	v.g() = 20;
	v.b() = 30;
	v.a() = 40;
	EXPECT_NEAR(v.x(), 10.f, 0.0001f);
	EXPECT_NEAR(v.y(), 20.f, 0.0001f);
	EXPECT_NEAR(v.z(), 30.f, 0.0001f);
	EXPECT_NEAR(v.w(), 40.f, 0.0001f);
}

TEST(math, vectorIntegerNorm) {
	constexpr vec3i v{3, -4, 1};
	EXPECT_EQ(v.norm(), 8);
	constexpr vec3ui vu{3, 4, 1};
	EXPECT_EQ(vu.norm(), 8u);
}

TEST(math, vectorScalarLeftMultiply) {
	constexpr vec3 v{1, 2, 3};
	const vec3 r = 5.f * v;
	EXPECT_NEAR(r.x(), 5.f, 0.0001f);
	EXPECT_NEAR(r.y(), 10.f, 0.0001f);
	EXPECT_NEAR(r.z(), 15.f, 0.0001f);
}
