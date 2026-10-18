/**
 * @file matrix_test.cpp
 * @author Silmaen
 * @date 02/07/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "math/matrixCreation.h"
#include "testHelper.h"

#include <math/linAlgebra.h>

#include <cmath>

using namespace owl::math;

TEST(math, matrixRowCol) {
	mat3 mat;
	mat.setRow(0, vec3{1.f, 2.f, 3.f});
	EXPECT_EQ(mat(0, 1), 2.f);
	mat.setColumn(1, vec3{1.f, 2.f, 3.f});
	EXPECT_EQ(mat(0, 1), 1.f);
	EXPECT_EQ(mat(0, 2), 3.f);
}

TEST(math, matrixBase) {
	constexpr mat3 mat{1, 2, 3, 4, 5, 6, 7, 8, 9};
	EXPECT_NEAR(mat.norm(), 16.88194, 0.001);
	EXPECT_FALSE(mat == mat3({9, 8, 7, 6, 5, 4, 3, 2, 1}));
	EXPECT_TRUE(mat != mat3({9, 8, 7, 6, 5, 4, 3, 2, 1}));
	EXPECT_NEAR(mat(0, 0), 1, 0.001);
	EXPECT_NEAR(mat(0, 2), 7, 0.001);
	mat3 mat2{1, 2, 3, 4, 5, 6, 7, 8, 9};
	mat2(0, 2) = 55;
	mat2(1, 1) = 45;
	EXPECT_NEAR(mat2(0, 2), 55, 0.0001);
	EXPECT_NEAR(mat2(1, 1), 45, 0.0001);
	EXPECT_NEAR(mat.transposed()(2, 0), 7, 0.0001);
	EXPECT_NEAR(mat2(1, 0), 2, 0.0001);
	mat2.transpose();
	EXPECT_NEAR(mat2(1, 0), 4, 0.0001);
}

TEST(math, matrixAddition) {
	constexpr mat3 mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	constexpr mat3 mat2{9, 8, 7, 6, 5, 4, 3, 2, 1};
	mat3 madd = mat1 + mat2;
	EXPECT_NEAR(madd(1, 1), 10, 0.0001);
	madd = mat2 - mat1;
	EXPECT_NEAR(madd(1, 1), 0, 0.0001);
}

TEST(math, matrixScalar) {
	constexpr mat3 mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	mat3 madd = mat1 * 2;
	EXPECT_NEAR(madd(1, 1), 10, 0.0001);
	madd = 7 * mat1;
	EXPECT_NEAR(madd(1, 1), 35, 0.0001);
	madd = mat1 / 2;
	EXPECT_NEAR(madd(0, 0), 0.5, 0.0001);
}


TEST(math, matrixProduct) {
	constexpr mat3 mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	constexpr mat3 mat2{9, 8, 7, 6, 5, 4, 3, 2, 1};
	mat3 madd = mat1 * mat2;
	EXPECT_NEAR(madd(1, 1), 69, 0.0001);
	constexpr mat3 mat3{1, 0, 0, 0, 1, 0, 0, 0, 1};
	madd = mat1 * mat3;
	EXPECT_NEAR(madd(1, 1), 5, 0.0001);
	madd *= mat1;
	EXPECT_NEAR(madd(1, 1), 81, 0.0001);
}

TEST(math, matrixVector) {
	constexpr mat3 mat{1, 2, 3, 2, 4, 5, 3, 5, 6};
	constexpr vec3 vec{1, 2, 3};
	const vec3 result = mat * vec;
	const vec3 tluser = vec * mat;
	EXPECT_NEAR(result.x(), tluser.x(), 0.0001f);
	EXPECT_NEAR(result.y(), tluser.y(), 0.0001f);
	EXPECT_NEAR(result.z(), tluser.z(), 0.0001f);
	const mat3 dmat = inverse(mat);
	const vec3 dresu = dmat * result;
	EXPECT_NEAR(dresu.x(), vec.x(), 0.0001f);
	EXPECT_NEAR(dresu.y(), vec.y(), 0.0001f);
	EXPECT_NEAR(dresu.z(), vec.z(), 0.0001f);
	const vec3 duser = tluser * dmat;
	EXPECT_NEAR(duser.x(), vec.x(), 0.0001f);
	EXPECT_NEAR(duser.y(), vec.y(), 0.0001f);
	EXPECT_NEAR(duser.z(), vec.z(), 0.0001f);
	const mat3 ident = mat * dmat;
	EXPECT_NEAR(ident(0, 0), 1, 0.0001f);
	EXPECT_NEAR(ident(1, 1), 1, 0.0001f);
	EXPECT_NEAR(ident(2, 2), 1, 0.0001f);
	EXPECT_NEAR(ident(0, 1), 0, 0.0001f);
	EXPECT_NEAR(ident(0, 2), 0, 0.0001f);
	EXPECT_NEAR(ident(1, 2), 0, 0.0001f);
	EXPECT_NEAR(ident(1, 0), 0, 0.0001f);
	EXPECT_NEAR(ident(2, 0), 0, 0.0001f);
	EXPECT_NEAR(ident(2, 1), 0, 0.0001f);
}

TEST(math, matrixInverse) {
	{
		constexpr mat2 mat{1, 18, 18, 45};
		const mat2 dmat = inverse(mat);
		const mat2 ident = mat * dmat;
		EXPECT_NEAR(ident(0, 0), 1, 0.0001f);
		EXPECT_NEAR(ident(1, 1), 1, 0.0001f);
		EXPECT_NEAR(ident(0, 1), 0, 0.0001f);
		EXPECT_NEAR(ident(1, 0), 0, 0.0001f);
	}
	{
		constexpr mat4 mat{1, 18, 4, 45, 18, 7, 69, 71, 4, 69, 32, 2, 45, 71, 2, 42};
		const mat4 dmat = inverse(mat);
		const mat4 ident = mat * dmat;
		EXPECT_NEAR(ident(0, 0), 1, 0.0001f);
		EXPECT_NEAR(ident(1, 1), 1, 0.0001f);
		EXPECT_NEAR(ident(2, 2), 1, 0.0001f);
		EXPECT_NEAR(ident(3, 3), 1, 0.0001f);
		EXPECT_NEAR(ident(0, 1), 0, 0.0001f);
		EXPECT_NEAR(ident(0, 2), 0, 0.0001f);
		EXPECT_NEAR(ident(0, 3), 0, 0.0001f);
		EXPECT_NEAR(ident(1, 0), 0, 0.0001f);
		EXPECT_NEAR(ident(1, 2), 0, 0.0001f);
		EXPECT_NEAR(ident(1, 3), 0, 0.0001f);
		EXPECT_NEAR(ident(2, 0), 0, 0.0001f);
		EXPECT_NEAR(ident(2, 1), 0, 0.0001f);
		EXPECT_NEAR(ident(2, 3), 0, 0.0001f);
		EXPECT_NEAR(ident(3, 0), 0, 0.0001f);
		EXPECT_NEAR(ident(3, 1), 0, 0.0001f);
		EXPECT_NEAR(ident(3, 2), 0, 0.0001f);
	}
}

TEST(math, matrixNormIsFrobenius) {
	// Not symmetric: sum of a_ij^2, not of a_ij * a_ji.
	constexpr mat2 mat{1, 0, 5, 1};
	EXPECT_NEAR(mat.norm(), std::sqrt(27.f), 0.0001f);
	// Not square: every component counts once, none is read out of range.
	constexpr Matrix<float, 2, 3> wide{1, 2, 3, 4, 5, 6};
	EXPECT_NEAR(wide.norm(), std::sqrt(91.f), 0.0001f);
}
