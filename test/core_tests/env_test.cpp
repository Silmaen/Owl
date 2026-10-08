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
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "");
	appendEnv("BBOOBBOOBB", "yo");
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "yo");
	appendEnv("BBOOBBOOBB", "lo");
	const std::string uhu = std::format("lo{}yo", g_sep);
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), uhu.c_str());
	// Leave the process environment as found so the test can repeat or run in any order.
	setEnv("BBOOBBOOBB", "");
}
