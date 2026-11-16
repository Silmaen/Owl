/**
 * @file Match_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/Match.h>

#include <limits>

using namespace owl;
using namespace owl::network;

TEST(HardwareProfile, AMachineMeetsTheMinimumOrNot) {
	const HardwareProfile machine{.cpuThreads = 8, .frameMs = 12.f};
	EXPECT_TRUE(machine.meets({}));
	EXPECT_TRUE(machine.meets({.cpuThreads = 8, .frameMs = 16.f}));
	EXPECT_FALSE(machine.meets({.cpuThreads = 12, .frameMs = 0.f}));
	EXPECT_FALSE(machine.meets({.cpuThreads = 0, .frameMs = 10.f}));
	EXPECT_FALSE(HardwareProfile{}.meets({.cpuThreads = 0, .frameMs = 30.f}));
}

TEST(MatchRules, OutOfRangeRulesAreClamped) {
	MatchRules rules;
	rules.maxPlayers = 0;
	rules.inputDelay = 1000;
	rules.snapshotRate = std::numeric_limits<float>::quiet_NaN();
	rules.interpolationDelay = -1.f;
	const auto clamped = rules.clamped();
	EXPECT_EQ(clamped.maxPlayers, 1U);
	EXPECT_EQ(clamped.inputDelay, 30U);
	EXPECT_FLOAT_EQ(clamped.snapshotRate, MatchRules{}.snapshotRate);
	EXPECT_FLOAT_EQ(clamped.interpolationDelay, 0.f);
	EXPECT_EQ(MatchRules{}.clamped(), MatchRules{});
}

TEST(MatchRules, TheRulesRoundTripThroughYaml) {
	MatchRules rules;
	rules.model = NetModel::Lockstep;
	rules.maxPlayers = 4;
	rules.crossplay = false;
	rules.minimum = {.cpuThreads = 4, .frameMs = 20.f};
	rules.inputDelay = 5;
	rules.snapshotRate = 30.f;
	EXPECT_EQ(MatchRules::fromYaml(rules.toYaml()), rules);
	EXPECT_EQ(MatchRules::fromYaml("maxPlayers: 2").maxPlayers, 2U);
	EXPECT_EQ(MatchRules::fromYaml("[: broken"), MatchRules{});
}

TEST(MatchRules, TheVersionNumberIsTheSameEverywhere) {
	EXPECT_EQ(getVersionNumber(""), 2166136261U);
	EXPECT_EQ(getVersionNumber("a"), 0xe40c292cU);
	EXPECT_NE(getVersionNumber("1.0.0"), getVersionNumber("1.0.1"));
}
