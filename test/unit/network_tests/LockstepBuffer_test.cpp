/**
 * @file LockstepBuffer_test.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <network/LockstepBuffer.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::network;

namespace {

auto inputOf(const bool iHeld, const float iValue) -> PlayerInput {
	return {input::ActionState{.held = iHeld,
							   .pressed = iHeld,
							   .released = false,
							   .value = iValue,
							   .vector = {0.5f, -1.f}}};
}

}// namespace

TEST(LockstepBuffer, TheFirstTicksPlayWithEmptyInputs) {
	LockstepBuffer buffer;
	const std::array<PlayerId, 2> players{0, 3};
	buffer.reset(players, 2);
	EXPECT_TRUE(buffer.isComplete(0));
	EXPECT_TRUE(buffer.isComplete(1));
	EXPECT_FALSE(buffer.isComplete(2));
	EXPECT_EQ(buffer.getPlayableEnd(0), 2U);
	ASSERT_NE(buffer.get(0, 3), nullptr);
	EXPECT_TRUE(buffer.get(0, 3)->empty());
}

TEST(LockstepBuffer, ATickWaitsForEveryPlayer) {
	LockstepBuffer buffer;
	const std::array<PlayerId, 2> players{0, 1};
	buffer.reset(players, 1);
	buffer.put(1, 0, inputOf(true, 1.f));
	EXPECT_FALSE(buffer.isComplete(1));
	EXPECT_EQ(buffer.getPlayableEnd(0), 1U);
	buffer.put(1, 1, inputOf(false, 0.f));
	EXPECT_TRUE(buffer.isComplete(1));
	EXPECT_EQ(buffer.getPlayableEnd(0), 2U);
	buffer.put(1, 1, inputOf(true, 1.f));
	EXPECT_FALSE(buffer.get(1, 1)->front().held);
}

TEST(LockstepBuffer, ALeavingPlayerNoLongerHoldsTheTicks) {
	LockstepBuffer buffer;
	const std::array<PlayerId, 2> players{0, 1};
	buffer.reset(players, 1);
	buffer.put(1, 0, inputOf(true, 1.f));
	EXPECT_FALSE(buffer.isComplete(1));
	buffer.removePlayer(1);
	EXPECT_TRUE(buffer.isComplete(1));
	EXPECT_EQ(buffer.getPlayers(), std::vector<PlayerId>{0});
}

TEST(LockstepBuffer, PlayedTicksAreDiscarded) {
	LockstepBuffer buffer;
	const std::array<PlayerId, 1> players{0};
	buffer.reset(players, 3);
	buffer.discardBefore(2);
	EXPECT_EQ(buffer.get(1, 0), nullptr);
	EXPECT_NE(buffer.get(2, 0), nullptr);
	buffer.put(0, 0, inputOf(true, 1.f));
	EXPECT_EQ(buffer.get(0, 0), nullptr);
	EXPECT_EQ(buffer.getPlayableEnd(0), 3U);
}

TEST(LockstepBuffer, AnInputCrossesTheNetworkUnchanged) {
	ByteWriter writer;
	const auto sent = inputOf(true, 0.25f);
	writePlayerInput(writer, sent);
	ByteReader reader{writer.getBytes()};
	const auto received = readPlayerInput(reader);
	ASSERT_TRUE(received.has_value());
	ASSERT_EQ(received->size(), 1U);
	EXPECT_TRUE(received->front().held);
	EXPECT_TRUE(received->front().pressed);
	EXPECT_FALSE(received->front().released);
	EXPECT_FLOAT_EQ(received->front().value, 0.25f);
	EXPECT_FLOAT_EQ(received->front().vector.y(), -1.f);
}

TEST(LockstepBuffer, ATruncatedInputIsRefused) {
	ByteWriter writer;
	writePlayerInput(writer, inputOf(true, 1.f));
	const auto bytes = writer.getBytes();
	ByteReader reader{bytes.first(bytes.size() - 1)};
	EXPECT_FALSE(readPlayerInput(reader).has_value());
}
