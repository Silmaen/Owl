/**
 * @file Waveform_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <sound/Waveform.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace owl;

namespace {

void writeLe(std::ofstream& ioFile, const uint32_t iValue, const int iBytes) {
	for (int i = 0; i < iBytes; ++i) ioFile.put(static_cast<char>((iValue >> (8 * i)) & 0xffU));
}

// Mono 16-bit PCM: silence, then a square wave of half amplitude.
void writeTestWav(const std::filesystem::path& iPath, const uint32_t iFrames) {
	std::ofstream file(iPath, std::ios::binary);
	constexpr uint32_t rate = 8000;
	file.write("RIFF", 4);
	writeLe(file, 36 + iFrames * 2, 4);
	file.write("WAVEfmt ", 8);
	writeLe(file, 16, 4);
	writeLe(file, 1, 2);
	writeLe(file, 1, 2);
	writeLe(file, rate, 4);
	writeLe(file, rate * 2, 4);
	writeLe(file, 2, 2);
	writeLe(file, 16, 2);
	file.write("data", 4);
	writeLe(file, iFrames * 2, 4);
	for (uint32_t i = 0; i < iFrames; ++i) {
		int16_t sample = 0;
		if (i >= iFrames / 2)
			sample = (i % 2 == 0) ? 16384 : -16384;
		writeLe(file, static_cast<uint16_t>(sample), 2);
	}
}

}// namespace

TEST(Waveform, PeaksFollowTheLoudness) {
	OWL_REQUIRE_MODULE(AUDIO);
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_waveform_test";
	std::filesystem::create_directories(dir);
	const auto path = dir / "half.wav";
	writeTestWav(path, 8000);
	const auto peaks = sound::readWaveform(path, 8);
	ASSERT_EQ(peaks.size(), 8u);
	EXPECT_NEAR(peaks[0], 0.f, 1e-3f);
	EXPECT_NEAR(peaks[3], 0.f, 1e-3f);
	EXPECT_NEAR(peaks[4], 0.5f, 1e-2f);
	EXPECT_NEAR(peaks[7], 0.5f, 1e-2f);
	EXPECT_TRUE(sound::readWaveform(path, 0).empty());
	EXPECT_TRUE(sound::readWaveform(dir / "missing.wav", 8).empty());
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
