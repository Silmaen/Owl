/**
 * @file GltfSkin.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/animation/SkeletalClip.h"
#include "data/animation/Skeleton.h"
#include "data/geometry/MeshLoader.h"

#include <tiny_gltf.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::data::geometry::gltf {

/**
 * @brief
 *  The skeleton of the first skin of a glTF file and how its joints map to the bones.
 */
struct SkinImport {
	/// The skeleton, null when the file has no skin.
	shared<const animation::Skeleton> skeleton;
	/// Bone index of each joint of the skin, in the order of the skin.
	std::vector<uint16_t> jointToBone;
	/// Bone index of each node, -1 for a node that is no bone.
	std::vector<int32_t> nodeToBone;
};

/**
 * @brief
 *  Read the first skin of a model.
 * @param[in] iModel The model.
 * @param[in] iName Name of the file, for the logs.
 * @return The skeleton and the joint mapping; an empty import without skin.
 */
auto importSkin(const tinygltf::Model& iModel, const std::string& iName) -> SkinImport;

/**
 * @brief
 *  Read the joints and weights of a skinned primitive.
 * @param[in] iModel The model.
 * @param[in] iPrimitive The primitive.
 * @param[in] iSkin The skin import.
 * @param[in] iVertexCount Number of vertices of the primitive.
 * @return One entry per vertex (zero weights where the file gives none).
 */
auto readPrimitiveSkin(const tinygltf::Model& iModel, const tinygltf::Primitive& iPrimitive, const SkinImport& iSkin,
					   size_t iVertexCount) -> std::vector<VertexSkin>;

/**
 * @brief
 *  Read the animations moving bones of the skin.
 * @param[in] iModel The model.
 * @param[in] iSkin The skin import.
 * @param[in] iName Name of the file, for the logs.
 * @return The clips.
 */
auto importClips(const tinygltf::Model& iModel, const SkinImport& iSkin, const std::string& iName)
		-> std::vector<animation::SkeletalClip>;

/**
 * @brief
 *  Read the nodes named `socket_<name>`.
 * @param[in] iModel The model.
 * @param[in] iSkin The skin import (sockets below a bone follow it).
 * @return The sockets.
 */
auto importSockets(const tinygltf::Model& iModel, const SkinImport& iSkin) -> std::vector<MeshSocket>;

}// namespace owl::data::geometry::gltf
