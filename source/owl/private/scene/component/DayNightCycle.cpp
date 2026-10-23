/**
 * @file DayNightCycle.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/DayNightCycle.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace owl::scene::component {

auto DayNightCycle::getSunDirection() const -> math::vec3 {
	constexpr float pi = std::numbers::pi_v<float>;
	const float hour = (timeOfDay - 6.f) / 24.f * 2.f * pi;
	const float elevation = std::clamp(maxElevation, 1.f, 90.f) * pi / 180.f;
	const float east = std::cos(hour);
	const float up = std::sin(hour) * std::sin(elevation);
	const float south = std::sin(hour) * std::cos(elevation);
	const float heading = azimuth * pi / 180.f;
	const math::vec3 rise{std::sin(heading), 0.f, -std::cos(heading)};
	const math::vec3 noon{-rise.z(), 0.f, rise.x()};
	return math::vec3{rise.x() * east + noon.x() * south, up, rise.z() * east + noon.z() * south}.normalized();
}

void DayNightCycle::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// DayNightCycle
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "timeOfDay" << YAML::Value << timeOfDay;
	out << YAML::Key << "dayLength" << YAML::Value << dayLength;
	out << YAML::Key << "azimuth" << YAML::Value << azimuth;
	out << YAML::Key << "maxElevation" << YAML::Value << maxElevation;
	out << YAML::Key << "sun" << YAML::Value << static_cast<uint64_t>(sun);
	out << YAML::Key << "sunIlluminance" << YAML::Value << sunIlluminance;
	out << YAML::Key << "sunColor" << YAML::Value << sunColor;
	out << YAML::Key << "sunsetColor" << YAML::Value << sunsetColor;
	out << YAML::Key << "moonIlluminance" << YAML::Value << moonIlluminance;
	out << YAML::Key << "moonColor" << YAML::Value << moonColor;
	out << YAML::Key << "nightExposure" << YAML::Value << nightExposure;
	out << YAML::EndMap;// DayNightCycle
}

void DayNightCycle::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["timeOfDay"])
		timeOfDay = std::fmod(std::fmod(node["timeOfDay"].as<float>(), 24.f) + 24.f, 24.f);
	if (node["dayLength"])
		dayLength = std::max(node["dayLength"].as<float>(), 1.f);
	if (node["azimuth"])
		azimuth = node["azimuth"].as<float>();
	if (node["maxElevation"])
		maxElevation = std::clamp(node["maxElevation"].as<float>(), 1.f, 90.f);
	if (node["sun"])
		sun = core::UUID{node["sun"].as<uint64_t>()};
	if (node["sunIlluminance"])
		sunIlluminance = std::max(node["sunIlluminance"].as<float>(), 0.f);
	if (node["sunColor"])
		sunColor = node["sunColor"].as<math::vec3>();
	if (node["sunsetColor"])
		sunsetColor = node["sunsetColor"].as<math::vec3>();
	if (node["moonIlluminance"])
		moonIlluminance = std::max(node["moonIlluminance"].as<float>(), 0.f);
	if (node["moonColor"])
		moonColor = node["moonColor"].as<math::vec3>();
	if (node["nightExposure"])
		nightExposure = std::clamp(node["nightExposure"].as<float>(), -10.f, 10.f);
	m_appliedExposure = 0.f;
}

}// namespace owl::scene::component
