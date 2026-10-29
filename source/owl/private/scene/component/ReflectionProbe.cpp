/**
 * @file ReflectionProbe.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/ReflectionProbe.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>

namespace owl::scene::component {

namespace {
constexpr float g_minProbeSize = 0.01f;
}// namespace

void ReflectionProbe::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;
	out << YAML::Key << "shape" << YAML::Value << std::string(magic_enum::enum_name(shape));
	out << YAML::Key << "extents" << YAML::Value << extents;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "blendDistance" << YAML::Value << blendDistance;
	out << YAML::Key << "parallax" << YAML::Value << parallax;
	out << YAML::Key << "intensity" << YAML::Value << intensity;
	out << YAML::Key << "resolution" << YAML::Value << resolution;
	out << YAML::Key << "realtime" << YAML::Value << realtime;
	out << YAML::EndMap;
}

void ReflectionProbe::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["shape"]) {
		if (const auto parsed = magic_enum::enum_cast<Shape>(node["shape"].as<std::string>()); parsed.has_value())
			shape = parsed.value();
	}
	if (node["extents"]) {
		const auto read = node["extents"].as<math::vec3>();
		extents = {std::max(read.x(), g_minProbeSize), std::max(read.y(), g_minProbeSize),
				   std::max(read.z(), g_minProbeSize)};
	}
	if (node["radius"])
		radius = std::max(node["radius"].as<float>(), g_minProbeSize);
	if (node["blendDistance"])
		blendDistance = std::max(node["blendDistance"].as<float>(), 0.f);
	if (node["parallax"])
		parallax = node["parallax"].as<bool>();
	if (node["intensity"])
		intensity = std::max(node["intensity"].as<float>(), 0.f);
	if (node["resolution"])
		resolution = std::bit_floor(
				std::clamp(node["resolution"].as<uint32_t>(), g_minProbeResolution, g_maxProbeResolution));
	if (node["realtime"])
		realtime = node["realtime"].as<bool>();
}

}// namespace owl::scene::component
