/**
 * @file SoundCue_test.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <sound/SoundCue.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::sound;

namespace {

auto makeCue() -> SoundCue {
	return {.variants = {{.file = "sounds/step_a.wav",
						  .trimStart = 0.1f,
						  .trimEnd = 0.6f,
						  .loopStart = 0.f,
						  .loopEnd = 0.f,
						  .weight = 1.f},
						 {.file = "sounds/step_b.wav",
						  .trimStart = 0.f,
						  .trimEnd = 0.f,
						  .loopStart = 0.2f,
						  .loopEnd = 0.4f,
						  .weight = 3.f}},
			.volume = 0.8f,
			.volumeJitter = 0.1f,
			.pitch = 1.2f,
			.pitchJitter = 0.05f,
			.loop = true};
}

}// namespace

TEST(SoundCue, RoundTripsThroughYamlAndFiles) {
	core::Log::init(core::Log::Level::Off);
	const auto cue = makeCue();
	const auto text = cue.serializeToString();
	EXPECT_NE(text.find("FormatVersion: 1"), std::string::npos);
	const auto read = SoundCue::deserializeFromString(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(*read, cue);
	EXPECT_FALSE(SoundCue::deserializeFromString("[unclosed").has_value());
	EXPECT_FALSE(SoundCue::deserializeFromString("Other: {}").has_value());
	EXPECT_FALSE(SoundCue::deserializeFromString("SoundCue: ''\nFormatVersion: 99").has_value());

	const auto path = std::filesystem::temp_directory_path() / "owl_sound_cue.owlsound";
	ASSERT_TRUE(cue.saveToFile(path));
	const auto loaded = SoundCue::load(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, cue);
	std::filesystem::remove(path);
	EXPECT_FALSE(SoundCue::load(path).has_value());
	core::Log::invalidate();
}

TEST(SoundCue, ClampsOutOfRangeValues) {
	const auto read = SoundCue::deserializeFromString(
			"SoundCue: ''\nvolume: -1\npitch: 0\npitchJitter: 4\nvariants: [{file: a.wav, trimStart: -2, weight: -1}]");
	ASSERT_TRUE(read.has_value());
	EXPECT_FLOAT_EQ(read->volume, 0.f);
	EXPECT_GT(read->pitch, 0.f);
	EXPECT_LT(read->pitchJitter, 1.f);
	ASSERT_EQ(read->variants.size(), 1u);
	EXPECT_FLOAT_EQ(read->variants[0].trimStart, 0.f);
	EXPECT_FLOAT_EQ(read->variants[0].weight, 0.f);
}

TEST(SoundCue, PickFollowsTheWeightsAndStaysInTheJitter) {
	core::Log::init(core::Log::Level::Off);
	const auto cue = makeCue();
	size_t heavy = 0;
	constexpr uint64_t draws = 4000;
	for (uint64_t i = 0; i < draws; ++i) {
		const auto pick = cue.pick(i * 0x2545f4914f6cdd1dULL);
		ASSERT_TRUE(pick.has_value());
		ASSERT_LT(pick->variant, cue.variants.size());
		heavy += pick->variant == 1 ? 1u : 0u;
		EXPECT_GE(pick->volume, cue.volume * 0.9f - 1e-5f);
		EXPECT_LE(pick->volume, cue.volume * 1.1f + 1e-5f);
		EXPECT_GE(pick->pitch, cue.pitch * 0.95f - 1e-5f);
		EXPECT_LE(pick->pitch, cue.pitch * 1.05f + 1e-5f);
	}
	EXPECT_NEAR(static_cast<double>(heavy) / draws, 0.75, 0.05);
	EXPECT_EQ(cue.pick(42)->variant, cue.pick(42)->variant);

	auto silent = cue;
	for (auto& variant: silent.variants) variant.weight = 0.f;
	EXPECT_FALSE(silent.pick(7).has_value());
	EXPECT_FALSE(SoundCue{}.pick(7).has_value());
	silent.variants[1].weight = 1.f;
	EXPECT_EQ(silent.pick(7)->variant, 1u);
	core::Log::invalidate();
}

TEST(SoundCue, VariantKeysAndSpecificationsCarryTheTrim) {
	const auto cue = makeCue();
	EXPECT_TRUE(SoundCue::isCue("sounds/steps.owlsound"));
	EXPECT_FALSE(SoundCue::isCue("sounds/step_a.wav"));
	EXPECT_EQ(getVariantKey({.file = "a.wav",
							 .trimStart = 0.f,
							 .trimEnd = 0.f,
							 .loopStart = 0.f,
							 .loopEnd = 0.f,
							 .weight = 1.f}),
			  "a.wav");
	EXPECT_NE(getVariantKey(cue.variants[0]), getVariantKey(cue.variants[1]));
	EXPECT_NE(getVariantKey(cue.variants[0]), cue.variants[0].file);
	const auto spec = getVariantSpecification(cue.variants[1]);
	EXPECT_EQ(spec.file, cue.variants[1].file);
	EXPECT_FLOAT_EQ(spec.loopStart, 0.2f);
	EXPECT_FLOAT_EQ(spec.loopEnd, 0.4f);
}
