/**
 * @file FrameMeter_test.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/FrameMeter.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

using owl::nest::panel::FrameMeter;

namespace {

auto at(const int64_t iMs) -> FrameMeter::clock::time_point {
	return FrameMeter::clock::time_point{std::chrono::milliseconds(iMs)};
}

/// Feed one window of frames 10 ms apart (100 per second), each costing @p iWorkMs.
void feedWindow(FrameMeter& ioMeter, const int64_t iStartMs, const double iWorkMs) {
	for (int64_t t = iStartMs; t <= iStartMs + 500; t += 10) ioMeter.addFrame(iWorkMs, at(t));
}

}// namespace

TEST(FrameMeter, NothingPublishedBeforeTheFirstWindowEnds) {
	FrameMeter meter;
	meter.addFrame(2.0, at(1'000));
	meter.addFrame(2.0, at(1'100));
	EXPECT_DOUBLE_EQ(meter.getWorkMs(), 0.0);
	EXPECT_DOUBLE_EQ(meter.getDisplayFps(), 0.0);
	EXPECT_DOUBLE_EQ(meter.getPotentialFps(), 0.0);
	EXPECT_FALSE(meter.getGpuMs().has_value());
}

TEST(FrameMeter, SeparatesTheFrameCostFromTheDisplayRate) {
	FrameMeter meter;
	feedWindow(meter, 0, 0.5);
	EXPECT_NEAR(meter.getWorkMs(), 0.5, 1e-9);
	EXPECT_NEAR(meter.getDisplayFps(), 102.0, 1e-9);
	EXPECT_NEAR(meter.getPotentialFps(), 2000.0, 1e-6);
}

TEST(FrameMeter, TheSlowerOfCpuAndGpuBoundsThePotentialRate) {
	FrameMeter meter;
	meter.addGpu(4.0);
	meter.addGpu(2.0);
	feedWindow(meter, 0, 1.0);
	ASSERT_TRUE(meter.getGpuMs().has_value());
	EXPECT_NEAR(*meter.getGpuMs(), 3.0, 1e-9);
	EXPECT_NEAR(meter.getPotentialFps(), 1000.0 / 3.0, 1e-6);
}

TEST(FrameMeter, AWindowWithoutGpuTimingKeepsTheLastOne) {
	FrameMeter meter;
	meter.addGpu(2.0);
	feedWindow(meter, 0, 1.0);
	feedWindow(meter, 510, 1.0);
	ASSERT_TRUE(meter.getGpuMs().has_value());
	EXPECT_NEAR(*meter.getGpuMs(), 2.0, 1e-9);
}

TEST(FrameMeter, NegativeSamplesCountAsZeroAndResetForgetsEverything) {
	FrameMeter meter;
	meter.addGpu(-1.0);
	feedWindow(meter, 0, -3.0);
	EXPECT_DOUBLE_EQ(meter.getWorkMs(), 0.0);
	EXPECT_DOUBLE_EQ(meter.getPotentialFps(), 0.0);
	meter.reset();
	EXPECT_FALSE(meter.getGpuMs().has_value());
	EXPECT_DOUBLE_EQ(meter.getDisplayFps(), 0.0);
}
