
#include "testHelper.h"

#include <math/trigonometry.h>

using namespace owl::math;

TEST(math, atan2) { EXPECT_NEAR(std::atan2(1.f, 2.f), 0.463647604, 0.001); }
