#include "testHelper.h"

#include "mathHelpers.h"

using namespace owl;

TEST(math, quaternionBase) {
	const math::quat q1{1, 2, 3, 4};
	math::quat q2{4, 3, 2, 1};
	EXPECT_TRUE(q1 == math::quat({1, 2, 3, 4}));
	EXPECT_FALSE(q1 != math::quat({1, 2, 3, 4}));
	EXPECT_TRUE(q1 != math::quat({1, 3, 2, 4}));
	EXPECT_FALSE(q1 == math::quat({1, 3, 2, 4}));
	EXPECT_TRUE(q1 + q2 == math::quat({5, 5, 5, 5}));
	EXPECT_TRUE(q1 - q2 == math::quat({-3, -1, 1, 3}));
	q2[2] = 8;
	EXPECT_TRUE(-q2 == math::quat({-4, -3, -2, -8}));
}

TEST(math, quaternionRotation) {
	const math::quat qr({0, 0, 0});
	EXPECT_TRUE(quatCompare(qr, {1, 0, 0, 0}));
	const math::mat4 mat = math::toMat4(qr);
	EXPECT_TRUE(matrixCompare(mat, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}));
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wimplicit-float-conversion")
TEST(math, quaternionProduct) {
	const math::quat qr{1, 2, 3, 4};
	EXPECT_TRUE(quatCompare(qr * 2, {2, 4, 6, 8}));
	EXPECT_TRUE(quatCompare(3 * qr, {3, 6, 9, 12}));
	EXPECT_TRUE(quatCompare(qr / 2, {0.5, 1, 1.5, 2}));
	math::quat qnr = qr.normalized();
	EXPECT_TRUE(quatCompare(qnr, {0.182574183, 0.365148365, 0.547722518, 0.730296731}));
	qnr *= qr;
	EXPECT_TRUE(quatCompare(qnr, {-5.11207676, 0.730296612, 1.09544516, 1.46059358}));
	const math::quat qor{0, 0, 0, 0};
	EXPECT_TRUE(quatCompare(qor, qor.normalized()));
	EXPECT_NEAR(qr.dot(qr), 30, 0.001);
	EXPECT_TRUE(quatCompare(qr.conjugated(), {1, -2, -3, -4}));
}
OWL_DIAG_POP

TEST(math, quaternionInverse) {
	const math::quat qr{1, 2, 3, 4};
	const math::quat iqr = qr.inverted();
	EXPECT_TRUE(quatCompare(qr * iqr, {1, 0, 0, 0}));
	const math::quat qor{0, 0, 0, 0};
	EXPECT_TRUE(quatCompare(qor, qor.inverted()));
}
