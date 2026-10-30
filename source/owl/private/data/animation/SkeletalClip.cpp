/**
 * @file SkeletalClip.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/animation/SkeletalClip.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <vector>

namespace owl::data::animation {

namespace {

struct KeySpan {
	size_t first = 0;
	size_t second = 0;
	float weight = 0.f;
};

auto findKeys(const std::vector<float>& iTimes, const float iTime, const Interpolation iInterpolation) -> KeySpan {
	if (iTimes.size() == 1 || iTime <= iTimes.front())
		return {};
	if (iTime >= iTimes.back())
		return {.first = iTimes.size() - 1, .second = iTimes.size() - 1, .weight = 0.f};
	const auto next = std::ranges::upper_bound(iTimes, iTime);
	const auto second = static_cast<size_t>(std::distance(iTimes.begin(), next));
	const size_t first = second - 1;
	if (iInterpolation == Interpolation::Step)
		return {.first = first, .second = first, .weight = 0.f};
	const float span = iTimes[second] - iTimes[first];
	return {.first = first, .second = second, .weight = span > 0.f ? (iTime - iTimes[first]) / span : 0.f};
}

}// namespace

auto VectorTrack::sample(const float iTime) const -> math::vec3 {
	if (isEmpty())
		return {0.f, 0.f, 0.f};
	const auto [first, second, weight] = findKeys(times, iTime, interpolation);
	return values[first] * (1.f - weight) + values[second] * weight;
}

auto RotationTrack::sample(const float iTime) const -> math::quat {
	if (isEmpty())
		return {1.f, 0.f, 0.f, 0.f};
	const auto [first, second, weight] = findKeys(times, iTime, interpolation);
	return BoneTransform::blend({.rotation = values[first]}, {.rotation = values[second]}, weight).rotation;
}

auto SkeletalClip::getLocalTime(const float iTime, const bool iLoop) const -> float {
	if (duration <= 0.f)
		return 0.f;
	if (!iLoop)
		return std::clamp(iTime, 0.f, duration);
	const float wrapped = std::fmod(iTime, duration);
	return wrapped < 0.f ? wrapped + duration : wrapped;
}

void SkeletalClip::sample(const float iTime, const bool iLoop, Pose& ioPose) const {
	const float time = getLocalTime(iTime, iLoop);
	for (const auto& track: tracks) {
		if (track.bone >= ioPose.size())
			continue;
		auto& bone = ioPose[track.bone];
		if (!track.translation.isEmpty())
			bone.translation = track.translation.sample(time);
		if (!track.rotation.isEmpty())
			bone.rotation = track.rotation.sample(time);
		if (!track.scale.isEmpty())
			bone.scale = track.scale.sample(time);
	}
}

}// namespace owl::data::animation
