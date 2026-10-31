/**
 * @file Sequence.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief
 *  Cutscene sequences (`.owlseq`): camera tracks and event tracks on one timeline, their file and their sampling.
 */
namespace owl::data::sequence {

/**
 * @brief
 *  How a key moves to the next one.
 */
enum struct Easing : uint8_t {
	Smooth,///< Cubic curve through the neighbour keys, eased at the first and last keys of the track.
	Linear,///< Straight line at constant speed.
	Step,///< Holds the value until the next key: a cut.
};

/**
 * @brief
 *  A pose of the camera at a time of the sequence.
 */
struct OWL_API CameraKey {
	/// Time of the key, in seconds from the start of the sequence.
	float time = 0.f;
	/// Position, in the space of the entity playing the sequence.
	math::vec3 position{0.f, 0.f, 0.f};
	/// Euler angles in radians, in the space of the entity playing the sequence.
	math::vec3 rotation{0.f, 0.f, 0.f};
	/// Vertical field of view of a perspective camera, in radians.
	float fov = 0.785398f;
	/// Size of an orthographic camera.
	float size = 10.f;
	/// How the camera moves from this key to the next one.
	Easing easing = Easing::Smooth;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other key.
	 * @return True when both are the same.
	 */
	auto operator==(const CameraKey& iOther) const -> bool;
};

/**
 * @brief
 *  A shot: the camera keys of one track, live from its first key to its last.
 */
struct OWL_API CameraTrack {
	/// Name shown by the editor.
	std::string name;
	/// Keys, sorted by time.
	std::vector<CameraKey> keys;
	/// A muted track does not drive the camera.
	bool muted = false;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other track.
	 * @return True when both are the same.
	 */
	auto operator==(const CameraTrack& iOther) const -> bool;

	/**
	 * @brief
	 *  Time of the first key.
	 * @return The time, 0 for a track without key.
	 */
	[[nodiscard]] auto getStart() const -> float;

	/**
	 * @brief
	 *  Time of the last key.
	 * @return The time, 0 for a track without key.
	 */
	[[nodiscard]] auto getEnd() const -> float;

	/**
	 * @brief
	 *  Pose of the track at a time, clamped to its first and last keys.
	 * @param[in] iTime Time in seconds.
	 * @return The pose, nothing for a track without key.
	 */
	[[nodiscard]] auto sample(float iTime) const -> std::optional<CameraKey>;
};

/**
 * @brief
 *  A Lua function called when the playback reaches a time.
 */
struct OWL_API EventKey {
	/// Time of the event, in seconds from the start of the sequence.
	float time = 0.f;
	/// Name of the Lua function called, in the script of the playing entity, else the scene script.
	std::string function;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other key.
	 * @return True when both are the same.
	 */
	auto operator==(const EventKey& iOther) const -> bool;
};

/**
 * @brief
 *  A row of events of a sequence.
 */
struct OWL_API EventTrack {
	/// Name shown by the editor.
	std::string name;
	/// Events, sorted by time.
	std::vector<EventKey> keys;
	/// A muted track calls nothing.
	bool muted = false;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other track.
	 * @return True when both are the same.
	 */
	auto operator==(const EventTrack& iOther) const -> bool;
};

/**
 * @brief
 *  The camera pose a sequence gives at a time, and the shot it comes from.
 */
struct OWL_API CameraShot {
	/// The pose.
	CameraKey pose;
	/// Index of the camera track giving it.
	size_t track = 0;
};

/**
 * @brief
 *  A cutscene: camera tracks (shots) and event tracks on one timeline.
 *
 * At a time, the last unmuted camera track whose keys span that time drives the camera; between two shots the last one
 * ended holds its last key, and before the first shot the earliest one holds its first key.
 */
struct OWL_API Sequence {
	/// Length of the sequence, in seconds.
	float duration = 5.f;
	/// Camera tracks, the later ones over the earlier ones.
	std::vector<CameraTrack> cameraTracks;
	/// Event tracks.
	std::vector<EventTrack> eventTracks;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other sequence.
	 * @return True when both are the same.
	 */
	auto operator==(const Sequence& iOther) const -> bool;

	/**
	 * @brief
	 *  Extension of a sequence file.
	 * @return `.owlseq`.
	 */
	static auto fileExtension() noexcept -> const char* { return ".owlseq"; }

	/**
	 * @brief
	 *  Whether a path names a sequence file.
	 * @param[in] iPath The path.
	 * @return True for a `.owlseq` file.
	 */
	[[nodiscard]] static auto isSequence(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Versioned format of the file.
	 * @return The format descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  A new sequence: one camera track of two keys and an empty event track.
	 * @return The sequence.
	 */
	[[nodiscard]] static auto makeDefault() -> Sequence;

	/**
	 * @brief
	 *  Write the sequence as YAML.
	 * @return The YAML text.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read a sequence from YAML; keys come out sorted by time.
	 * @param[in] iYaml The YAML text.
	 * @return The sequence, or nothing when the text is not a readable sequence.
	 */
	[[nodiscard]] static auto deserializeFromString(std::string_view iYaml) -> std::optional<Sequence>;

	/**
	 * @brief
	 *  Write the sequence to a file, atomically.
	 * @param[in] iPath Path of the file.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read a sequence from a file on disk.
	 * @param[in] iPath Path of the file.
	 * @return The sequence, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto loadFile(const std::filesystem::path& iPath) -> std::optional<Sequence>;

	/**
	 * @brief
	 *  Read a sequence asset from the open pack or the asset folders.
	 * @param[in] iAsset Asset path or `gid:` reference.
	 * @return The sequence, or nothing when it cannot be read.
	 */
	[[nodiscard]] static auto load(const std::filesystem::path& iAsset) -> std::optional<Sequence>;

	/**
	 * @brief
	 *  Sort the keys of every track by time (stable: keys at one time keep their order).
	 */
	void sortKeys();

	/**
	 * @brief
	 *  The camera pose at a time.
	 * @param[in] iTime Time in seconds.
	 * @return The pose and its track, nothing when no unmuted camera track has a key.
	 */
	[[nodiscard]] auto sampleCamera(float iTime) const -> std::optional<CameraShot>;

	/**
	 * @brief
	 *  The events of the unmuted tracks in a span of time, in time order.
	 * @param[in] iFrom Start of the span.
	 * @param[in] iTo End of the span, included.
	 * @param[in] iIncludeFrom Whether an event at `iFrom` is met (the first frame of a playback).
	 * @return The events met.
	 */
	[[nodiscard]] auto getEvents(float iFrom, float iTo, bool iIncludeFrom) const -> std::vector<const EventKey*>;

	/**
	 * @brief
	 *  Names of the Lua functions the events call, sorted and without duplicates.
	 * @return The names.
	 */
	[[nodiscard]] auto getFunctions() const -> std::vector<std::string>;
};

/**
 * @brief
 *  Name of an easing.
 * @param[in] iEasing The easing.
 * @return Its name.
 */
OWL_API auto getEasingName(Easing iEasing) -> std::string_view;

/**
 * @brief
 *  Easing from its name.
 * @param[in] iName The name.
 * @return The easing, nothing for an unknown name.
 */
OWL_API auto parseEasing(std::string_view iName) -> std::optional<Easing>;

/**
 * @brief
 *  Every easing, in declaration order.
 * @return The easings.
 */
OWL_API auto getEasings() -> std::span<const Easing>;

}// namespace owl::data::sequence
