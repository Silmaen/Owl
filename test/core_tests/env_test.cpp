
#include "testHelper.h"

#include <core/Environment.h>

#include <format>
#include <string>

using namespace owl::core;

TEST(Environement, variables) {
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "");
	appendEnv("BBOOBBOOBB", "yo");
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "yo");
	appendEnv("BBOOBBOOBB", "lo");
	const std::string uhu = std::format("lo{}yo", g_sep);
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), uhu.c_str());
	// Leave the process environment as found so the test can repeat or run in any order.
	setEnv("BBOOBBOOBB", "");
}
