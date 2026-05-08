/**
 * @file GameState.cpp
 * @author Silmaen
 * @date 13/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/GameState.h"

#include "core/SerializerImpl.h"

namespace owl::scene {

void GameState::set(const std::string& iKey, Value iValue) { m_data[iKey] = std::move(iValue); }

auto GameState::get(const std::string& iKey) const -> std::optional<Value> {
	if (const auto it = m_data.find(iKey); it != m_data.end())
		return it->second;
	return std::nullopt;
}

auto GameState::get(const std::string& iKey, const Value& iDefault) const -> Value {
	if (const auto it = m_data.find(iKey); it != m_data.end())
		return it->second;
	return iDefault;
}

void GameState::remove(const std::string& iKey) { m_data.erase(iKey); }

void GameState::clear() { m_data.clear(); }

auto GameState::keys() const -> std::vector<std::string> {
	std::vector<std::string> result;
	result.reserve(m_data.size());
	for (const auto& [key, val]: m_data)
		result.push_back(key);
	return result;
}

auto GameState::empty() const noexcept -> bool { return m_data.empty(); }

auto GameState::size() const noexcept -> size_t { return m_data.size(); }

void GameState::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << "GameState" << YAML::Value << YAML::BeginSeq;
	for (const auto& [key, value]: m_data) {
		iOut.getImpl()->emitter << YAML::BeginMap;
		iOut.getImpl()->emitter << YAML::Key << "key" << YAML::Value << key;
		std::visit(
				[&](const auto& iVal) -> void {
					using T = std::decay_t<decltype(iVal)>;
					if constexpr (std::is_same_v<T, int64_t>) {
						iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "int";
						iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << iVal;
					} else if constexpr (std::is_same_v<T, float>) {
						iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "float";
						iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << iVal;
					} else if constexpr (std::is_same_v<T, std::string>) {
						iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "string";
						iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << iVal;
					} else if constexpr (std::is_same_v<T, bool>) {
						iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "bool";
						iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << iVal;
					}
				},
				value);
		iOut.getImpl()->emitter << YAML::EndMap;
	}
	iOut.getImpl()->emitter << YAML::EndSeq;
}

void GameState::deserialize(const core::Serializer& iNode) {
	m_data.clear();
	const auto gsNode = iNode.getImpl()->node["GameState"];
	if (!gsNode || !gsNode.IsSequence())
		return;
	for (const auto& entry: gsNode) {
		if (!entry["key"] || !entry["type"] || !entry["value"])
			continue;
		const auto key = entry["key"].as<std::string>();
		const auto type = entry["type"].as<std::string>();
		if (type == "int")
			m_data[key] = entry["value"].as<int64_t>();
		else if (type == "float")
			m_data[key] = entry["value"].as<float>();
		else if (type == "string")
			m_data[key] = entry["value"].as<std::string>();
		else if (type == "bool")
			m_data[key] = entry["value"].as<bool>();
	}
}

}// namespace owl::scene
