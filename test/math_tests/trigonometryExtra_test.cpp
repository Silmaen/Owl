/**
 * @file trigonometryExtra_test.cpp
 * @author Silmaen
 * @date 06/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/trigonometry.h>

using namespace owl::math;

TEST(math, RadiansDegrees) {
	EXPECT_NEAR(radians(180.f), std::numbers::pi_v<float>, 0.0001f);
	EXPECT_NEAR(radians(90.f), std::numbers::pi_v<float> / 2.f, 0.0001f);
	EXPECT_NEAR(radians(0.f), 0.f, 0.0001f);
	EXPECT_NEAR(degrees(std::numbers::pi_v<float>), 180.f, 0.0001f);
	EXPECT_NEAR(degrees(std::numbers::pi_v<float> / 2.f), 90.f, 0.0001f);
}

TEST(math, RadiansDegreesVector) {
	constexpr vec3 deg{0, 90, 180};
	const vec3 rad = radians<float, 3>(deg);
	EXPECT_NEAR(rad.x(), 0.f, 0.0001f);
	EXPECT_NEAR(rad.y(), std::numbers::pi_v<float> / 2.f, 0.0001f);
	EXPECT_NEAR(rad.z(), std::numbers::pi_v<float>, 0.0001f);

	const vec3 back = degrees<float, 3>(rad);
	EXPECT_NEAR(back.x(), 0.f, 0.001f);
	EXPECT_NEAR(back.y(), 90.f, 0.001f);
	EXPECT_NEAR(back.z(), 180.f, 0.001f);
}

TEST(math, Atan2Values) {
	EXPECT_NEAR(atan2(0.f, 1.f), 0.f, 0.0001f);
	EXPECT_NEAR(atan2(1.f, 0.f), std::numbers::pi_v<float> / 2.f, 0.0001f);
	EXPECT_NEAR(atan2(0.f, -1.f), std::numbers::pi_v<float>, 0.0001f);
	EXPECT_NEAR(atan2(-1.f, 0.f), -std::numbers::pi_v<float> / 2.f, 0.0001f);
}
