/**
 * @file GpuCounterMonitor_test.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/GpuCounterMonitor.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

using namespace owl;
using owl::nest::panel::GpuCounterMonitor;

namespace {

auto at(const int64_t iMs) -> GpuCounterMonitor::clock::time_point {
	return GpuCounterMonitor::clock::time_point{std::chrono::milliseconds(iMs)};
}

auto counters(const uint64_t iSubmits, const uint64_t iQueueWaits, const double iPaceMs)
		-> renderer::gpu::RenderCounters {
	return {.drawCalls = 0,
			.submits = iSubmits,
			.queueWaitIdles = iQueueWaits,
			.deviceWaitIdles = iQueueWaits / 2,
			.fenceWaits = iSubmits / 2,
			.paceWaitMs = iPaceMs};
}

}// namespace

TEST(GpuCounterMonitor, FirstUpdateOnlySetsTheBaseline) {
	GpuCounterMonitor monitor;
	monitor.update(counters(100, 40, 50.0), at(1'000));
	EXPECT_EQ(monitor.getLast().submits, 0u);
	EXPECT_EQ(monitor.getWindowMax().queueWaitIdles, 0u);
	monitor.update(counters(102, 40, 51.5), at(1'016));
	EXPECT_EQ(monitor.getLast().submits, 2u);
	EXPECT_EQ(monitor.getLast().fenceWaits, 1u);
	EXPECT_EQ(monitor.getLast().queueWaitIdles, 0u);
	EXPECT_DOUBLE_EQ(monitor.getLast().paceWaitMs, 1.5);
}

TEST(GpuCounterMonitor, OneOffDrainStaysVisibleForTheWindow) {
	GpuCounterMonitor monitor;
	monitor.update(counters(0, 0, 0.0), at(1'000));
	monitor.update(counters(1, 12, 1.0), at(1'016));
	int64_t now = 1'016;
	uint64_t submits = 1;
	for (; now < 2'800; now += 16) monitor.update(counters(++submits, 12, 1.0), at(now + 16));
	EXPECT_EQ(monitor.getLast().queueWaitIdles, 0u);
	EXPECT_EQ(monitor.getWindowMax().queueWaitIdles, 12u);
	EXPECT_EQ(monitor.getWindowMax().deviceWaitIdles, 6u);
	for (; now < 3'400; now += 16) monitor.update(counters(++submits, 12, 1.0), at(now + 16));
	EXPECT_EQ(monitor.getWindowMax().queueWaitIdles, 0u);
	EXPECT_EQ(monitor.getWindowMax().submits, 1u);
}

TEST(GpuCounterMonitor, LongGapClearsTheWindow) {
	GpuCounterMonitor monitor;
	monitor.update(counters(0, 0, 0.0), at(1'000));
	monitor.update(counters(5, 3, 0.0), at(1'016));
	monitor.update(counters(6, 3, 0.0), at(10'000));
	EXPECT_EQ(monitor.getWindowMax().queueWaitIdles, 0u);
	EXPECT_EQ(monitor.getWindowMax().submits, 1u);
}

TEST(GpuCounterMonitor, CountersGoingBackRestartTheBaseline) {
	GpuCounterMonitor monitor;
	monitor.update(counters(50, 5, 10.0), at(1'000));
	monitor.update(counters(2, 0, 0.0), at(1'016));
	EXPECT_EQ(monitor.getLast().submits, 0u);
	monitor.update(counters(3, 0, 0.5), at(1'032));
	EXPECT_EQ(monitor.getLast().submits, 1u);
	monitor.reset();
	monitor.update(counters(10, 0, 0.0), at(1'048));
	EXPECT_EQ(monitor.getLast().submits, 0u);
	EXPECT_EQ(monitor.getWindowMax().submits, 0u);
}
