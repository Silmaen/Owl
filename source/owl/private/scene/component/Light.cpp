/**
 * @file Light.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Light.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace owl::scene::component {

void Light::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Light
	out << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(type));
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "color" << YAML::Value << color;
	out << YAML::Key << "intensity" << YAML::Value << intensity;
	out << YAML::Key << "range" << YAML::Value << range;
	out << YAML::Key << "innerAngle" << YAML::Value << innerAngle;
	out << YAML::Key << "outerAngle" << YAML::Value << outerAngle;
	out << YAML::Key << "castShadows" << YAML::Value << castShadows;
	out << YAML::Key << "shadowDistance" << YAML::Value << shadowDistance;
	out << YAML::Key << "cascadeCount" << YAML::Value << cascadeCount;
	out << YAML::Key << "shadowResolution" << YAML::Value << shadowResolution;
	out << YAML::Key << "shadowBias" << YAML::Value << shadowBias;
	out << YAML::EndMap;// Light
}

void Light::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["type"]) {
		if (const auto parsed = magic_enum::enum_cast<Type>(node["type"].as<std::string>()); parsed.has_value())
			type = parsed.value();
	}
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["color"])
		color = node["color"].as<math::vec3>();
	if (node["intensity"])
		intensity = node["intensity"].as<float>();
	if (node["range"])
		range = node["range"].as<float>();
	if (node["innerAngle"])
		innerAngle = node["innerAngle"].as<float>();
	if (node["outerAngle"])
		outerAngle = node["outerAngle"].as<float>();
	if (node["castShadows"])
		castShadows = node["castShadows"].as<bool>();
	if (node["shadowDistance"])
		shadowDistance = node["shadowDistance"].as<float>();
	if (node["cascadeCount"])
		cascadeCount = std::clamp(node["cascadeCount"].as<uint32_t>(), 1u, 4u);
	if (node["shadowResolution"])
		shadowResolution = std::clamp(node["shadowResolution"].as<uint32_t>(), 256u, 4096u);
	if (node["shadowBias"])
		shadowBias = node["shadowBias"].as<float>();
}

}// namespace owl::scene::component
