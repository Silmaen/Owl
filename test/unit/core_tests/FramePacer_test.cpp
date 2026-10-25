/**
 * @file FramePacer_test.cpp
 * @author Silmaen
 * @date 24/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/FramePacer.h>

#include <chrono>
#include <cstdint>

using owl::core::FramePacer;
using namespace std::chrono_literals;

namespace {
auto at(const int64_t iMs) -> FramePacer::clock::time_point {
	return FramePacer::clock::time_point{std::chrono::milliseconds(iMs)};
}
}// namespace

TEST(FramePacer, InactiveByDefault) {
	FramePacer pacer;
	EXPECT_FALSE(pacer.isActive());
	EXPECT_EQ(pacer.next(at(100)), at(100));
	pacer.wait();
}

TEST(FramePacer, SchedulesOnePeriodAfterThePreviousDeadline) {
	FramePacer pacer;
	pacer.setRate(100, at(0));
	EXPECT_TRUE(pacer.isActive());
	EXPECT_EQ(pacer.getPeriod(), 10ms);
	// A frame costing 3 ms waits until the end of its period, the next one is not shifted by the sleep error.
	EXPECT_EQ(pacer.next(at(3)), at(10));
	EXPECT_EQ(pacer.next(at(11)), at(20));
}

TEST(FramePacer, ALateFrameRestartsTheSchedule) {
	FramePacer pacer;
	pacer.setPeriod(10ms, at(0));
	EXPECT_EQ(pacer.next(at(35)), at(35));
	EXPECT_EQ(pacer.next(at(36)), at(45));
}

TEST(FramePacer, ZeroDisablesThePacing) {
	FramePacer pacer;
	pacer.setRate(60, at(0));
	EXPECT_TRUE(pacer.isActive());
	pacer.setRate(0, at(5));
	EXPECT_FALSE(pacer.isActive());
	pacer.setPeriod(-5ms, at(5));
	EXPECT_FALSE(pacer.isActive());
	EXPECT_EQ(pacer.getPeriod(), FramePacer::clock::duration::zero());
}

TEST(FramePacer, WaitSleepsOutThePeriod) {
	FramePacer pacer;
	// The period starts at the instant given to setPeriod: measure from it, a loaded machine may run late after it.
	const auto start = FramePacer::clock::now();
	pacer.setPeriod(20ms, start);
	pacer.wait();
	EXPECT_GE(FramePacer::clock::now() - start, 20ms);
}
