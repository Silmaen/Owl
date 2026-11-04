/**
 * @file ReplayFile_test.cpp
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/KeyCodes.h>
#include <input/MouseCode.h>
#include <scene/Replay.h>

#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

using namespace owl;
using namespace owl::scene;

namespace {

class ReplayFileTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override { core::Log::invalidate(); }

	// A replay touching every recorded field, with floats that a short decimal would not give back.
	[[nodiscard]] static auto makeReplay() -> Replay {
		Replay replay;
		replay.scene = "scenes/level 1.owl";
		replay.seed = 0xfedcba9876543210ull;
		replay.tickRate = 50.f;
		replay.maxTicksPerFrame = 5;
		replay.start.setKey(input::key::LeftShift, true);
		replay.start.cursor = {1.f / 3.f, 2.f};
		replay.startHash = 0x0123456789abcdefull;
		for (int index = 0; index < 40; ++index) {
			ReplayFrame frame;
			frame.duration = std::chrono::nanoseconds{index < 30 ? 16'666'667 : 33'333'334 + index};
			frame.input = replay.start;
			if (index >= 5 && index < 9)
				frame.input.setKey(input::key::Space, true);
			if (index >= 3)
				frame.input.setKey(input::key::D0, true);
			if (index == 7)
				frame.input.keys.set(5);
			if (index == 12)
				frame.input.wheel = {0.f, -1.f};
			if (index == 13)
				frame.input.wheel = {0.f, -1.f};
			if (index >= 10)
				frame.input.setMouseButton(input::mouse::ButtonRight, true);
			frame.input.cursor = {std::nextafter(static_cast<float>(index) * 0.1f, 10.f), 7.25f};
			if (index >= 20) {
				auto& pad = frame.input.gamepads[2];
				pad.connected = true;
				pad.buttons.set(1);
				pad.axes[0] = std::sin(static_cast<float>(index));
			}
			frame.cursorCaptured = index >= 25;
			if (index < 15)
				frame.pointer = ReplayPointer{.viewport = {800, 600},
											  .position = {static_cast<float>(index) + 0.3f, 1e-7f},
											  .pressed = index % 4 == 0,
											  .wheel = {0.f, index == 2 ? 1.f : 0.f},
											  .deltaTime = 1.f / 60.f};
			replay.frames.push_back(frame);
		}
		replay.checkpoints = {{.frame = 12, .worldHash = 0xffffffffffffffffull}, {.frame = 30, .worldHash = 42}};
		return replay;
	}

	static void expectSameInput(const input::InputState& iLeft, const input::InputState& iRight) {
		EXPECT_EQ(iLeft.keys, iRight.keys);
		EXPECT_EQ(iLeft.mouseButtons, iRight.mouseButtons);
		EXPECT_EQ(std::bit_cast<uint32_t>(iLeft.cursor.x()), std::bit_cast<uint32_t>(iRight.cursor.x()));
		EXPECT_EQ(std::bit_cast<uint32_t>(iLeft.cursor.y()), std::bit_cast<uint32_t>(iRight.cursor.y()));
		EXPECT_EQ(iLeft.wheel, iRight.wheel);
		EXPECT_EQ(iLeft.gamepads, iRight.gamepads);
	}
};

}// namespace

TEST_F(ReplayFileTest, RoundTripsEveryFieldToTheBit) {
	const Replay replay = makeReplay();
	const auto text = replay.toYaml();
	const auto loaded = Replay::fromYaml(text);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_EQ(loaded->scene, replay.scene);
	EXPECT_EQ(loaded->seed, replay.seed);
	EXPECT_EQ(std::bit_cast<uint32_t>(loaded->tickRate), std::bit_cast<uint32_t>(replay.tickRate));
	EXPECT_EQ(loaded->maxTicksPerFrame, replay.maxTicksPerFrame);
	EXPECT_EQ(loaded->startHash, replay.startHash);
	expectSameInput(loaded->start, replay.start);
	EXPECT_EQ(loaded->checkpoints, replay.checkpoints);
	ASSERT_EQ(loaded->frames.size(), replay.frames.size());
	for (std::size_t index = 0; index < replay.frames.size(); ++index) {
		SCOPED_TRACE(index);
		const auto& expected = replay.frames[index];
		const auto& actual = loaded->frames[index];
		EXPECT_EQ(actual.duration, expected.duration);
		expectSameInput(actual.input, expected.input);
		EXPECT_EQ(actual.cursorCaptured, expected.cursorCaptured);
		EXPECT_EQ(actual.pointer, expected.pointer);
	}
	EXPECT_DOUBLE_EQ(loaded->getSeconds(), replay.getSeconds());
}

TEST_F(ReplayFileTest, WritesOnlyTheChanges) {
	const auto text = makeReplay().toYaml();
	EXPECT_NE(text.find("FormatVersion"), std::string::npos);
	// Thirty identical durations make one run.
	EXPECT_NE(text.find("[16666667, 30]"), std::string::npos);
	EXPECT_NE(text.find("Space"), std::string::npos);
	// A code without a key name is written as its number.
	EXPECT_NE(text.find("keys: [5, Space"), std::string::npos);
}

TEST_F(ReplayFileTest, AnEmptyReplayRoundTrips) {
	const auto loaded = Replay::fromYaml(Replay{}.toYaml());
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_TRUE(loaded->frames.empty());
	EXPECT_EQ(loaded->getSeconds(), 0.0);
}

TEST_F(ReplayFileTest, RefusesANewerFormatAndBrokenEvents) {
	EXPECT_FALSE(Replay::fromYaml("FormatVersion: 99\n").has_value());
	EXPECT_FALSE(Replay::fromYaml("- a list\n").has_value());
	EXPECT_FALSE(Replay::fromYaml("durations: [[1000, 2]]\nevents: [{frame: 5, keys: [Space]}]\n").has_value());
	EXPECT_FALSE(Replay::fromYaml("durations: [[1000, 2]]\nevents: [{frame: 0, keys: [NoSuchKey]}]\n").has_value());
	EXPECT_FALSE(Replay::loadFile("/no/such/file.owlreplay").has_value());
}
