/**
 * @file simpleFunction_test.cpp
 * @author Silmaen
 * @date 02/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/simpleFunctions.h>

using namespace owl::math;

TEST(math, clamp) {
	EXPECT_EQ(clamp(150, 20, 200), 150);
	EXPECT_EQ(clamp(10, 20, 200), 20);
	EXPECT_EQ(clamp(300, 20, 200), 200);
	EXPECT_EQ(clamp(10, 200, 20), 200);
	EXPECT_EQ(clamp(300, 200, 20), 20);
}

TEST(math, clampSafe) {
	EXPECT_EQ(clampSafe(150, 20, 200), 150);
	EXPECT_EQ(clampSafe(10, 20, 200), 20);
	EXPECT_EQ(clampSafe(300, 20, 200), 200);
	EXPECT_EQ(clampSafe(10, 200, 20), 20);
	EXPECT_EQ(clampSafe(300, 200, 20), 200);
}

TEST(math, heaviside) {
	EXPECT_EQ(heaviside(150), 1);
	EXPECT_EQ(heaviside(-20), 0);
	EXPECT_EQ(heaviside(0), 0);
	EXPECT_NEAR(heaviside(150.0), 1.0, 1e-15);
	EXPECT_NEAR(heaviside(-20.0), 0.0, 1e-15);
	EXPECT_NEAR(heaviside(0.0), 0.0, 1e-15);
}

TEST(math, sign) {
	EXPECT_EQ(sign(150), 1);
	EXPECT_EQ(sign(-20), -1);
	EXPECT_EQ(sign(0), 0);
	EXPECT_NEAR(sign(150.0), 1.0, 1e-15);
	EXPECT_NEAR(sign(-20.0), -1.0, 1e-15);
	EXPECT_NEAR(sign(0.0), 0.0, 1e-15);
}
