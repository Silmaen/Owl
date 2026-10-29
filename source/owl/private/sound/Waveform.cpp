/**
 * @file Waveform.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "sound/Waveform.h"

#include "core/Log.h"

#if OWL_WITH_AUDIO
#include <sndfile.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace owl::sound {

auto readWaveform(const std::filesystem::path& iPath, const size_t iBuckets) -> std::vector<float> {
	if (iBuckets == 0)
		return {};
#if OWL_WITH_AUDIO
	SF_INFO info{};
	SNDFILE* file = sf_open(reinterpret_cast<const char*>(iPath.u8string().c_str()), SFM_READ, &info);
	if (file == nullptr) {
		OWL_CORE_WARN("Waveform: Cannot read {} ({}).", iPath.string(), sf_strerror(nullptr))
		return {};
	}
	if (info.frames <= 0 || info.channels <= 0) {
		sf_close(file);
		OWL_CORE_WARN("Waveform: {} holds no sample.", iPath.string())
		return {};
	}
	std::vector<float> peaks(iBuckets, 0.f);
	const auto channels = static_cast<size_t>(info.channels);
	const auto frames = static_cast<size_t>(info.frames);
	constexpr size_t blockFrames = 4096;
	std::vector<float> block(blockFrames * channels);
	size_t frame = 0;
	while (frame < frames) {
		const sf_count_t read = sf_readf_float(file, block.data(), static_cast<sf_count_t>(blockFrames));
		if (read <= 0)
			break;
		for (size_t i = 0; i < static_cast<size_t>(read); ++i, ++frame) {
			float peak = 0.f;
			for (size_t channel = 0; channel < channels; ++channel)
				peak = std::max(peak, std::abs(block[i * channels + channel]));
			auto& bucket = peaks[std::min(iBuckets - 1, frame * iBuckets / frames)];
			bucket = std::max(bucket, std::min(peak, 1.f));
		}
	}
	sf_close(file);
	return peaks;
#else
	OWL_CORE_WARN("Waveform: The audio module is not built, {} is not read.", iPath.string())
	return {};
#endif
}

auto readSoundDuration(const std::filesystem::path& iPath) -> std::optional<float> {
#if OWL_WITH_AUDIO
	SF_INFO info{};
	SNDFILE* file = sf_open(reinterpret_cast<const char*>(iPath.u8string().c_str()), SFM_READ, &info);
	if (file == nullptr) {
		OWL_CORE_WARN("Waveform: Cannot read {} ({}).", iPath.string(), sf_strerror(nullptr))
		return std::nullopt;
	}
	sf_close(file);
	if (info.samplerate <= 0) {
		OWL_CORE_WARN("Waveform: {} has no sample rate.", iPath.string())
		return std::nullopt;
	}
	return static_cast<float>(static_cast<double>(info.frames) / static_cast<double>(info.samplerate));
#else
	OWL_CORE_WARN("Waveform: The audio module is not built, {} is not read.", iPath.string())
	return std::nullopt;
#endif
}

}// namespace owl::sound
