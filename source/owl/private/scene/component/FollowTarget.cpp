/**
 * @file FollowTarget.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/FollowTarget.h"

#include <cstdint>

namespace owl::scene::component {

void FollowTarget::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// FollowTarget
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "target" << YAML::Value << static_cast<uint64_t>(target);
	out << YAML::Key << "offset" << YAML::Value << offset;
	out << YAML::Key << "followYaw" << YAML::Value << followYaw;
	out << YAML::EndMap;// FollowTarget
}

void FollowTarget::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["target"])
		target = core::UUID{node["target"].as<uint64_t>()};
	if (node["offset"])
		offset = node["offset"].as<math::vec3>();
	if (node["followYaw"])
		followYaw = node["followYaw"].as<bool>();
	m_captured = false;
}

}// namespace owl::scene::component
