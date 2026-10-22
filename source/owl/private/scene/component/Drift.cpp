/**
 * @file Drift.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Drift.h"

namespace owl::scene::component {

void Drift::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Drift
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "velocity" << YAML::Value << velocity;
	out << YAML::Key << "range" << YAML::Value << range;
	out << YAML::Key << "cameraFollow" << YAML::Value << cameraFollow;
	out << YAML::EndMap;// Drift
}

void Drift::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["velocity"])
		velocity = node["velocity"].as<math::vec3>();
	if (node["range"])
		range = node["range"].as<math::vec3>();
	if (node["cameraFollow"])
		cameraFollow = node["cameraFollow"].as<float>();
	m_captured = false;
}

}// namespace owl::scene::component
