/**
 * @file Oscillator.cpp
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Oscillator.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace owl::scene::component {

auto Oscillator::share(const float iSeconds) const -> float {
	const float cycle = iSeconds / std::max(period, 0.001f) + phase;
	const float unit = cycle - std::floor(cycle);
	if (easing == Easing::Smooth)
		return 0.5f - 0.5f * std::cos(2.f * std::numbers::pi_v<float> * unit);
	return unit < 0.5f ? 2.f * unit : 2.f - 2.f * unit;
}

void Oscillator::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Oscillator
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "offset" << YAML::Value << offset;
	out << YAML::Key << "period" << YAML::Value << period;
	out << YAML::Key << "phase" << YAML::Value << phase;
	out << YAML::Key << "easing" << YAML::Value << std::string(magic_enum::enum_name(easing));
	out << YAML::EndMap;// Oscillator
}

void Oscillator::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["offset"])
		offset = node["offset"].as<math::vec3>();
	if (node["period"])
		period = node["period"].as<float>();
	if (node["phase"])
		phase = node["phase"].as<float>();
	if (node["easing"]) {
		if (const auto parsed = magic_enum::enum_cast<Easing>(node["easing"].as<std::string>()); parsed.has_value())
			easing = parsed.value();
	}
	m_captured = false;
}

}// namespace owl::scene::component
