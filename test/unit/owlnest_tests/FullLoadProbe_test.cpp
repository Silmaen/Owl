/**
 * @file FullLoadProbe_test.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/FullLoadProbe.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <tuple>

using owl::nest::panel::FullLoadProbe;

namespace {
auto at(const int64_t iMs) -> FullLoadProbe::clock::time_point {
	return FullLoadProbe::clock::time_point{std::chrono::milliseconds(iMs)};
}
}// namespace

TEST(FullLoadProbe, IdleUntilStarted) {
	FullLoadProbe probe;
	EXPECT_FALSE(probe.isRunning());
	EXPECT_FALSE(probe.addFrame(1.0, at(10)));
	EXPECT_FALSE(probe.getResult().has_value());
}

TEST(FullLoadProbe, MeasuresAfterTheWarmup) {
	FullLoadProbe probe;
	probe.start(at(0));
	// Warm-up frames are slow and ignored.
	for (int64_t t = 50; t < 1'000; t += 50) {
		probe.addGpu(9.0);
		EXPECT_FALSE(probe.addFrame(5.0, at(t)));
	}
	EXPECT_FALSE(probe.addFrame(5.0, at(1'000)));// opens the measure
	bool done = false;
	int64_t t = 1'000;
	while (!done) {
		t += 1;// 1000 frames per second
		probe.addGpu(0.25);
		done = probe.addFrame(0.5, at(t));
	}
	EXPECT_FALSE(probe.isRunning());
	ASSERT_TRUE(probe.getResult().has_value());
	const auto& result = *probe.getResult();
	EXPECT_NEAR(result.fps, 1000.0, 1.0);
	EXPECT_NEAR(result.workMs, 0.5, 1e-9);
	ASSERT_TRUE(result.gpuMs.has_value());
	EXPECT_NEAR(*result.gpuMs, 0.25, 1e-9);
}

TEST(FullLoadProbe, RestartForgetsThePreviousSamples) {
	FullLoadProbe probe;
	probe.start(at(0));
	std::ignore = probe.addFrame(3.0, at(1'000));
	std::ignore = probe.addFrame(3.0, at(1'500));
	probe.start(at(2'000));
	EXPECT_TRUE(probe.isRunning());
	std::ignore = probe.addFrame(1.0, at(3'000));
	EXPECT_FALSE(probe.addFrame(1.0, at(4'000)));
	EXPECT_TRUE(probe.addFrame(1.0, at(5'000)));
	ASSERT_TRUE(probe.getResult().has_value());
	EXPECT_NEAR(probe.getResult()->workMs, 1.0, 1e-9);
	EXPECT_FALSE(probe.getResult()->gpuMs.has_value());
}
