/**
 * @file GravitySource.cpp
 * @author Silmaen
 * @date 06/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/GravitySource.h"

#include <cmath>

namespace owl::scene::component {

auto GravitySource::getGravitationalParameter() const -> double {
	const auto surface = static_cast<double>(std::max(radius, minRadius));
	return static_cast<double>(strength) * surface * surface;
}

auto GravitySource::getAcceleration(const math::vec3f& iOffset) const -> math::vec3f {
	const float distanceSq = iOffset.normSq();
	if (!enabled || (range > 0.f && distanceSq > range * range))
		return {0.f, 0.f, 0.f};
	if (type == Type::Uniform) {
		const float length = direction.norm();
		return length > 0.f ? direction * (strength / length) : math::vec3f{0.f, 0.f, 0.f};
	}
	const float distance = std::sqrt(distanceSq);
	if (distance <= 0.f)
		return {0.f, 0.f, 0.f};
	const float surface = std::max(radius, minRadius);
	float pull = strength;
	if (distance >= surface) {
		const float ratio = surface / distance;
		pull = strength * ratio * ratio;
	} else if (type == Type::Spherical)
		pull = strength * distance / surface;
	return iOffset * (-pull / distance);
}

void GravitySource::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// GravitySource
	out << YAML::Key << "type" << YAML::Value << std::string(magic_enum::enum_name(type));
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "strength" << YAML::Value << strength;
	out << YAML::Key << "radius" << YAML::Value << radius;
	out << YAML::Key << "range" << YAML::Value << range;
	out << YAML::Key << "direction" << YAML::Value << direction;
	out << YAML::EndMap;// GravitySource
}

void GravitySource::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["type"])
		type = magic_enum::enum_cast<Type>(node["type"].as<std::string>()).value_or(Type::Spherical);
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["strength"])
		strength = node["strength"].as<float>();
	if (node["radius"])
		radius = std::max(node["radius"].as<float>(), minRadius);
	if (node["range"])
		range = std::max(node["range"].as<float>(), 0.f);
	if (node["direction"])
		direction = node["direction"].as<math::vec3f>();
}

}// namespace owl::scene::component
