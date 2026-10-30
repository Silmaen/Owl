/**
 * @file Skeleton.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/matrices.h"
#include "math/matrixCreation.h"
#include "math/quaternion.h"
#include "math/vectors.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::animation {

/**
 * @brief
 *  Local transform of a bone: translation, rotation and scale, relative to its parent.
 */
struct OWL_API BoneTransform {
	/// Translation relative to the parent bone.
	math::vec3 translation{0.f, 0.f, 0.f};
	/// Rotation relative to the parent bone (unit quaternion).
	math::quat rotation{1.f, 0.f, 0.f, 0.f};
	/// Scale along the bone axes.
	math::vec3 scale{1.f, 1.f, 1.f};

	/**
	 * @brief
	 *  Get the matrix of the transform: translation * rotation * scale.
	 * @return The 4x4 matrix.
	 */
	[[nodiscard]] auto toMatrix() const -> math::mat4;

	/**
	 * @brief
	 *  Blend two transforms: linear translation and scale, normalized-lerp of the rotation on the shortest arc.
	 * @param[in] iFrom The transform at weight 0.
	 * @param[in] iTo The transform at weight 1.
	 * @param[in] iWeight The weight of `iTo`, in `[0, 1]`.
	 * @return The blended transform.
	 */
	[[nodiscard]] static auto blend(const BoneTransform& iFrom, const BoneTransform& iTo, float iWeight)
			-> BoneTransform;
};

/// The local transform of every bone of a skeleton, in the bone order.
using Pose = std::vector<BoneTransform>;

/**
 * @brief
 *  Blend two poses of one skeleton bone by bone.
 * @param[in] iFrom The pose at weight 0.
 * @param[in] iTo The pose at weight 1.
 * @param[in] iWeight The weight of `iTo`, in `[0, 1]`.
 * @param[out] oResult The blended pose (resized to the shorter of the two).
 */
OWL_API void blendPoses(const Pose& iFrom, const Pose& iTo, float iWeight, Pose& oResult);

/**
 * @brief
 *  One joint of a skeleton.
 */
struct OWL_API Bone {
	/// Name of the bone (its glTF node name).
	std::string name;
	/// Index of the parent bone, -1 for a root; always lower than the index of the bone.
	int32_t parent = -1;
	/// Matrix from the bind space of the mesh to the space of the bone.
	math::mat4 inverseBind = math::identity<float, 4>();
	/// Local transform of the bone at rest.
	BoneTransform rest;
	/// Transform of the nodes above a root bone that are not bones (identity for the other bones).
	math::mat4 anchor = math::identity<float, 4>();
};

/**
 * @brief
 *  A bone hierarchy and its bind pose, the parents always before their children.
 */
class OWL_API Skeleton final {
public:
	/// Most bones a skinned draw reads.
	static constexpr size_t g_maxBones = 256;

	/**
	 * @brief
	 *  Default constructor.
	 */
	Skeleton() = default;

	/**
	 * @brief
	 *  Build a skeleton from its bones.
	 * @param[in] iBones The bones, each parent before its children.
	 */
	explicit Skeleton(std::vector<Bone> iBones);

	/**
	 * @brief
	 *  Default destructor.
	 */
	~Skeleton() = default;

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	Skeleton(const Skeleton&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	Skeleton(Skeleton&&) noexcept = default;

	/**
	 * @brief
	 *  Default copy assignment.
	 * @return This skeleton.
	 */
	auto operator=(const Skeleton&) -> Skeleton& = default;

	/**
	 * @brief
	 *  Default move assignment.
	 * @return This skeleton.
	 */
	auto operator=(Skeleton&&) noexcept -> Skeleton& = default;

	/**
	 * @brief
	 *  Get the bones.
	 * @return The bones, parents first.
	 */
	[[nodiscard]] auto getBones() const -> const std::vector<Bone>& { return m_bones; }

	/**
	 * @brief
	 *  Get the number of bones.
	 * @return The bone count.
	 */
	[[nodiscard]] auto getBoneCount() const -> size_t { return m_bones.size(); }

	/**
	 * @brief
	 *  Check if the skeleton has no bone.
	 * @return True without bone.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return m_bones.empty(); }

	/**
	 * @brief
	 *  Check that every parent comes before its children and that the bone count fits a skinned draw.
	 * @return True when the skeleton can be drawn.
	 */
	[[nodiscard]] auto isValid() const -> bool;

	/**
	 * @brief
	 *  Find a bone by its name.
	 * @param[in] iName The name.
	 * @return The index of the bone, nothing when no bone has that name.
	 */
	[[nodiscard]] auto findBone(std::string_view iName) const -> std::optional<size_t>;

	/**
	 * @brief
	 *  Get the rest pose.
	 * @return The local transform of every bone at rest.
	 */
	[[nodiscard]] auto getRestPose() const -> Pose;

	/**
	 * @brief
	 *  Compute the model-space matrix of every bone of a pose.
	 * @param[in] iPose The pose (missing bones take their rest transform).
	 * @param[out] oMatrices One matrix per bone (its size must be the bone count).
	 */
	void computeModelMatrices(const Pose& iPose, std::span<math::mat4> oMatrices) const;

	/**
	 * @brief
	 *  Compute the skin matrices of a pose: model-space bone matrix times inverse bind.
	 * @param[in] iModelMatrices The model-space bone matrices (`computeModelMatrices`).
	 * @param[out] oMatrices One matrix per bone (its size must be the bone count).
	 */
	void computeSkinMatrices(std::span<const math::mat4> iModelMatrices, std::span<math::mat4> oMatrices) const;

private:
	/// The bones, parents first.
	std::vector<Bone> m_bones;
};

}// namespace owl::data::animation
