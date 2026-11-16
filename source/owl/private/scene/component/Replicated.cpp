/**
 * @file Replicated.cpp
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/Replicated.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::scene::component {

void Replicated::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// Replicated
	out << YAML::Key << "authority" << YAML::Value << std::string(magic_enum::enum_name(authority));
	out << YAML::Key << "owner" << YAML::Value << owner;
	out << YAML::Key << "transform" << YAML::Value << transform;
	if (!components.empty())
		out << YAML::Key << "components" << YAML::Value << YAML::Flow << components;
	out << YAML::Key << "interpolate" << YAML::Value << interpolate;
	out << YAML::Key << "predict" << YAML::Value << predict;
	out << YAML::Key << "relevance" << YAML::Value << relevance;
	out << YAML::Key << "priority" << YAML::Value << priority;
	out << YAML::Key << "hitRadius" << YAML::Value << hitRadius;
	out << YAML::EndMap;// Replicated
}

void Replicated::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["authority"]) {
		if (const auto parsed = magic_enum::enum_cast<Authority>(node["authority"].as<std::string>());
			parsed.has_value())
			authority = parsed.value();
	}
	if (node["owner"])
		owner = node["owner"].as<uint32_t>();
	if (node["transform"])
		transform = node["transform"].as<bool>();
	components.clear();
	if (const auto list = node["components"]; list.isSequence()) {
		for (const auto item: list) components.push_back(item.as<std::string>());
	}
	if (node["interpolate"])
		interpolate = node["interpolate"].as<bool>();
	if (node["predict"])
		predict = node["predict"].as<bool>();
	if (node["relevance"])
		relevance = std::max(0.f, node["relevance"].as<float>());
	if (node["priority"])
		priority = std::max(0.f, node["priority"].as<float>());
	if (node["hitRadius"])
		hitRadius = std::max(0.f, node["hitRadius"].as<float>());
}

}// namespace owl::scene::component
