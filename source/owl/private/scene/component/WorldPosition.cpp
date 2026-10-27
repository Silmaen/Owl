/**
 * @file WorldPosition.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/WorldPosition.h"

namespace owl::scene::component {

void WorldPosition::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// WorldPosition
	out << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << position.x() << position.y()
		<< position.z() << YAML::EndSeq;
	out << YAML::EndMap;// WorldPosition
}

void WorldPosition::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["position"])
		position = node["position"].as<math::vec3d>();
	m_synced = false;
}

}// namespace owl::scene::component
