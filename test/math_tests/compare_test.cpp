#include "testHelper.h"

#include "mathHelpers.h"

using namespace owl;

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wimplicit-float-conversion")
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
TEST(compare, Matrix) {
	{
		const math::mat4 mmat = math::translate(math::identity<float, 4>(), {14, 15, 16});
		EXPECT_TRUE(matrixCompare(mmat, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 14, 15, 16, 1}));
	}
	{
		const math::mat4 mmat = math::rotate(math::identity<float, 4>(), math::radians(45.f), {1, 0, 0});
		EXPECT_TRUE(matrixCompare(
				mmat, {1, 0, 0, 0, 0, 0.707106769, 0.707106769, 0, 0, -0.707106769, 0.707106769, 0, 0, 0, 0, 1}));
	}
	{
		const math::mat4 mmat = math::rotate(math::identity<float, 4>(), math::radians(45.f), {0, 1, 0});
		EXPECT_TRUE(matrixCompare(
				mmat, {0.707106769, 0, -0.707106769, 0, 0, 1, 0, 0, 0.707106769, 0, 0.707106769, 0, 0, 0, 0, 1}));
	}
	{
		const math::mat4 mmat = math::rotate(math::identity<float, 4>(), math::radians(45.f), {0, 0, 1});
		EXPECT_TRUE(matrixCompare(
				mmat, {0.707106769, 0.707106769, 0, 0, -0.707106769, 0.707106769, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}));
	}
	{
		const math::mat4 mmat = math::scale(math::identity<float, 4>(), {0.1, 0.2, 0.3});
		EXPECT_TRUE(matrixCompare(mmat, {0.1, 0, 0, 0, 0, 0.2, 0, 0, 0, 0, 0.3, 0, 0, 0, 0, 1}));
	}
	{
		const math::mat4 mmat = math::translate(math::identity<float, 4>(), {14, 15, 16}) *
								math::rotate(math::identity<float, 4>(), math::radians(45.f), {1, 0, 0}) *
								math::rotate(math::identity<float, 4>(), math::radians(45.f), {0, 1, 0}) *
								math::rotate(math::identity<float, 4>(), math::radians(45.f), {0, 0, 1}) *
								math::scale(math::identity<float, 4>(), {0.1, 0.2, 0.3});
		EXPECT_TRUE(matrixCompare(mmat, {0.05, 0.085355354, 0.0146446619, 0, -0.1, 0.0292893238, 0.170710668, 0,
										 0.212132037, -0.15, 0.15, 0, 14, 15, 16, 1}));
		math::mat4 gmat;
		float* gg = gmat.data();
		const float* mm = mmat.data();
		for (size_t it = 0; it < 16; ++it) gg[it] = mm[it];
		EXPECT_TRUE(matrixCompare(gmat, mmat));
	}
	{
		const math::mat4 mmat = math::perspective(math::radians(85.f), 1.2f, 0.f, 10.f);
		EXPECT_TRUE(matrixCompare(mmat, {0.909423828, 0, 0, 0, 0, 1.09130859, 0, 0, 0, 0, -1, -1, 0, 0, 0, 0}));
	}
	{
		const math::mat4 mmat = math::ortho(-15.f, 14.f, -4.f, 11.f, -1.f, 10.f);
		EXPECT_TRUE(matrixCompare(mmat, {0.068965517, 0, 0, 0, 0, 0.13333334, 0, 0, 0, 0, -0.181818187, 0, 0.0344827585,
										 -0.466666669, -0.818181813, 1}));
	}
}

TEST(compare, Vector) {
	{
		math::vec4 gmat;
		constexpr math::vec4 mmat{1, 2, 3, 4};
		float* gg = gmat.data();
		const float* mm = mmat.data();
		for (size_t it = 0; it < 4; ++it) gg[it] = mm[it];
		EXPECT_TRUE(vectorCompare(gmat, mmat));
	}
}
OWL_DIAG_POP
