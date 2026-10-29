/**
 * @file SkeletalClip.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/animation/Skeleton.h"
#include "math/quaternion.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>
#include <vector>

namespace owl::data::animation {

/**
 * @brief
 *  How a track goes from one key to the next.
 */
enum struct Interpolation : uint8_t {
	Linear,///< Linear (normalized-lerp for rotations).
	Step///< The value of the previous key until the next one.
};

/**
 * @brief
 *  Keys of one vector channel (translation or scale) of a bone.
 */
struct OWL_API VectorTrack {
	/// Time of each key, in seconds, increasing.
	std::vector<float> times;
	/// Value of each key.
	std::vector<math::vec3> values;
	/// Interpolation between the keys.
	Interpolation interpolation = Interpolation::Linear;

	/**
	 * @brief
	 *  Check if the track has no key.
	 * @return True without key.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return times.empty() || values.size() < times.size(); }

	/**
	 * @brief
	 *  Sample the track.
	 * @param[in] iTime The time, in seconds (clamped to the keys).
	 * @return The interpolated value.
	 */
	[[nodiscard]] auto sample(float iTime) const -> math::vec3;
};

/**
 * @brief
 *  Keys of the rotation channel of a bone.
 */
struct OWL_API RotationTrack {
	/// Time of each key, in seconds, increasing.
	std::vector<float> times;
	/// Value of each key (unit quaternions).
	std::vector<math::quat> values;
	/// Interpolation between the keys.
	Interpolation interpolation = Interpolation::Linear;

	/**
	 * @brief
	 *  Check if the track has no key.
	 * @return True without key.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return times.empty() || values.size() < times.size(); }

	/**
	 * @brief
	 *  Sample the track.
	 * @param[in] iTime The time, in seconds (clamped to the keys).
	 * @return The interpolated rotation.
	 */
	[[nodiscard]] auto sample(float iTime) const -> math::quat;
};

/**
 * @brief
 *  The animated channels of one bone in a clip.
 */
struct OWL_API BoneTrack {
	/// Index of the bone in the skeleton.
	uint32_t bone = 0;
	/// Translation keys, empty when the clip keeps the rest translation.
	VectorTrack translation;
	/// Rotation keys, empty when the clip keeps the rest rotation.
	RotationTrack rotation;
	/// Scale keys, empty when the clip keeps the rest scale.
	VectorTrack scale;
};

/**
 * @brief
 *  A skeletal animation: tracks of bone transforms over time.
 */
struct OWL_API SkeletalClip {
	/// Name of the clip (its glTF animation name).
	std::string name;
	/// Length of the clip, in seconds (the last key).
	float duration = 0.f;
	/// Animated bones.
	std::vector<BoneTrack> tracks;

	/**
	 * @brief
	 *  Get the time of the clip at a playback time.
	 * @param[in] iTime The playback time, in seconds.
	 * @param[in] iLoop True to wrap around the duration, false to hold the last key.
	 * @return The clip time, in `[0, duration]`.
	 */
	[[nodiscard]] auto getLocalTime(float iTime, bool iLoop) const -> float;

	/**
	 * @brief
	 *  Write the animated channels of the clip at a time into a pose; the other channels keep their value.
	 * @param[in] iTime The playback time, in seconds.
	 * @param[in] iLoop True to wrap around the duration.
	 * @param[in,out] ioPose The pose (a track of a bone outside it is skipped).
	 */
	void sample(float iTime, bool iLoop, Pose& ioPose) const;
};

}// namespace owl::data::animation
