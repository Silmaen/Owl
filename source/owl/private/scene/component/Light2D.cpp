/**
 * @file Light2D.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Light2D.h"

#include <algorithm>
#include <string>

namespace owl::scene::component {

void Light2D::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Light2D
	out << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(type));
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "color" << YAML::Value << color;
	out << YAML::Key << "intensity" << YAML::Value << intensity;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "falloff" << YAML::Value << falloff;
	out << YAML::Key << "height" << YAML::Value << height;
	out << YAML::Key << "innerAngle" << YAML::Value << innerAngle;
	out << YAML::Key << "outerAngle" << YAML::Value << outerAngle;
	out << YAML::Key << "castShadows" << YAML::Value << castShadows;
	out << YAML::Key << "shadowSoftness" << YAML::Value << shadowSoftness;
	out << YAML::EndMap;// Light2D
}

void Light2D::deserialize(const core::Serializer& iNode) {
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
		intensity = std::max(node["intensity"].as<float>(), 0.f);
	if (node["radius"])
		radius = std::max(node["radius"].as<float>(), 0.01f);
	if (node["falloff"])
		falloff = std::clamp(node["falloff"].as<float>(), 0.1f, 8.f);
	if (node["height"])
		height = std::max(node["height"].as<float>(), 0.f);
	if (node["innerAngle"])
		innerAngle = node["innerAngle"].as<float>();
	if (node["outerAngle"])
		outerAngle = node["outerAngle"].as<float>();
	if (node["castShadows"])
		castShadows = node["castShadows"].as<bool>();
	if (node["shadowSoftness"])
		shadowSoftness = std::clamp(node["shadowSoftness"].as<float>(), 0.f, 4.f);
}

}// namespace owl::scene::component
