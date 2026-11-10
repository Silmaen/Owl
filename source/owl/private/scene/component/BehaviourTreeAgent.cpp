/**
 * @file BehaviourTreeAgent.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/ai/BehaviourTreeInstance.h"
#include "scene/component/BehaviourTreeAgent.h"

#include <magic_enum/magic_enum.hpp>

#include <string>

namespace owl::scene::component {

void BehaviourTreeAgent::serialize(const core::Serializer& iOut) const {
	auto& out = iOut.getImpl()->emitter;
	out << YAML::Key << key();
	out << YAML::BeginMap;// BehaviourTreeAgent
	out << YAML::Key << "enabled" << YAML::Value << enabled;
	out << YAML::Key << "tree" << YAML::Value << tree;
	if (!overrides.empty()) {
		out << YAML::Key << "overrides" << YAML::Value << YAML::BeginSeq;
		for (const auto& entry: overrides) {
			out << YAML::Flow << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << entry.name;
			out << YAML::Key << "type" << YAML::Value
				<< std::string{magic_enum::enum_name(ai::getBlackboardType(entry.value))};
			out << YAML::Key << "value" << YAML::Value << ai::toText(entry.value);
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
	}
	out << YAML::EndMap;// BehaviourTreeAgent
}

void BehaviourTreeAgent::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	enabled = node["enabled"].as<bool>(enabled);
	tree = node["tree"].as<std::string>(tree);
	overrides.clear();
	for (const auto entry: node["overrides"]) {
		const auto type = magic_enum::enum_cast<ai::BlackboardType>(entry["type"].as<std::string>("Number"));
		if (!type.has_value())
			continue;
		if (auto value = ai::parseValue(*type, entry["value"].as<std::string>("")); value.has_value())
			overrides.push_back(
					{.name = entry["name"].as<std::string>(""), .value = std::move(*value), .description = {}});
	}
	m_instance.reset();
	m_owner = core::UUID{0};
}

}// namespace owl::scene::component
