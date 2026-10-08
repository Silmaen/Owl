/**
 * @file transform_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <cmath>
#include <math/Transform.h>
#include <math/matrixCreation.h>

using namespace owl::math;

constexpr auto vecNear(const vec3& a, const vec3& b, float accuracy = 0.001f) -> bool {
	return (std::abs(a[0] - b[0]) < accuracy) && (std::abs(a[1] - b[1]) < accuracy) &&
		   (std::abs(a[2] - b[2]) < accuracy);
}

const float pis2 = 2.f * std::atan(1.f);

TEST(Transform, decomposeTrivial) {
	mat4 mat{};

	// identity matrix
	mat = identity<float, 4>();
	{
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.rotation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.scale(), {1.f, 1.f, 1.f}));
	}

	// pure scale matrix
	mat(0, 0) = 0.1f;
	mat(1, 1) = 0.1f;
	mat(2, 2) = 0.1f;
	mat(3, 3) = 1;
	{
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.rotation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.scale(), {.1f, .1f, .1f}));
	}
}

TEST(Transform, decomposeWithPerspective) {
	mat4 mat = identity<float, 4>();

	// first perspective
	mat(3, 0) = 0.1f;
	mat(3, 1) = 0.;
	mat(3, 2) = 0.;
	{
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.rotation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.scale(), {1, 1, 1}));
	}
	mat(3, 0) = 0.;
	mat(3, 1) = 0.1f;
	mat(3, 2) = 0.;
	{
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.rotation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.scale(), {1, 1, 1}));
	}
	mat(3, 0) = 0.;
	mat(3, 1) = 0.;
	mat(3, 2) = 0.1f;
	{
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.rotation(), {0, 0, 0}));
		EXPECT_TRUE(vecNear(transform.scale(), {1, 1, 1}));
	}
}


TEST(Transform, moreInits) {
	{
		const mat4 mat;
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.translation(), {0, 0, 0}));
	}
	{
		mat4 mat;
		mat(0, 0) = 1.f;
		mat(2, 1) = 1.f;
		mat(1, 2) = 1.f;
		mat(3, 3) = 1.f;
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.scale(), {-1, -1, -1}));
	}
	{
		mat4 mat;
		mat(2, 0) = -1.f;
		mat(1, 1) = 1.f;
		mat(0, 2) = 1.f;
		mat(3, 3) = 1.f;
		Transform transform{mat};
		EXPECT_TRUE(vecNear(transform.rotation(), {0, pis2, 0}));
	}
	{
		Transform transform{vec3{10, 12, 13}, vec3{0.2f, 0.3f, 0.4f}};
		EXPECT_TRUE(vecNear(transform.scale(), {1, 1, 1}));
	}
}
