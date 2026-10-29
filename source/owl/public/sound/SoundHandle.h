/**
 * @file SoundHandle.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "AudioEffects.h"
#include "Bus.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <optional>

namespace owl::sound {
/// Opaque handle to an active sound source.
using SoundHandle = uint64_t;
/// Invalid handle sentinel.
constexpr SoundHandle invalidSoundHandle = 0;

/**
 * @brief
 *  Parameters for playing a sound.
 */
struct OWL_API PlayParams {
	/// Gain [0..∞), default 1.0.
	float volume = 1.0f;
	/// Pitch multiplier (> 0), default 1.0.
	float pitch = 1.0f;
	/// Loop playback.
	bool loop = false;
	/// Enable 3D spatial positioning.
	bool spatial = false;
	/// Initial 3D position (if spatial).
	math::vec3f position{0, 0, 0};
	/// Initial 3D velocity (if spatial).
	math::vec3f velocity{0, 0, 0};
	/// Max audible distance for attenuation (if spatial).
	float maxDistance = 50.0f;
	/// Rolloff factor for distance attenuation (if spatial).
	float rolloff = 1.0f;
	/// Mixer bus of the sound; none: the default bus of its cue, SFX for a bare audio file.
	std::optional<Bus> bus = std::nullopt;
	/// Direct filter of the sound (low-pass, high-pass), combined with the filter of the reverb zones.
	Filter filter{};
};

}// namespace owl::sound
