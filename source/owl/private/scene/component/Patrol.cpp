/**
 * @file Patrol.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Patrol.h"

#include <string>

namespace owl::scene::component {

void Patrol::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Patrol
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "mode" << YAML::Value << std::string(magic_enum::enum_name(mode));
	out << YAML::Key << "speed" << YAML::Value << speed;
	out << YAML::Key << "waitTime" << YAML::Value << waitTime;
	out << YAML::Key << "wanderRadius" << YAML::Value << wanderRadius;
	out << YAML::Key << "seed" << YAML::Value << seed;
	out << YAML::Key << "faceDirection" << YAML::Value << faceDirection;
	out << YAML::Key << "hopHeight" << YAML::Value << hopHeight;
	out << YAML::Key << "hopRate" << YAML::Value << hopRate;
	out << YAML::Key << "waypoints" << YAML::Value << YAML::BeginSeq;
	for (const auto& waypoint: waypoints) out << waypoint;
	out << YAML::EndSeq;
	out << YAML::EndMap;// Patrol
}

void Patrol::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["mode"]) {
		if (const auto parsed = magic_enum::enum_cast<Mode>(node["mode"].as<std::string>()); parsed.has_value())
			mode = parsed.value();
	}
	if (node["speed"])
		speed = node["speed"].as<float>();
	if (node["waitTime"])
		waitTime = node["waitTime"].as<float>();
	if (node["wanderRadius"])
		wanderRadius = node["wanderRadius"].as<float>();
	if (node["seed"])
		seed = node["seed"].as<float>();
	if (node["faceDirection"])
		faceDirection = node["faceDirection"].as<bool>();
	if (node["hopHeight"])
		hopHeight = node["hopHeight"].as<float>();
	if (node["hopRate"])
		hopRate = node["hopRate"].as<float>();
	waypoints.clear();
	if (const auto list = node["waypoints"]; list.isSequence()) {
		for (const auto item: list) waypoints.push_back(item.as<math::vec3>());
	}
	m_captured = false;
}

}// namespace owl::scene::component
