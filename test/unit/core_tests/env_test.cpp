/**
 * @file env_test.cpp
 * @author Silmaen
 * @date 30/04/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Environment.h>

#include <format>
#include <string>

using namespace owl::core;

TEST(Environment, variables) {
	EXPECT_EQ(getEnv("BBOOBBOOBB"), "");
	appendEnv("BBOOBBOOBB", "yo");
	// Whole strings: a value carrying its terminating null character must fail.
	EXPECT_EQ(getEnv("BBOOBBOOBB"), "yo");
	appendEnv("BBOOBBOOBB", "lo");
	EXPECT_EQ(getEnv("BBOOBBOOBB"), std::format("lo{}yo", g_sep));
	// Leave the process environment as found so the test can repeat or run in any order.
	setEnv("BBOOBBOOBB", "");
}
