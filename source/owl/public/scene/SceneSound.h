/**
 * @file SceneSound.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "sound/AudioEffects.h"
#include "sound/Bus.h"
#include "sound/SoundHandle.h"

#include <cstdint>
#include <string>

namespace owl::scene {
/**
 * @brief
 *  Class describing a sound source in the scene.
 */
class OWL_API SceneSound final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	SceneSound();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~SceneSound() = default;

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	SceneSound(const SceneSound&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SceneSound(SceneSound&&) = default;

	/**
	 * @brief
	 *  Default copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const SceneSound&) -> SceneSound& = default;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(SceneSound&&) -> SceneSound& = default;

	/// Bus of the sound in the mixer (Master, Music, SFX, Ambient, Voice).
	using Category = sound::Bus;

	/// Asset path relative to asset directory.
	std::string soundAsset;
	/// Subtitle shown while the sound plays (a localisation key or the text itself), empty for none.
	std::string subtitle;
	/// Mixer bus the sound plays on.
	Category category = Category::SFX;
	/// Volume gain [0..∞).
	float volume = 1.0f;
	/// Pitch multiplier (> 0).
	float pitch = 1.0f;
	/// Loop playback.
	bool loop = false;
	/// Enable 3D spatial positioning.
	bool spatial = false;
	/// Play automatically when runtime starts.
	bool playOnStart = false;
	/// Max audible distance for attenuation (if spatial).
	float maxDistance = 50.0f;
	/// Rolloff factor for distance attenuation (if spatial).
	float rolloff = 1.0f;

	/// Direct filter of the sound: low-pass behind a wall, high-pass for a radio.
	sound::Filter filter{};
	/// Whether the motion of the entity gives the sound a Doppler effect (spatial sounds only).
	bool doppler = true;

	/// World position at the previous frame, for the Doppler velocity (not serialized).
	math::vec3d runtimeLastPosition{0.0, 0.0, 0.0};
	/// Whether runtimeLastPosition holds a position (not serialized).
	bool runtimeHasLastPosition = false;
	/// Runtime handle (not serialized).
	sound::SoundHandle runtimeHandle = sound::invalidSoundHandle;
};

}// namespace owl::scene
