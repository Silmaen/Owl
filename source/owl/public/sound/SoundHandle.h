/**
 * @file SoundHandle.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

namespace owl::sound {

/// Opaque handle to an active sound source.
using SoundHandle = uint64_t;
/// Invalid handle sentinel.
constexpr SoundHandle invalidSoundHandle = 0;

/**
 * @brief Parameters for playing a sound.
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
};

}// namespace owl::sound
