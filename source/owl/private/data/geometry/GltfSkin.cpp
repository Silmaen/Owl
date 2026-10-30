/**
 * @file GltfSkin.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GltfSkin.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::data::geometry::gltf {

namespace {

using size_type = std::vector<tinygltf::Accessor>::size_type;
constexpr std::string_view g_socketPrefix{"socket_"};

auto toMatrix(const std::vector<double>& iColumnMajor) -> math::mat4 {
	math::mat4 matrix;
	for (size_t col = 0; col < 4; ++col)
		for (size_t row = 0; row < 4; ++row) matrix(row, col) = static_cast<float>(iColumnMajor[col * 4 + row]);
	return matrix;
}

// Rotation of an orthonormal 3x3 block (Shepperd's method).
auto toQuaternion(const math::mat4& iM) -> math::quat {
	const float trace = iM(0, 0) + iM(1, 1) + iM(2, 2);
	math::quat q;
	if (trace > 0.f) {
		const float s = std::sqrt(trace + 1.f) * 2.f;
		q = {0.25f * s, (iM(2, 1) - iM(1, 2)) / s, (iM(0, 2) - iM(2, 0)) / s, (iM(1, 0) - iM(0, 1)) / s};
	} else if (iM(0, 0) > iM(1, 1) && iM(0, 0) > iM(2, 2)) {
		const float s = std::sqrt(1.f + iM(0, 0) - iM(1, 1) - iM(2, 2)) * 2.f;
		q = {(iM(2, 1) - iM(1, 2)) / s, 0.25f * s, (iM(0, 1) + iM(1, 0)) / s, (iM(0, 2) + iM(2, 0)) / s};
	} else if (iM(1, 1) > iM(2, 2)) {
		const float s = std::sqrt(1.f + iM(1, 1) - iM(0, 0) - iM(2, 2)) * 2.f;
		q = {(iM(0, 2) - iM(2, 0)) / s, (iM(0, 1) + iM(1, 0)) / s, 0.25f * s, (iM(1, 2) + iM(2, 1)) / s};
	} else {
		const float s = std::sqrt(1.f + iM(2, 2) - iM(0, 0) - iM(1, 1)) * 2.f;
		q = {(iM(1, 0) - iM(0, 1)) / s, (iM(0, 2) + iM(2, 0)) / s, (iM(1, 2) + iM(2, 1)) / s, 0.25f * s};
	}
	return q.normalized();
}

auto localTransform(const tinygltf::Node& iNode) -> animation::BoneTransform {
	animation::BoneTransform result;
	if (iNode.matrix.size() == 16) {
		math::mat4 matrix = toMatrix(iNode.matrix);
		result.translation = {matrix(0, 3), matrix(1, 3), matrix(2, 3)};
		for (size_t col = 0; col < 3; ++col) {
			const math::vec3 axis{matrix(0, col), matrix(1, col), matrix(2, col)};
			const float length = axis.norm();
			result.scale[col] = length;
			if (length > 0.f) {
				for (size_t row = 0; row < 3; ++row) matrix(row, col) /= length;
			}
		}
		result.rotation = toQuaternion(matrix);
		return result;
	}
	if (iNode.translation.size() == 3)
		result.translation = {static_cast<float>(iNode.translation[0]), static_cast<float>(iNode.translation[1]),
							  static_cast<float>(iNode.translation[2])};
	if (iNode.rotation.size() == 4)
		result.rotation = math::quat{static_cast<float>(iNode.rotation[3]), static_cast<float>(iNode.rotation[0]),
									 static_cast<float>(iNode.rotation[1]), static_cast<float>(iNode.rotation[2])};
	if (iNode.scale.size() == 3)
		result.scale = {static_cast<float>(iNode.scale[0]), static_cast<float>(iNode.scale[1]),
						static_cast<float>(iNode.scale[2])};
	return result;
}

auto parentsOf(const tinygltf::Model& iModel) -> std::vector<int32_t> {
	std::vector<int32_t> parents(iModel.nodes.size(), -1);
	for (size_t node = 0; node < iModel.nodes.size(); ++node) {
		for (const int child: iModel.nodes[node].children) {
			if (child >= 0 && static_cast<size_t>(child) < parents.size())
				parents[static_cast<size_t>(child)] = static_cast<int32_t>(node);
		}
	}
	return parents;
}

// World matrix of a node: the chain of the local matrices from its root, the depth bounded by the node count.
auto worldMatrix(const tinygltf::Model& iModel, const std::vector<int32_t>& iParents, const int32_t iNode)
		-> math::mat4 {
	math::mat4 world = math::identity<float, 4>();
	int32_t node = iNode;
	for (size_t depth = 0; node >= 0 && depth <= iModel.nodes.size(); ++depth) {
		world = localTransform(iModel.nodes[static_cast<size_t>(node)]).toMatrix() * world;
		node = iParents[static_cast<size_t>(node)];
	}
	return world;
}

// Elements of an accessor as floats: float components as is, integer ones normalized or not.
template<size_t N>
auto readComponents(const tinygltf::Model& iModel, const int iAccessor, const bool iNormalize)
		-> std::vector<std::array<float, N>> {
	if (iAccessor < 0 || static_cast<size_t>(iAccessor) >= iModel.accessors.size())
		return {};
	const auto& accessor = iModel.accessors[static_cast<size_type>(iAccessor)];
	if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= iModel.bufferViews.size())
		return {};
	const auto& view = iModel.bufferViews[static_cast<size_type>(accessor.bufferView)];
	if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= iModel.buffers.size())
		return {};
	const auto& buffer = iModel.buffers[static_cast<size_type>(view.buffer)];
	const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
	const int stride = accessor.ByteStride(view);
	const size_t start = view.byteOffset + accessor.byteOffset;
	if (componentSize <= 0 || stride <= 0 || accessor.count == 0 ||
		start + static_cast<size_t>(stride) * (accessor.count - 1) + N * static_cast<size_t>(componentSize) >
				buffer.data.size()) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Accessor {} overflows its buffer, ignored.", iAccessor)
		return {};
	}
	std::vector<std::array<float, N>> values(accessor.count);
	for (size_t i = 0; i < accessor.count; ++i) {
		const unsigned char* element = &buffer.data[start + i * static_cast<size_t>(stride)];
		for (size_t c = 0; c < N; ++c) {
			const unsigned char* at = element + c * static_cast<size_t>(componentSize);
			float value = 0.f;
			switch (accessor.componentType) {
				case TINYGLTF_COMPONENT_TYPE_FLOAT:
					std::memcpy(&value, at, sizeof(float));
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					value = static_cast<float>(*at) / (iNormalize ? 255.f : 1.f);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						uint16_t raw = 0;
						std::memcpy(&raw, at, sizeof(uint16_t));
						value = static_cast<float>(raw) / (iNormalize ? 65535.f : 1.f);
						break;
					}
				default:
					OWL_CORE_WARN("MeshLoader::loadGltf: Accessor {} has an unsupported component type, ignored.",
								  iAccessor)
					return {};
			}
			values[i][c] = value;
		}
	}
	return values;
}

// Bones in depth-first order from their roots, so that every parent comes before its children.
void orderBones(const std::vector<int32_t>& iJoints, const std::vector<int32_t>& iBoneParentJoint,
				std::vector<size_t>& oOrder) {
	std::vector<std::vector<size_t>> children(iJoints.size());
	std::vector<size_t> roots;
	for (size_t joint = 0; joint < iJoints.size(); ++joint) {
		if (iBoneParentJoint[joint] < 0)
			roots.push_back(joint);
		else
			children[static_cast<size_t>(iBoneParentJoint[joint])].push_back(joint);
	}
	std::vector<size_t> stack;
	for (const size_t root: std::views::reverse(roots)) stack.push_back(root);
	while (!stack.empty() && oOrder.size() <= iJoints.size()) {
		const size_t joint = stack.back();
		stack.pop_back();
		oOrder.push_back(joint);
		for (const size_t child: std::views::reverse(children[joint])) stack.push_back(child);
	}
}

}// namespace

auto importSkin(const tinygltf::Model& iModel, const std::string& iName) -> SkinImport {
	SkinImport result;
	result.nodeToBone.assign(iModel.nodes.size(), -1);
	if (iModel.skins.empty())
		return result;
	if (iModel.skins.size() > 1)
		OWL_CORE_WARN("MeshLoader::loadGltf: {} has {} skins, only the first one is read.", iName, iModel.skins.size())
	const auto& skin = iModel.skins.front();
	std::vector<int32_t> joints;
	for (const int joint: skin.joints) {
		if (joint < 0 || static_cast<size_t>(joint) >= iModel.nodes.size()) {
			OWL_CORE_WARN("MeshLoader::loadGltf: Skin of {} names an unknown node, skin ignored.", iName)
			return result;
		}
		joints.push_back(joint);
	}
	if (joints.empty() || joints.size() > animation::Skeleton::g_maxBones) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Skin of {} has {} joints (1 to {} drawn), skin ignored.", iName,
					  joints.size(), animation::Skeleton::g_maxBones)
		return result;
	}
	const auto parents = parentsOf(iModel);
	std::vector<int32_t> nodeToJoint(iModel.nodes.size(), -1);
	for (size_t joint = 0; joint < joints.size(); ++joint)
		nodeToJoint[static_cast<size_t>(joints[joint])] = static_cast<int32_t>(joint);
	// The parent bone of a joint is its nearest ancestor that is a joint; the nodes in between are its anchor.
	std::vector<int32_t> parentJoint(joints.size(), -1);
	std::vector<int32_t> anchorNode(joints.size(), -1);
	for (size_t joint = 0; joint < joints.size(); ++joint) {
		int32_t node = parents[static_cast<size_t>(joints[joint])];
		anchorNode[joint] = node;
		for (size_t depth = 0; node >= 0 && depth <= iModel.nodes.size(); ++depth) {
			if (nodeToJoint[static_cast<size_t>(node)] >= 0) {
				parentJoint[joint] = nodeToJoint[static_cast<size_t>(node)];
				break;
			}
			node = parents[static_cast<size_t>(node)];
		}
	}
	std::vector<size_t> order;
	orderBones(joints, parentJoint, order);
	if (order.size() != joints.size()) {
		OWL_CORE_WARN("MeshLoader::loadGltf: Skin of {} is not a tree, skin ignored.", iName)
		return result;
	}
	const auto inverseBinds = readComponents<16>(iModel, skin.inverseBindMatrices, false);
	result.jointToBone.resize(joints.size());
	for (size_t bone = 0; bone < order.size(); ++bone) result.jointToBone[order[bone]] = static_cast<uint16_t>(bone);
	std::vector<animation::Bone> bones;
	bones.reserve(order.size());
	for (const size_t joint: order) {
		const auto& node = iModel.nodes[static_cast<size_t>(joints[joint])];
		animation::Bone bone;
		bone.name = node.name.empty() ? std::format("bone_{}", joints[joint]) : node.name;
		bone.rest = localTransform(node);
		if (parentJoint[joint] >= 0) {
			bone.parent = result.jointToBone[static_cast<size_t>(parentJoint[joint])];
			bone.anchor = math::identity<float, 4>();
		} else if (anchorNode[joint] >= 0) {
			bone.anchor = worldMatrix(iModel, parents, anchorNode[joint]);
		}
		if (joint < inverseBinds.size()) {
			const std::vector<double> values(inverseBinds[joint].begin(), inverseBinds[joint].end());
			bone.inverseBind = toMatrix(values);
		}
		result.nodeToBone[static_cast<size_t>(joints[joint])] = static_cast<int32_t>(bones.size());
		bones.push_back(std::move(bone));
	}
	result.skeleton = mkShared<const animation::Skeleton>(std::move(bones));
	return result;
}

auto readPrimitiveSkin(const tinygltf::Model& iModel, const tinygltf::Primitive& iPrimitive, const SkinImport& iSkin,
					   const size_t iVertexCount) -> std::vector<VertexSkin> {
	std::vector<VertexSkin> skin(iVertexCount);
	const auto attribute = [&iPrimitive](const char* iKey) -> int {
		const auto found = iPrimitive.attributes.find(iKey);
		return found == iPrimitive.attributes.end() ? -1 : found->second;
	};
	if (attribute("JOINTS_1") >= 0)
		OWL_CORE_WARN("MeshLoader::loadGltf: JOINTS_1 ignored, four joints per vertex at most.")
	const auto joints = readComponents<4>(iModel, attribute("JOINTS_0"), false);
	const auto weights = readComponents<4>(iModel, attribute("WEIGHTS_0"), true);
	const size_t count = std::min({iVertexCount, joints.size(), weights.size()});
	for (size_t vertex = 0; vertex < count; ++vertex) {
		float sum = 0.f;
		for (size_t slot = 0; slot < 4; ++slot) {
			const auto joint = static_cast<size_t>(joints[vertex][slot]);
			const float weight = std::max(weights[vertex][slot], 0.f);
			if (joint >= iSkin.jointToBone.size() || weight <= 0.f)
				continue;
			skin[vertex].joints[slot] = iSkin.jointToBone[joint];
			skin[vertex].weights[slot] = weight;
			sum += weight;
		}
		if (sum > 0.f)
			skin[vertex].weights = skin[vertex].weights * (1.f / sum);
	}
	return skin;
}

auto importClips(const tinygltf::Model& iModel, const SkinImport& iSkin, const std::string& iName)
		-> std::vector<animation::SkeletalClip> {
	std::vector<animation::SkeletalClip> clips;
	if (!iSkin.skeleton)
		return clips;
	for (size_t index = 0; index < iModel.animations.size(); ++index) {
		const auto& animation = iModel.animations[index];
		animation::SkeletalClip clip;
		clip.name = animation.name.empty() ? std::format("animation_{}", index) : animation.name;
		for (const auto& channel: animation.channels) {
			if (channel.target_node < 0 || static_cast<size_t>(channel.target_node) >= iSkin.nodeToBone.size() ||
				channel.sampler < 0 || static_cast<size_t>(channel.sampler) >= animation.samplers.size())
				continue;
			const int32_t bone = iSkin.nodeToBone[static_cast<size_t>(channel.target_node)];
			if (bone < 0)
				continue;
			const auto& sampler = animation.samplers[static_cast<size_t>(channel.sampler)];
			const auto times = readComponents<1>(iModel, sampler.input, false);
			if (times.empty())
				continue;
			const bool cubic = sampler.interpolation == "CUBICSPLINE";
			const auto interpolation =
					sampler.interpolation == "STEP" ? animation::Interpolation::Step : animation::Interpolation::Linear;
			const size_t stride = cubic ? 3 : 1;
			auto track = std::ranges::find(clip.tracks, static_cast<uint32_t>(bone), &animation::BoneTrack::bone);
			if (track == clip.tracks.end()) {
				clip.tracks.push_back(
						{.bone = static_cast<uint32_t>(bone), .translation = {}, .rotation = {}, .scale = {}});
				track = std::prev(clip.tracks.end());
			}
			std::vector<float> keyTimes;
			keyTimes.reserve(times.size());
			for (const auto& time: times) keyTimes.push_back(time[0]);
			clip.duration = std::max(clip.duration, keyTimes.back());
			if (channel.target_path == "rotation") {
				const auto values = readComponents<4>(iModel, sampler.output, true);
				if (values.size() < keyTimes.size() * stride)
					continue;
				track->rotation.times = keyTimes;
				track->rotation.interpolation = interpolation;
				for (size_t key = 0; key < keyTimes.size(); ++key) {
					const auto& v = values[key * stride + (cubic ? 1 : 0)];
					track->rotation.values.push_back(math::quat{v[3], v[0], v[1], v[2]}.normalized());
				}
			} else if (channel.target_path == "translation" || channel.target_path == "scale") {
				const auto values = readComponents<3>(iModel, sampler.output, false);
				if (values.size() < keyTimes.size() * stride)
					continue;
				auto& vectorTrack = channel.target_path == "translation" ? track->translation : track->scale;
				vectorTrack.times = keyTimes;
				vectorTrack.interpolation = interpolation;
				for (size_t key = 0; key < keyTimes.size(); ++key) {
					const auto& v = values[key * stride + (cubic ? 1 : 0)];
					vectorTrack.values.emplace_back(v[0], v[1], v[2]);
				}
			}
		}
		if (clip.tracks.empty()) {
			OWL_CORE_WARN("MeshLoader::loadGltf: Animation {} of {} moves no bone, ignored.", clip.name, iName)
			continue;
		}
		clips.push_back(std::move(clip));
	}
	return clips;
}

auto importSockets(const tinygltf::Model& iModel, const SkinImport& iSkin) -> std::vector<MeshSocket> {
	std::vector<MeshSocket> sockets;
	const auto parents = parentsOf(iModel);
	for (size_t node = 0; node < iModel.nodes.size(); ++node) {
		const auto& name = iModel.nodes[node].name;
		if (name.size() <= g_socketPrefix.size() ||
			!std::ranges::equal(std::string_view{name}.substr(0, g_socketPrefix.size()), g_socketPrefix,
								[](const char iA, const char iB) -> bool {
									return std::tolower(static_cast<unsigned char>(iA)) == iB;
								}))
			continue;
		MeshSocket socket;
		socket.name = name.substr(g_socketPrefix.size());
		math::mat4 offset = math::identity<float, 4>();
		auto current = static_cast<int32_t>(node);
		for (size_t depth = 0; current >= 0 && depth <= iModel.nodes.size(); ++depth) {
			if (const int32_t bone = iSkin.nodeToBone[static_cast<size_t>(current)]; bone >= 0 && iSkin.skeleton) {
				socket.bone = iSkin.skeleton->getBones()[static_cast<size_t>(bone)].name;
				break;
			}
			offset = localTransform(iModel.nodes[static_cast<size_t>(current)]).toMatrix() * offset;
			current = parents[static_cast<size_t>(current)];
		}
		socket.offset = offset;
		sockets.push_back(std::move(socket));
	}
	return sockets;
}

}// namespace owl::data::geometry::gltf
