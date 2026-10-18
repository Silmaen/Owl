/**
 * @file trigonometry_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/trigonometry.h>

#include <cmath>

using namespace owl::math;

TEST(math, atan2) { EXPECT_NEAR(std::atan2(1.f, 2.f), 0.463647604, 0.001); }
