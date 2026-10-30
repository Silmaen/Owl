/**
 * @file SoundCue.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "core/Macros.h"
#include "sound/Bus.h"
#include "sound/SoundData.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace owl::sound {

/**
 * @brief
 *  One take of a sound cue: a source file, the part of it that plays and the part that loops.
 *
 * Times are in seconds from the start of the file. An end of 0 means the end of the file; a loop end of 0 loops the
 * whole played part.
 */
struct OWL_API SoundVariant {
	/// Source audio file, relative to the asset folders.
	std::string file;
	/// First played second.
	float trimStart{0.f};
	/// Last played second, 0 for the end of the file.
	float trimEnd{0.f};
	/// Start of the looped part, in seconds from the start of the file.
	float loopStart{0.f};
	/// End of the looped part, 0 for the end of the played part.
	float loopEnd{0.f};
	/// Relative chance of this take being picked.
	float weight{1.f};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other variant.
	 * @return True when both are the same.
	 */
	auto operator==(const SoundVariant& iOther) const -> bool = default;
	OWL_DIAG_POP
};

/**
 * @brief
 *  What one play of a cue picked: the take and its randomised volume and pitch.
 */
struct OWL_API SoundCuePick {
	/// Index of the take in `SoundCue::variants`.
	size_t variant{0};
	/// Volume multiplier.
	float volume{1.f};
	/// Pitch multiplier.
	float pitch{1.f};
};

/**
 * @brief
 *  A sound authored in Owl Nest (`.owlsound`): random takes of one sound, trimmed and looped, with a volume and a
 *  pitch varied at each play.
 */
struct OWL_API SoundCue {
	/// Takes, one picked at random at each play.
	std::vector<SoundVariant> variants;
	/// Base volume multiplier.
	float volume{1.f};
	/// Random volume change at each play, as a fraction of the volume (0.1 = ±10 %).
	float volumeJitter{0.f};
	/// Base pitch multiplier.
	float pitch{1.f};
	/// Random pitch change at each play, as a fraction of the pitch.
	float pitchJitter{0.f};
	/// Whether the sound loops.
	bool loop{false};
	/// Bus the cue plays on when the caller names none (`sound.play`); a Sound Source picks its own.
	Bus bus{Bus::SFX};

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other cue.
	 * @return True when both are the same.
	 */
	auto operator==(const SoundCue& iOther) const -> bool = default;
	OWL_DIAG_POP

	/**
	 * @brief
	 *  File extension of sound cues (with the leading dot).
	 * @return The extension literal (`".owlsound"`).
	 */
	static auto fileExtension() noexcept -> const char* { return ".owlsound"; }

	/**
	 * @brief
	 *  Whether a sound reference names a cue rather than an audio file.
	 * @param[in] iPath The reference.
	 * @return True for a `.owlsound` path.
	 */
	[[nodiscard]] static auto isCue(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Format descriptor of the `.owlsound` files, with its migration chain.
	 * @return The SoundCue format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Serialize the cue to a YAML string.
	 * @return The YAML document.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read a cue from a YAML string.
	 * @param[in] iYaml The YAML document.
	 * @return The cue, or nothing when the document is not a sound cue.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml) -> std::optional<SoundCue>;

	/**
	 * @brief
	 *  Save the cue to a file. Atomic: a failed write keeps the previous file.
	 * @param[in] iPath Destination file.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a cue from the pack or the asset folders.
	 * @param[in] iPath The cue, relative to the asset folders, or absolute.
	 * @return The cue, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iPath) -> std::optional<SoundCue>;

	/**
	 * @brief
	 *  Pick a take, a volume and a pitch from one random value.
	 * @param[in] iRandom A uniformly distributed 64-bit value.
	 * @return The pick, or nothing when the cue has no take with a positive weight.
	 */
	[[nodiscard]] auto pick(uint64_t iRandom) const -> std::optional<SoundCuePick>;
};

/**
 * @brief
 *  Specification of the sound data a take plays.
 * @param[in] iVariant The take.
 * @return The file with its trim and loop points.
 */
[[nodiscard]] OWL_API auto getVariantSpecification(const SoundVariant& iVariant) -> SoundData::Specification;

/**
 * @brief
 *  Name of the sound data of a take in the sound library, distinct for each trim and loop.
 * @param[in] iVariant The take.
 * @return The library key.
 */
[[nodiscard]] OWL_API auto getVariantKey(const SoundVariant& iVariant) -> std::string;

}// namespace owl::sound
