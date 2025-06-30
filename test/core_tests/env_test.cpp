
#include "testHelper.h"

#include <core/Environment.h>

using namespace owl::core;

TEST(Environement, variables) {
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "");
	appendEnv("BBOOBBOOBB", "yo");
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), "yo");
	appendEnv("BBOOBBOOBB", "lo");
	const std::string uhu = std::format("lo{}yo", g_sep);
	EXPECT_STREQ(getEnv("BBOOBBOOBB").c_str(), uhu.c_str());
}
