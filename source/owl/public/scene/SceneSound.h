/**
 * @file SceneSound.h
 * @author Silmaen
 * @date 08/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "sound/SoundHandle.h"

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
	 */
	auto operator=(const SceneSound&) -> SceneSound& = default;

	/**
	 * @brief
	 *  Default move affectation operator.
	 */
	auto operator=(SceneSound&&) -> SceneSound& = default;

	/// Sound category.
	enum struct Category : uint8_t {
		SFX,///< Sound effect (one-shot or short).
		Music,///< Background music (typically looping, non-spatial).
		Ambient///< Ambient sound (environmental, may be spatial).
	};

	/// Asset path relative to asset directory.
	std::string soundAsset;
	/// Sound category.
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

	/// Runtime handle (not serialized).
	sound::SoundHandle runtimeHandle = sound::invalidSoundHandle;
};

}// namespace owl::scene
