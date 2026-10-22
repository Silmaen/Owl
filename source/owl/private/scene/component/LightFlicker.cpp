/**
 * @file LightFlicker.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/LightFlicker.h"

#include <cstdint>
#include <string>

namespace owl::scene::component {

void LightFlicker::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// LightFlicker
	out << YAML::Key << "style" << YAML::Value << std::string(magic_enum::enum_name(style));
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "amplitude" << YAML::Value << amplitude;
	out << YAML::Key << "speed" << YAML::Value << speed;
	out << YAML::Key << "seed" << YAML::Value << seed;
	out << YAML::Key << "colorShift" << YAML::Value << colorShift;
	out << YAML::Key << "positionJitter" << YAML::Value << positionJitter;
	out << YAML::Key << "dipRate" << YAML::Value << dipRate;
	out << YAML::Key << "dipDepth" << YAML::Value << dipDepth;
	if (follow != core::UUID{0})
		out << YAML::Key << "follow" << YAML::Value << static_cast<uint64_t>(follow);
	out << YAML::Key << "scaleResponse" << YAML::Value << scaleResponse;
	out << YAML::EndMap;// LightFlicker
}

void LightFlicker::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["style"]) {
		if (const auto parsed = magic_enum::enum_cast<Style>(node["style"].as<std::string>()); parsed.has_value())
			style = parsed.value();
	}
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["amplitude"])
		amplitude = node["amplitude"].as<float>();
	if (node["speed"])
		speed = node["speed"].as<float>();
	if (node["seed"])
		seed = node["seed"].as<float>();
	if (node["colorShift"])
		colorShift = node["colorShift"].as<math::vec3>();
	if (node["positionJitter"])
		positionJitter = node["positionJitter"].as<math::vec3>();
	if (node["dipRate"])
		dipRate = node["dipRate"].as<float>();
	if (node["dipDepth"])
		dipDepth = node["dipDepth"].as<float>();
	if (node["follow"])
		follow = core::UUID{node["follow"].as<uint64_t>()};
	if (node["scaleResponse"])
		scaleResponse = node["scaleResponse"].as<math::vec3>();
	m_captured = false;
}

}// namespace owl::scene::component
