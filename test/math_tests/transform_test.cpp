
#include "testHelper.h"

#include <cmath>
#include <math/Transform.h>

using namespace owl::math;

constexpr bool vecNear(const vec3& a, const vec3& b, float accuracy = 0.001f) {
	return (std::abs(a[0] - b[0]) < accuracy) && (std::abs(a[1] - b[1]) < accuracy) &&
		   (std::abs(a[2] - b[2]) < accuracy);
}

const float pis2 = 2.f * std::atan(1.f);

TEST(math, decomposeTrivial) {
	mat4 mat{};
	vec3 tran{};
	vec3 rot{};
	vec3 scale{};
	// null matrix
	EXPECT_FALSE(decomposeTransform(mat, tran, rot, scale));

	// identity matrix
	mat = identity<float, 4>();
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, {0, 0, 0}));
	EXPECT_TRUE(vecNear(rot, {0, 0, 0}));
	EXPECT_TRUE(vecNear(scale, {1, 1, 1}));

	// pure scale matrix
	mat(0, 0) = 0.1f;
	mat(1, 1) = 0.1f;
	mat(2, 2) = 0.1f;
	mat(3, 3) = 1;
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, {0, 0, 0}));
	EXPECT_TRUE(vecNear(rot, {0, 0, 0}));
	EXPECT_TRUE(vecNear(scale, {0.1f, 0.1f, 0.1f}));
}

TEST(math, decomposeWithPerspective) {
	mat4 mat = identity<float, 4>();
	vec3 tran{};
	vec3 rot{};
	vec3 scale{};
	// first perspective
	mat(3, 0) = 0.1f;
	mat(3, 1) = 0.;
	mat(3, 2) = 0.;
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, {0, 0, 0}));
	EXPECT_TRUE(vecNear(rot, {0, 0, 0}));
	EXPECT_TRUE(vecNear(scale, {1, 1, 1}));
	mat(3, 0) = 0.;
	mat(3, 1) = 0.1f;
	mat(3, 2) = 0.;
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, {0, 0, 0}));
	EXPECT_TRUE(vecNear(rot, {0, 0, 0}));
	EXPECT_TRUE(vecNear(scale, {1, 1, 1}));
	mat(3, 0) = 0.;
	mat(3, 1) = 0.;
	mat(3, 2) = 0.1f;
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, {0, 0, 0}));
	EXPECT_TRUE(vecNear(rot, {0, 0, 0}));
	EXPECT_TRUE(vecNear(scale, {1, 1, 1}));
}

struct ComposeDecomposeData {
	vec3 translation{0.f, 0.f, 0.f};
	vec3 rotation{0.f, 0.f, 0.f};
	vec3 scale{1.f, 1.f, 1.f};
};

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class ComposeDecomposeTest : public TestWithParam<ComposeDecomposeData> {
public:
	mat4 mat;
	vec3 tran;
	vec3 rot;
	vec3 scale;
};
OWL_DIAG_POP

TEST_P(ComposeDecomposeTest, ComposeDecomposeTests) {
	mat = composeTransform(GetParam().translation, GetParam().rotation, GetParam().scale);
	EXPECT_TRUE(decomposeTransform(mat, tran, rot, scale));
	EXPECT_TRUE(vecNear(tran, GetParam().translation));
	EXPECT_TRUE(vecNear(rot, GetParam().rotation));
	EXPECT_TRUE(vecNear(scale, GetParam().scale));
}

static ComposeDecomposeData ComposeDecomposeTestData[] = {
		{vec3{0.f, 0.f, 0.f}, vec3{pis2, 0.f, 0.f}, vec3{1.f, 1.f, 1.f}},
		{vec3{0.f, 0.f, 0.f}, vec3{0.f, pis2, 0.f}, vec3{1.f, 1.f, 1.f}},
		{vec3{0.f, 0.f, 0.f}, vec3{0.f, 0.f, pis2}, vec3{1.f, 1.f, 1.f}},
		{vec3{10.f, -0.2f, 0.7f}, vec3{-pis2 / 2.f, 0.f, pis2}, vec3{1.1f, 0.4f, 1.8f}},
		{vec3{1.f, 0.0f, 0.0f}, vec3{0.f, 0.f, 0.f}, vec3{-1.0f, -1.0f, -1.0f}}};

INSTANTIATE_TEST_SUITE_P(ComposeDecomposeCases, ComposeDecomposeTest, testing::ValuesIn(ComposeDecomposeTestData));
