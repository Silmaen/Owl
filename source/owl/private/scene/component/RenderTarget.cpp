/**
 * @file RenderTarget.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/RenderTarget.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::component {

void RenderTarget::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// RenderTarget
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "targetName" << YAML::Value << targetName;
	out << YAML::Key << "size" << YAML::Value << YAML::Flow << YAML::BeginSeq << size.x() << size.y() << YAML::EndSeq;
	out << YAML::Key << "clearColor" << YAML::Value << clearColor;
	out << YAML::EndMap;// RenderTarget
}

void RenderTarget::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["targetName"])
		targetName = node["targetName"].as<std::string>();
	if (const auto read = node["size"]; read.isSequence() && read.size() == 2)
		size = {std::clamp(read[0].as<uint32_t>(), 1u, 4096u), std::clamp(read[1].as<uint32_t>(), 1u, 4096u)};
	if (node["clearColor"])
		clearColor = node["clearColor"].as<math::vec4>();
	m_framebuffer.reset();
}

}// namespace owl::scene::component
