/**
 * @file Skeleton.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/animation/Skeleton.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::data::animation {

auto BoneTransform::toMatrix() const -> math::mat4 {
	math::mat4 result = math::toMat4(rotation);
	for (size_t row = 0; row < 3; ++row) {
		result(row, 0) *= scale.x();
		result(row, 1) *= scale.y();
		result(row, 2) *= scale.z();
	}
	result(0, 3) = translation.x();
	result(1, 3) = translation.y();
	result(2, 3) = translation.z();
	return result;
}

auto BoneTransform::blend(const BoneTransform& iFrom, const BoneTransform& iTo, const float iWeight) -> BoneTransform {
	const float weight = std::clamp(iWeight, 0.f, 1.f);
	const float keep = 1.f - weight;
	BoneTransform result;
	result.translation = iFrom.translation * keep + iTo.translation * weight;
	result.scale = iFrom.scale * keep + iTo.scale * weight;
	const float sign = iFrom.rotation.dot(iTo.rotation) < 0.f ? -1.f : 1.f;
	math::quat rotation = iFrom.rotation * keep + iTo.rotation * (weight * sign);
	if (rotation.normSq() > 1e-12f)
		rotation.normalize();
	else
		rotation = iTo.rotation;
	result.rotation = rotation;
	return result;
}

void blendPoses(const Pose& iFrom, const Pose& iTo, const float iWeight, Pose& oResult) {
	const size_t count = std::min(iFrom.size(), iTo.size());
	oResult.resize(count);
	for (size_t index = 0; index < count; ++index)
		oResult[index] = BoneTransform::blend(iFrom[index], iTo[index], iWeight);
}

Skeleton::Skeleton(std::vector<Bone> iBones) : m_bones{std::move(iBones)} {}

auto Skeleton::isValid() const -> bool {
	if (m_bones.size() > g_maxBones)
		return false;
	for (size_t index = 0; index < m_bones.size(); ++index) {
		if (m_bones[index].parent >= static_cast<int32_t>(index))
			return false;
	}
	return true;
}

auto Skeleton::findBone(const std::string_view iName) const -> std::optional<size_t> {
	const auto found = std::ranges::find(m_bones, iName, &Bone::name);
	if (found == m_bones.end())
		return std::nullopt;
	return static_cast<size_t>(std::distance(m_bones.begin(), found));
}

auto Skeleton::getRestPose() const -> Pose {
	Pose pose;
	pose.reserve(m_bones.size());
	for (const auto& bone: m_bones) pose.push_back(bone.rest);
	return pose;
}

void Skeleton::computeModelMatrices(const Pose& iPose, const std::span<math::mat4> oMatrices) const {
	const size_t count = std::min(m_bones.size(), oMatrices.size());
	for (size_t index = 0; index < count; ++index) {
		const auto& bone = m_bones[index];
		const math::mat4 local = index < iPose.size() ? iPose[index].toMatrix() : bone.rest.toMatrix();
		const bool hasParent = bone.parent >= 0 && static_cast<size_t>(bone.parent) < index;
		oMatrices[index] = hasParent ? oMatrices[static_cast<size_t>(bone.parent)] * local : bone.anchor * local;
	}
}

void Skeleton::computeSkinMatrices(const std::span<const math::mat4> iModelMatrices,
								   const std::span<math::mat4> oMatrices) const {
	const size_t count = std::min({m_bones.size(), iModelMatrices.size(), oMatrices.size()});
	for (size_t index = 0; index < count; ++index)
		oMatrices[index] = iModelMatrices[index] * m_bones[index].inverseBind;
}

}// namespace owl::data::animation
