/**
 * @file VoxelPlayer.cpp
 * @author Silmaen
 * @date 08/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/VoxelPlayer.h"

namespace owl::scene::component {

void VoxelPlayer::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;// VoxelPlayer
	emitter << YAML::Key << "WalkSpeed" << YAML::Value << walkSpeed;
	emitter << YAML::Key << "RunSpeed" << YAML::Value << runSpeed;
	emitter << YAML::Key << "JumpSpeed" << YAML::Value << jumpSpeed;
	emitter << YAML::Key << "Gravity" << YAML::Value << gravity;
	emitter << YAML::Key << "LookSpeed" << YAML::Value << lookSpeed;
	emitter << YAML::Key << "MouseSensitivity" << YAML::Value << mouseSensitivity;
	emitter << YAML::Key << "FlySpeed" << YAML::Value << flySpeed;
	emitter << YAML::Key << "SuperSpeedMultiplier" << YAML::Value << superSpeedMultiplier;
	emitter << YAML::Key << "HalfExtents" << YAML::Value << YAML::Flow << YAML::BeginSeq << halfExtents.x()
			<< halfExtents.y() << halfExtents.z() << YAML::EndSeq;
	emitter << YAML::Key << "Reach" << YAML::Value << reach;
	emitter << YAML::Key << "PlaceBlock" << YAML::Value << static_cast<uint32_t>(placeBlock);
	emitter << YAML::Key << "CaptureCursor" << YAML::Value << captureCursor;
	emitter << YAML::EndMap;// VoxelPlayer
}

void VoxelPlayer::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["WalkSpeed"])
		walkSpeed = node["WalkSpeed"].as<float>();
	if (node["RunSpeed"])
		runSpeed = node["RunSpeed"].as<float>();
	if (node["JumpSpeed"])
		jumpSpeed = node["JumpSpeed"].as<float>();
	if (node["Gravity"])
		gravity = node["Gravity"].as<float>();
	if (node["LookSpeed"])
		lookSpeed = node["LookSpeed"].as<float>();
	if (node["MouseSensitivity"])
		mouseSensitivity = node["MouseSensitivity"].as<float>();
	if (node["FlySpeed"])
		flySpeed = node["FlySpeed"].as<float>();
	if (node["SuperSpeedMultiplier"])
		superSpeedMultiplier = node["SuperSpeedMultiplier"].as<float>();
	if (const auto he = node["HalfExtents"]; he && he.IsSequence() && he.size() >= 3)
		halfExtents = math::vec3{he[0].as<float>(), he[1].as<float>(), he[2].as<float>()};
	if (node["Reach"])
		reach = node["Reach"].as<float>();
	if (node["PlaceBlock"])
		placeBlock = static_cast<data::voxel::BlockId>(node["PlaceBlock"].as<uint32_t>());
	if (node["CaptureCursor"])
		captureCursor = node["CaptureCursor"].as<bool>();
}

}// namespace owl::scene::component
