/**
 * @file RandomStream_test.cpp
 * @author Silmaen
 * @date 03/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/RandomStream.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

using namespace owl::math;

TEST(RandomStream, MatchesTheReferenceSequenceOnEveryTarget) {
	RandomStream stream{0};
	EXPECT_EQ(stream.nextU64(), 0x99ec5f36cb75f2b4ull);
	EXPECT_EQ(stream.nextU64(), 0xbf6e1f784956452aull);
	EXPECT_EQ(stream.nextU64(), 0x1a5f849d4933e6e0ull);
	EXPECT_EQ(RandomStream::hashName("lua"), 0x12a991191dfbb001ull);
	RandomStream named{42, "lua"};
	EXPECT_EQ(named.nextU64(), 0xd37f4a3501ba0abeull);
	EXPECT_EQ(named.nextU64(), 0x1eb0dbcea21bffbfull);
}

TEST(RandomStream, NamesGiveIndependentSequences) {
	RandomStream first{7, "owl.entity"};
	RandomStream second{7, "lua"};
	EXPECT_NE(first.nextU64(), second.nextU64());
	EXPECT_EQ(RandomStream(7, "lua"), RandomStream(7, "lua"));
}

TEST(RandomStream, StateRoundTripsAndZeroStateRestartsSeedZero) {
	RandomStream stream{123};
	std::ignore = stream.nextU64();
	const auto state = stream.getState();
	const auto expected = stream.nextU64();
	RandomStream copy;
	copy.setState(state);
	EXPECT_EQ(copy.nextU64(), expected);
	copy.setState({});
	EXPECT_EQ(copy, RandomStream{0});
	copy.reseed(123);
	EXPECT_EQ(copy, RandomStream{123});
}

TEST(RandomStream, ValuesStayInTheirRanges) {
	RandomStream stream{99};
	for (int i = 0; i < 2000; ++i) {
		const float unit = stream.nextFloat();
		EXPECT_GE(unit, 0.f);
		EXPECT_LT(unit, 1.f);
		const double precise = stream.nextDouble();
		EXPECT_GE(precise, 0.0);
		EXPECT_LT(precise, 1.0);
		const auto dice = stream.nextInt(1, 6);
		EXPECT_GE(dice, 1);
		EXPECT_LE(dice, 6);
		const auto swapped = stream.nextInt(3, -3);
		EXPECT_GE(swapped, -3);
		EXPECT_LE(swapped, 3);
		const float ranged = stream.nextRange(-2.f, 2.f);
		EXPECT_GE(ranged, -2.f);
		EXPECT_LT(ranged, 2.f);
	}
	EXPECT_EQ(stream.nextInt(5, 5), 5);
	std::ignore = stream.nextInt(INT64_MIN, INT64_MAX);
}

TEST(RandomStream, IntegersCoverTheWholeInterval) {
	RandomStream stream{1};
	std::array<int, 6> counts{};
	for (int i = 0; i < 6000; ++i) ++counts[static_cast<size_t>(stream.nextInt(0, 5))];
	for (const int count: counts) {
		EXPECT_GT(count, 850);
		EXPECT_LT(count, 1150);
	}
}
