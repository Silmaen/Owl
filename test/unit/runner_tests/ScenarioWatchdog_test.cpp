/**
 * @file ScenarioWatchdog_test.cpp
 * @author Silmaen
 * @date 19/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include "ScenarioWatchdog.h"

#include <chrono>
#include <string>

using namespace owl::nest::runner;
using namespace std::chrono_literals;

TEST(ScenarioWatchdog, ReportsTheStepOfAStalledFrame) {
	const auto start = ScenarioWatchdog::clock::now();
	ScenarioWatchdog watchdog{100ms, 4, start};
	EXPECT_FALSE(watchdog.check(start + 50ms).has_value());
	watchdog.onFrame(0);
	watchdog.onFrame(2);
	EXPECT_FALSE(watchdog.check(start + 150ms).has_value());
	EXPECT_FALSE(watchdog.check(start + 240ms).has_value());
	const auto stall = watchdog.check(start + 2400ms);
	ASSERT_TRUE(stall.has_value());
	EXPECT_TRUE(stall->contains("no progress for 2 s"));
	EXPECT_TRUE(stall->contains("playing, frame 2, step 3/4"));
}

TEST(ScenarioWatchdog, ReportsAStallBeforeTheFirstFrameAndAtShutdown) {
	const auto start = ScenarioWatchdog::clock::now();
	ScenarioWatchdog watchdog{100ms, 1, start};
	const auto starting = watchdog.check(start + 200ms);
	ASSERT_TRUE(starting.has_value());
	EXPECT_TRUE(starting->contains("starting"));
	watchdog.onFrame(0);
	watchdog.onShutdown();
	EXPECT_FALSE(watchdog.check(start + 300ms).has_value());
	const auto shutdown = watchdog.check(start + 500ms);
	ASSERT_TRUE(shutdown.has_value());
	EXPECT_TRUE(shutdown->contains("shutting down"));
	EXPECT_FALSE(shutdown->contains("frame"));
}

TEST(ScenarioWatchdog, LimitIsScaledByAWholeFactor) {
	EXPECT_EQ(ScenarioWatchdog::limitFor(nullptr), ScenarioWatchdog::clock::duration{120s});
	EXPECT_EQ(ScenarioWatchdog::limitFor("8"), ScenarioWatchdog::clock::duration{960s});
	EXPECT_EQ(ScenarioWatchdog::limitFor("0"), ScenarioWatchdog::clock::duration{120s});
	EXPECT_EQ(ScenarioWatchdog::limitFor("fast"), ScenarioWatchdog::clock::duration{120s});
	EXPECT_EQ(ScenarioWatchdog::get(), nullptr);
}
