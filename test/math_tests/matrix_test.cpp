
#include "testHelper.h"

#include <core/Core.h>

using namespace owl::math;

TEST(math, matrixBase) {
	mat3 const mat{1, 2, 3, 4, 5, 6, 7, 8, 9};
	EXPECT_NEAR(mat.norm(), 16.15549, 0.001);
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
	mat3 const mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	mat3 const mat2{9, 8, 7, 6, 5, 4, 3, 2, 1};
	mat3 madd = mat1 + mat2;
	EXPECT_NEAR(madd(1, 1), 10, 0.0001);
	madd = mat2 - mat1;
	EXPECT_NEAR(madd(1, 1), 0, 0.0001);
}

TEST(math, matrixScalar) {
	mat3 const mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	mat3 madd = mat1 * 2;
	EXPECT_NEAR(madd(1, 1), 10, 0.0001);
	madd = 7 * mat1;
	EXPECT_NEAR(madd(1, 1), 35, 0.0001);
	madd = mat1 / 2;
	EXPECT_NEAR(madd(0, 0), 0.5, 0.0001);
}


TEST(math, matrixProduct) {
	mat3 const mat1{1, 2, 3, 4, 5, 6, 7, 8, 9};
	mat3 const mat2{9, 8, 7, 6, 5, 4, 3, 2, 1};
	mat3 madd = mat1 * mat2;
	EXPECT_NEAR(madd(1, 1), 69, 0.0001);
	mat3 const mat3{1, 0, 0, 0, 1, 0, 0, 0, 1};
	madd = mat1 * mat3;
	EXPECT_NEAR(madd(1, 1), 5, 0.0001);
	madd *= mat1;
	EXPECT_NEAR(madd(1, 1), 81, 0.0001);
}

TEST(math, matrixVector) {
	const mat3 mat{1, 2, 3, 2, 4, 5, 3, 5, 6};
	const vec3 vec{1, 2, 3};
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
		const mat2 mat{1, 18, 18, 45};
		const mat2 dmat = inverse(mat);
		const mat2 ident = mat * dmat;
		EXPECT_NEAR(ident(0, 0), 1, 0.0001f);
		EXPECT_NEAR(ident(1, 1), 1, 0.0001f);
		EXPECT_NEAR(ident(0, 1), 0, 0.0001f);
		EXPECT_NEAR(ident(1, 0), 0, 0.0001f);
	}
	{
		const mat4 mat{1, 18, 4, 45, 18, 7, 69, 71, 4, 69, 32, 2, 45, 71, 2, 42};
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
