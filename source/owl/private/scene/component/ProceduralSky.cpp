/**
 * @file ProceduralSky.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/ProceduralSky.h"

#include <algorithm>
#include <cstdint>

namespace owl::scene::component {

void ProceduralSky::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// ProceduralSky
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "sunDirection" << YAML::Value << sunDirection;
	out << YAML::Key << "turbidity" << YAML::Value << turbidity;
	out << YAML::Key << "groundColor" << YAML::Value << groundColor;
	out << YAML::Key << "nightColor" << YAML::Value << nightColor;
	out << YAML::Key << "nightBrightness" << YAML::Value << nightBrightness;
	out << YAML::Key << "stars" << YAML::Value << stars;
	out << YAML::Key << "discSize" << YAML::Value << discSize;
	out << YAML::Key << "resolution" << YAML::Value << resolution;
	out << YAML::EndMap;// ProceduralSky
}

void ProceduralSky::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["sunDirection"])
		sunDirection = node["sunDirection"].as<math::vec3>();
	if (node["turbidity"])
		turbidity = std::clamp(node["turbidity"].as<float>(), 1.7f, 10.f);
	if (node["groundColor"])
		groundColor = node["groundColor"].as<math::vec3>();
	if (node["nightColor"])
		nightColor = node["nightColor"].as<math::vec3>();
	if (node["nightBrightness"])
		nightBrightness = std::max(node["nightBrightness"].as<float>(), 0.f);
	if (node["stars"])
		stars = std::max(node["stars"].as<float>(), 0.f);
	if (node["discSize"])
		discSize = std::clamp(node["discSize"].as<float>(), 0.f, 20.f);
	if (node["resolution"])
		resolution = std::clamp(node["resolution"].as<uint32_t>(), 64u, 1024u);
}

}// namespace owl::scene::component
