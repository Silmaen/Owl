/**
 * @file Sequence_test.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <data/sequence/Sequence.h>
#include <data/sequence/SequenceLibrary.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::data::sequence;

namespace {

auto makeKey(const float iTime, const math::vec3& iPosition, const Easing iEasing = Easing::Linear) -> CameraKey {
	return {.time = iTime, .position = iPosition, .easing = iEasing};
}

auto makeTrack(std::vector<CameraKey> iKeys) -> CameraTrack {
	return {.name = "Shot", .keys = std::move(iKeys), .muted = false};
}

}// namespace

TEST(Sequence, RoundTripsThroughYaml) {
	core::Log::init(core::Log::Level::Off);
	Sequence sequence;
	sequence.duration = 7.5f;
	CameraTrack track = makeTrack({makeKey(0.f, {1.f, 2.f, 3.f}, Easing::Step), makeKey(4.f, {0.f, 0.f, -2.f})});
	track.keys[1].rotation = {0.1f, 0.2f, 0.3f};
	track.keys[1].fov = 0.5f;
	track.keys[1].size = 4.f;
	track.muted = true;
	sequence.cameraTracks.push_back(track);
	sequence.eventTracks.push_back(
			{.name = "Cues", .keys = {{.time = 1.f, .function = "on_cue"}, {.time = 2.f, .function = "on_end"}}});
	const auto read = Sequence::deserializeFromString(sequence.serializeToString());
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, sequence);
	core::Log::invalidate();
}

TEST(Sequence, ReadingSortsKeysAndRejectsOtherFiles) {
	core::Log::init(core::Log::Level::Off);
	const auto read = Sequence::deserializeFromString("Sequence: ''\n"
													  "duration: 3\n"
													  "cameraTracks:\n"
													  "  - name: A\n"
													  "    keys:\n"
													  "      - {time: 2, position: [2, 0, 0]}\n"
													  "      - {time: 1, position: [1, 0, 0], easing: Step}\n"
													  "eventTracks:\n"
													  "  - name: E\n"
													  "    keys:\n"
													  "      - {time: 2, function: b}\n"
													  "      - {time: 0.5, function: a}\n");
	ASSERT_TRUE(read.has_value());
	ASSERT_EQ(read->cameraTracks.size(), 1u);
	EXPECT_FLOAT_EQ(read->cameraTracks[0].keys[0].time, 1.f);
	EXPECT_EQ(read->cameraTracks[0].keys[0].easing, Easing::Step);
	EXPECT_EQ(read->cameraTracks[0].keys[1].easing, Easing::Smooth);
	EXPECT_EQ(read->eventTracks[0].keys[0].function, "a");
	EXPECT_FALSE(Sequence::deserializeFromString("Dialogue: ''\n").has_value());
	EXPECT_FALSE(Sequence::deserializeFromString("{ not yaml").has_value());
	EXPECT_FALSE(Sequence::deserializeFromString("Sequence: ''\nFormatVersion: 99\n").has_value());
	EXPECT_TRUE(Sequence::isSequence("cutscenes/intro.owlseq"));
	EXPECT_FALSE(Sequence::isSequence("cutscenes/intro.owldlg"));
	core::Log::invalidate();
}

TEST(Sequence, EasingsLinearStepAndSmooth) {
	const auto linear = makeTrack({makeKey(0.f, {0.f, 0.f, 0.f}), makeKey(2.f, {4.f, 0.f, 0.f})});
	EXPECT_NEAR(linear.sample(1.f)->position.x(), 2.f, 1e-5f);
	EXPECT_NEAR(linear.sample(-1.f)->position.x(), 0.f, 1e-5f);
	EXPECT_NEAR(linear.sample(5.f)->position.x(), 4.f, 1e-5f);

	const auto step = makeTrack({makeKey(0.f, {0.f, 0.f, 0.f}, Easing::Step), makeKey(2.f, {4.f, 0.f, 0.f})});
	EXPECT_NEAR(step.sample(1.9f)->position.x(), 0.f, 1e-5f);
	EXPECT_NEAR(step.sample(2.f)->position.x(), 4.f, 1e-5f);

	const auto smooth =
			makeTrack({makeKey(0.f, {0.f, 0.f, 0.f}, Easing::Smooth), makeKey(1.f, {1.f, 0.f, 0.f}, Easing::Smooth),
					   makeKey(2.f, {2.f, 0.f, 0.f}, Easing::Smooth)});
	// Through every key, eased at both ends, straight in the middle of evenly spaced keys.
	EXPECT_NEAR(smooth.sample(1.f)->position.x(), 1.f, 1e-5f);
	EXPECT_LT(smooth.sample(0.1f)->position.x(), 0.1f);
	EXPECT_GT(smooth.sample(1.9f)->position.x(), 1.9f);
	const float speed = (smooth.sample(1.001f)->position.x() - smooth.sample(0.999f)->position.x()) / 0.002f;
	EXPECT_NEAR(speed, 1.f, 0.02f);
	EXPECT_FALSE(CameraTrack{}.sample(1.f).has_value());
}

TEST(Sequence, RotationTakesTheShortWay) {
	constexpr float pi = std::numbers::pi_v<float>;
	auto track = makeTrack({makeKey(0.f, {0.f, 0.f, 0.f}), makeKey(1.f, {0.f, 0.f, 0.f})});
	track.keys[0].rotation = {0.f, pi * 0.9f, 0.f};
	track.keys[1].rotation = {0.f, -pi * 0.9f, 0.f};
	// Through the half turn, not back across zero.
	EXPECT_NEAR(std::abs(track.sample(0.5f)->rotation.y()), pi, 1e-4f);
}

TEST(Sequence, ShotsCutHoldAndMute) {
	Sequence sequence;
	sequence.duration = 10.f;
	sequence.cameraTracks.push_back(makeTrack({makeKey(1.f, {1.f, 0.f, 0.f}), makeKey(3.f, {3.f, 0.f, 0.f})}));
	sequence.cameraTracks.push_back(makeTrack({makeKey(5.f, {50.f, 0.f, 0.f}), makeKey(7.f, {70.f, 0.f, 0.f})}));
	sequence.cameraTracks.push_back(makeTrack({makeKey(2.f, {20.f, 0.f, 0.f}), makeKey(2.5f, {25.f, 0.f, 0.f})}));
	EXPECT_EQ(sequence.sampleCamera(0.f)->track, 0u);// before every shot: the earliest holds its first key
	EXPECT_NEAR(sequence.sampleCamera(0.f)->pose.position.x(), 1.f, 1e-5f);
	EXPECT_EQ(sequence.sampleCamera(2.f)->track, 2u);// a later track over an earlier one
	EXPECT_EQ(sequence.sampleCamera(2.8f)->track, 0u);
	EXPECT_EQ(sequence.sampleCamera(4.f)->track, 0u);// a gap: the last shot ended holds
	EXPECT_NEAR(sequence.sampleCamera(4.f)->pose.position.x(), 3.f, 1e-5f);
	EXPECT_EQ(sequence.sampleCamera(6.f)->track, 1u);
	EXPECT_EQ(sequence.sampleCamera(9.f)->track, 1u);
	sequence.cameraTracks[2].muted = true;
	EXPECT_EQ(sequence.sampleCamera(2.f)->track, 0u);
	for (auto& track: sequence.cameraTracks) track.muted = true;
	EXPECT_FALSE(sequence.sampleCamera(2.f).has_value());
}

TEST(Sequence, EventsInASpan) {
	Sequence sequence;
	sequence.eventTracks.push_back({.name = "A",
									.keys = {{.time = 0.f, .function = "first"},
											 {.time = 2.f, .function = "third"},
											 {.time = 3.f, .function = ""}}});
	sequence.eventTracks.push_back({.name = "B", .keys = {{.time = 1.f, .function = "second"}}});
	sequence.eventTracks.push_back({.name = "Muted", .keys = {{.time = 1.f, .function = "never"}}, .muted = true});
	const auto all = sequence.getEvents(0.f, 3.f, true);
	ASSERT_EQ(all.size(), 3u);
	EXPECT_EQ(all[0]->function, "first");
	EXPECT_EQ(all[1]->function, "second");
	EXPECT_EQ(all[2]->function, "third");
	EXPECT_EQ(sequence.getEvents(0.f, 1.f, false).size(), 1u);
	EXPECT_TRUE(sequence.getEvents(1.f, 1.5f, false).empty());
	EXPECT_EQ(sequence.getFunctions(), (std::vector<std::string>{"first", "never", "second", "third"}));
}

TEST(Sequence, DefaultAndLibrary) {
	core::Log::init(core::Log::Level::Off);
	const auto sequence = Sequence::makeDefault();
	ASSERT_EQ(sequence.cameraTracks.size(), 1u);
	EXPECT_EQ(sequence.cameraTracks[0].keys.size(), 2u);
	EXPECT_EQ(sequence.eventTracks.size(), 1u);
	const auto path = std::filesystem::temp_directory_path() / "library.owlseq";
	ASSERT_TRUE(sequence.saveToFile(path));
	SequenceLibrary library;
	const auto first = library.get(path.generic_string());
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(*first, sequence);
	EXPECT_EQ(library.get(path.generic_string()), first);
	EXPECT_EQ(library.getLoadCount(), 1u);
	library.invalidate(path.generic_string());
	EXPECT_NE(library.get(path.generic_string()), nullptr);
	EXPECT_EQ(library.getLoadCount(), 2u);
	EXPECT_EQ(library.get("nowhere.owlseq"), nullptr);
	library.clear();
	EXPECT_TRUE(Sequence::loadFile(path).has_value());
	EXPECT_FALSE(Sequence::loadFile(std::filesystem::temp_directory_path() / "nowhere.owlseq").has_value());
	std::filesystem::remove(path);
	core::Log::invalidate();
}
