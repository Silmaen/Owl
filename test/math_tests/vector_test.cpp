
#include "testHelper.h"

#include <math/vectors.h>

using namespace owl::math;

TEST(math, vectorsBase) {
	vec2 vect{4, 5};
	EXPECT_TRUE(vect == vec2({4, 5}));
	EXPECT_FALSE(vect == vec2({4, 7}));
	EXPECT_FALSE(vect != vec2({4, 5}));
	EXPECT_TRUE(vect != vec2({4, 7}));
	EXPECT_NEAR(vect.x(), 4, 0.001);
	EXPECT_NEAR(vect.y(), 5, 0.001);
}
TEST(math, vectorsAddition) {
	constexpr vec2 vect{4, 5};
	constexpr vec2 vect2{1, 2};
	EXPECT_NEAR((vect + vect2).x(), 5, 0.0001);
	EXPECT_NEAR((vect + vect2).y(), 7, 0.0001);
	EXPECT_NEAR((vect - vect2).x(), 3, 0.0001);
	EXPECT_NEAR((vect - vect2).y(), 3, 0.0001);
}

TEST(math, vectorsProduct) {
	vec2 vect{4, 5};
	EXPECT_NEAR(vect.normSq(), 41, 0.001);
	EXPECT_NEAR(vect.norm(), 6.40312, 0.001);
	EXPECT_NEAR(vect.normalized().norm(), 1.0, 0.001);
	EXPECT_NEAR(vect.normSq() * 2, 82, 0.001);
	EXPECT_NEAR(2 * vect.normSq(), 82, 0.001);
	EXPECT_NEAR(vect.normSq() / 2, 20.5, 0.001);
	EXPECT_NEAR((3 * vect).x(), 12, 0.001);
	EXPECT_NEAR((vect * 3).x(), 12, 0.001);
	EXPECT_NEAR((vect / 2).x(), 2, 0.001);
	vect = {0, 0};
	EXPECT_NEAR(vect.normalized().norm(), 0.0, 0.001);
}

TEST(math, othertype) {
	vec3ui vect{4, 5, 1};
	vect.normalize();
	EXPECT_EQ(vect.norm(), 1);
	vec3i vecti{4, 5, 1};
	vecti.normalize();
	EXPECT_EQ(vecti.norm(), 1);
	EXPECT_EQ(vecti.y(), 1);
	vec3i vecti2{4, -5, 1};
	vecti2.normalize();
	EXPECT_EQ(vecti2.norm(), 1);
	EXPECT_EQ(vecti2.y(), -1);
}

TEST(math, vectorsComponents) {
	vec4 vect{4, 5, 6, 7};
	EXPECT_NEAR(vect.x(), 4, 0.0001);
	EXPECT_NEAR(vect.y(), 5, 0.0001);
	EXPECT_NEAR(vect.z(), 6, 0.0001);
	EXPECT_NEAR(vect.w(), 7, 0.0001);
	EXPECT_NEAR(vect.b(), 6, 0.0001);
	EXPECT_NEAR(vect.a(), 7, 0.0001);
	vect.x() = 36;
	EXPECT_NEAR(vect.r(), 36, 0.0001);
	vect[1] = 36;
	EXPECT_NEAR(vect.g(), 36, 0.0001);
	constexpr vec4 cvect{4, 5, 6, 7};
	EXPECT_NEAR(cvect.x(), 4, 0.0001);
	EXPECT_NEAR(cvect.y(), 5, 0.0001);
	EXPECT_NEAR(cvect.z(), 6, 0.0001);
	EXPECT_NEAR(cvect.w(), 7, 0.0001);
	EXPECT_NEAR(cvect.r(), 4, 0.0001);
	EXPECT_NEAR(cvect.g(), 5, 0.0001);
	EXPECT_NEAR(cvect.b(), 6, 0.0001);
	EXPECT_NEAR(cvect.a(), 7, 0.0001);
	float accu = 0;
	for (auto& v: vect) accu += v;
	EXPECT_EQ(accu, 85);
	float acc2 = 0;
	for (const auto& v: cvect) acc2 += v;
	EXPECT_EQ(acc2, 22);
}


TEST(math, vectorCrossProduct) {
	vec3 test{1, 2, 3};
	constexpr vec3 test2{3, 4, 1};
	vec3 res = test ^ test2;
	EXPECT_NEAR(res.x(), -10, 0.001);
	EXPECT_NEAR(res.y(), 8, 0.001);
	EXPECT_NEAR(res.z(), -2, 0.001);
}
