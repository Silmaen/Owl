/**
 * @file SimulationLod.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/SimulationLod.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::component {

void SimulationLod::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// SimulationLod
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "hysteresis" << YAML::Value << hysteresis;
	out << YAML::Key << "coarseInterval" << YAML::Value << coarseInterval;
	out << YAML::Key << "model" << YAML::Value << std::string(magic_enum::enum_name(model));
	out << YAML::EndMap;// SimulationLod
}

void SimulationLod::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["radius"])
		radius = std::max(node["radius"].as<float>(), 0.f);
	if (node["hysteresis"])
		hysteresis = std::max(node["hysteresis"].as<float>(), 0.f);
	if (node["coarseInterval"])
		coarseInterval = std::clamp(node["coarseInterval"].as<uint32_t>(), 1u, maxCoarseInterval);
	if (node["model"])
		model = magic_enum::enum_cast<Model>(node["model"].as<std::string>()).value_or(Model::Rails);
	m_demoted = false;
	m_coarseCountdown = 0;
	m_velocity = {0.f, 0.f, 0.f};
}

}// namespace owl::scene::component
