/**
 * @file time_test.cpp
 * @author Silmaen
 * @date 03/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Timestep.h>

using namespace owl::core;

TEST(TimeStep, update) {
	Timestep ts;
#if defined(OWL_SANITIZER) || defined(OWL_COVERAGE) || defined(OWL_STACKTRACE)
	const float millis = 200.f;// due to instrumentation slowness, can be slow.
#elif defined(OWL_PLATFORM_ARM64)
	const float millis = 10.f;// due to some emulation slowness.
#else
	constexpr float millis = 1.f;
#endif
	EXPECT_LT(ts.getSeconds(), 0.001f * millis);// there can be a little delay.
	EXPECT_LT(ts.getMilliseconds(), millis);// there can be a little delay.
	EXPECT_EQ(ts.getFrameNumber(), 1);
	ts.update();
	EXPECT_GT(ts.getFps(), 0);
	EXPECT_EQ(ts.getFrameNumber(), 2);
	EXPECT_GT(ts.getStabilizedFps(), 0);
}
