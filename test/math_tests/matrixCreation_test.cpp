/**
 * @file matrixCreation_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/matrixCreation.h>
#include <math/trigonometry.h>

using namespace owl::math;

TEST(math, Identity) {
	const auto id3 = identity<float, 3>();
	EXPECT_NEAR(id3(0, 0), 1.f, 0.0001f);
	EXPECT_NEAR(id3(1, 1), 1.f, 0.0001f);
	EXPECT_NEAR(id3(2, 2), 1.f, 0.0001f);
	EXPECT_NEAR(id3(0, 1), 0.f, 0.0001f);
	EXPECT_NEAR(id3(1, 0), 0.f, 0.0001f);

	const auto id4 = identity<float, 4>();
	EXPECT_NEAR(id4(3, 3), 1.f, 0.0001f);
	EXPECT_NEAR(id4(0, 3), 0.f, 0.0001f);
}

TEST(math, ToMat3FromQuaternion) {
	// Identity quaternion should give identity matrix.
	const Quaternion<float> q{1, 0, 0, 0};
	const auto m = toMat3(q);
	EXPECT_NEAR(m(0, 0), 1.f, 0.001f);
	EXPECT_NEAR(m(1, 1), 1.f, 0.001f);
	EXPECT_NEAR(m(2, 2), 1.f, 0.001f);
	EXPECT_NEAR(m(0, 1), 0.f, 0.001f);
	EXPECT_NEAR(m(1, 0), 0.f, 0.001f);
}

TEST(math, ToMat4FromQuaternion) {
	const Quaternion<float> q{1, 0, 0, 0};
	const auto m = toMat4(q);
	EXPECT_NEAR(m(0, 0), 1.f, 0.001f);
	EXPECT_NEAR(m(3, 3), 1.f, 0.001f);
	EXPECT_NEAR(m(0, 3), 0.f, 0.001f);
	EXPECT_NEAR(m(3, 0), 0.f, 0.001f);
}

TEST(math, PerspectiveMatrix) {
	const auto p = perspective(radians(45.f), 16.f / 9.f, 0.1f, 100.f);
	// Non-zero on diagonal.
	EXPECT_TRUE(std::abs(p(0, 0)) > 0.f);
	EXPECT_TRUE(std::abs(p(1, 1)) > 0.f);
	EXPECT_TRUE(std::abs(p(2, 2)) > 0.f);
	// (3,2) should be -1.
	EXPECT_NEAR(p(3, 2), -1.f, 0.0001f);
	// Symmetric: (0,1) == 0 etc.
	EXPECT_NEAR(p(0, 1), 0.f, 0.0001f);
	EXPECT_NEAR(p(1, 0), 0.f, 0.0001f);
}

TEST(math, OrthoMatrix) {
	const auto o = ortho(-1.f, 1.f, -1.f, 1.f, 0.1f, 10.f);
	EXPECT_NEAR(o(0, 0), 1.f, 0.0001f);
	EXPECT_NEAR(o(1, 1), 1.f, 0.0001f);
	EXPECT_NEAR(o(3, 3), 1.f, 0.0001f);
	EXPECT_NEAR(o(0, 1), 0.f, 0.0001f);
}

TEST(math, TranslateMatrix) {
	const auto id = identity<float, 4>();
	const auto t = translate(id, vec3{3, 4, 5});
	// Translation should be in the last column.
	EXPECT_NEAR(t(0, 3), 3.f, 0.0001f);
	EXPECT_NEAR(t(1, 3), 4.f, 0.0001f);
	EXPECT_NEAR(t(2, 3), 5.f, 0.0001f);
	// Diagonal still 1.
	EXPECT_NEAR(t(0, 0), 1.f, 0.0001f);
	EXPECT_NEAR(t(1, 1), 1.f, 0.0001f);
	EXPECT_NEAR(t(2, 2), 1.f, 0.0001f);
	EXPECT_NEAR(t(3, 3), 1.f, 0.0001f);
}

TEST(math, RotateMatrix) {
	const auto id = identity<float, 4>();
	// Rotate 90 degrees around Z axis.
	const auto r = rotate(id, radians(90.f), vec3{0, 0, 1});
	// After 90° rotation around Z: x→y, y→-x.
	EXPECT_NEAR(r(0, 0), 0.f, 0.001f);
	EXPECT_NEAR(r(1, 0), 1.f, 0.001f);
	EXPECT_NEAR(r(0, 1), -1.f, 0.001f);
	EXPECT_NEAR(r(1, 1), 0.f, 0.001f);
	EXPECT_NEAR(r(2, 2), 1.f, 0.001f);
}

TEST(math, ScaleMatrix) {
	const auto id = identity<float, 4>();
	const auto s = scale(id, vec3{2, 3, 4});
	EXPECT_NEAR(s(0, 0), 2.f, 0.0001f);
	EXPECT_NEAR(s(1, 1), 3.f, 0.0001f);
	EXPECT_NEAR(s(2, 2), 4.f, 0.0001f);
	EXPECT_NEAR(s(3, 3), 1.f, 0.0001f);
	EXPECT_NEAR(s(0, 1), 0.f, 0.0001f);
}

TEST(math, ShearMatrix) {
	const auto id = identity<float, 4>();
	// Apply shear with lambdaX[0]=1 (shear X by Y).
	const auto sh = shear(id, vec3{0, 0, 0}, vec2{1, 0}, vec2{0, 0}, vec2{0, 0});
	// Diagonal stays 1.
	EXPECT_NEAR(sh(0, 0), 1.f, 0.0001f);
	EXPECT_NEAR(sh(1, 1), 1.f, 0.0001f);
	EXPECT_NEAR(sh(2, 2), 1.f, 0.0001f);
	EXPECT_NEAR(sh(3, 3), 1.f, 0.0001f);
	// lambdaX[0]=1 goes into shear matrix at position (0,1) => result(0,1).
	EXPECT_NEAR(sh(0, 1), 1.f, 0.0001f);
	// Identity shear: no change to a point at origin.
	const auto sh2 = shear(id, vec3{0, 0, 0}, vec2{0, 0}, vec2{0, 0}, vec2{0, 0});
	for (size_t i = 0; i < 4; ++i)
		for (size_t j = 0; j < 4; ++j) EXPECT_NEAR(sh2(i, j), i == j ? 1.f : 0.f, 0.0001f);
}

TEST(math, RotateInverse) {
	const auto id = identity<float, 4>();
	const auto r = rotate(id, radians(45.f), vec3{1, 0, 0});
	const auto ri = inverse(r);
	const auto result = r * ri;
	for (size_t i = 0; i < 4; ++i) {
		for (size_t j = 0; j < 4; ++j) { EXPECT_NEAR(result(i, j), i == j ? 1.f : 0.f, 0.001f); }
	}
}
