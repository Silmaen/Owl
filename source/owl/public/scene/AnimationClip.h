/**
 * @file AnimationClip.h
 * @author Silmaen
 * @date 27/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Curve.h"
#include "renderer/Texture.h"

#include <filesystem>
#include <string>

namespace owl::scene {

/**
 * @brief Reusable spritesheet animation asset (`.owlanim`).
 *
 * An `AnimationClip` carries the data needed to play a frame-by-frame animation off a regular
 * spritesheet grid: the source texture, grid dimensions, frame range, per-frame duration,
 * looping mode and an optional speed-remap curve. It mirrors the playback parameters of
 * `scene::component::AnimatedSpriteRenderer` minus the per-instance tint color, which stays
 * on the consuming component (the clip is a shared asset; the tint is an instance property).
 *
 * The asset is YAML-serialized with the extension `.owlanim`. The on-disk schema is:
 * ```yaml
 * AnimationClip: <name>
 * Version: 1
 * texture: <texture serialized name>     # optional
 * columns: 4
 * rows: 2
 * firstFrame: 0
 * lastFrame: 7
 * frameDuration: 0.1
 * loop: true
 * speedCurve:                            # optional, omitted when empty
 *   interpolation: Linear
 *   keys:
 *     - [0.0, 1.0]
 *     - [1.0, 1.0]
 * ```
 *
 * Wire-compatible with the existing component serialization for the shared keys, so the
 * fields can later be re-used to introduce a `clipPath` reference on the component without
 * a YAML migration.
 */
class OWL_API AnimationClip final {
public:
	AnimationClip() = default;
	~AnimationClip() = default;
	AnimationClip(const AnimationClip&) = default;
	AnimationClip(AnimationClip&&) = default;
	auto operator=(const AnimationClip&) -> AnimationClip& = default;
	auto operator=(AnimationClip&&) -> AnimationClip& = default;

	/// @brief File extension used by `.owlanim` assets (with the leading dot).
	static auto fileExtension() -> const char* { return ".owlanim"; }

	/// Spritesheet texture (may be null while the asset is being authored).
	shared<renderer::Texture2D> texture;
	/// Number of columns in the spritesheet grid (>= 1).
	uint32_t columns = 1;
	/// Number of rows in the spritesheet grid (>= 1).
	uint32_t rows = 1;
	/// First frame index (0-based, row-major, inclusive).
	uint32_t firstFrame = 0;
	/// Last frame index (inclusive).
	uint32_t lastFrame = 0;
	/// Duration of each frame, in seconds.
	float frameDuration = 0.1f;
	/// Whether playback loops back to `firstFrame` after `lastFrame`.
	bool loop = true;
	/// Optional speed-remap curve sampled by playback progress; empty curve = constant speed.
	math::Curve speedCurve;

	/**
	 * @brief Serialize the clip to a YAML string.
	 * @param[in] iName Optional display name written under the `AnimationClip:` key.
	 * @return The YAML document as a string.
	 */
	[[nodiscard]] auto serializeToString(std::string_view iName = "") const -> std::string;

	/**
	 * @brief Populate the clip from a YAML string (clip is reset on success).
	 * @param[in] iYaml The YAML document.
	 * @return True on success, false on malformed input (clip left unchanged).
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iYaml) -> bool;

	/**
	 * @brief Save the clip to a file on disk.
	 * @param[in] iPath Destination file (any extension is allowed; `.owlanim` is conventional).
	 * @param[in] iName Optional display name.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath, std::string_view iName = "") const -> bool;

	/**
	 * @brief Load the clip from a file on disk.
	 * @param[in] iPath Source file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iPath) -> bool;
};

}// namespace owl::scene
