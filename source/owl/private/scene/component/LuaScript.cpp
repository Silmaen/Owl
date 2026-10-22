/**
 * @file LuaScript.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "core/YamlNode.h"
#include "scene/component/LuaScript.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace owl::scene::component {

namespace {

template<size_t Count>
void writeComponents(YAML::Emitter& ioEmitter, const std::array<float, Count>& iValues) {
	ioEmitter << YAML::Flow << YAML::BeginSeq;
	for (const auto component: iValues) ioEmitter << component;
	ioEmitter << YAML::EndSeq;
}

void writeValue(YAML::Emitter& ioEmitter, const script::ScriptProperty& iProperty) {
	switch (iProperty.type) {
		case script::ScriptPropertyType::Float:
			ioEmitter << std::get<float>(iProperty.value);
			break;
		case script::ScriptPropertyType::Int:
			ioEmitter << std::get<int64_t>(iProperty.value);
			break;
		case script::ScriptPropertyType::String:
			ioEmitter << std::get<std::string>(iProperty.value);
			break;
		case script::ScriptPropertyType::Bool:
			ioEmitter << std::get<bool>(iProperty.value);
			break;
		case script::ScriptPropertyType::Color:
			writeComponents(ioEmitter, std::get<std::array<float, 4>>(iProperty.value));
			break;
		case script::ScriptPropertyType::Vec2:
			writeComponents(ioEmitter, std::get<std::array<float, 2>>(iProperty.value));
			break;
		case script::ScriptPropertyType::Vec3:
			writeComponents(ioEmitter, std::get<std::array<float, 3>>(iProperty.value));
			break;
		case script::ScriptPropertyType::Entity:
			ioEmitter << std::get<uint64_t>(iProperty.value);
			break;
		case script::ScriptPropertyType::Numbers:
			ioEmitter << YAML::Flow << YAML::BeginSeq;
			for (const auto number: std::get<std::vector<double>>(iProperty.value)) ioEmitter << number;
			ioEmitter << YAML::EndSeq;
			break;
		case script::ScriptPropertyType::Strings:
			ioEmitter << YAML::BeginSeq;
			for (const auto& text: std::get<std::vector<std::string>>(iProperty.value)) ioEmitter << text;
			ioEmitter << YAML::EndSeq;
			break;
	}
}

template<size_t Count>
auto readComponents(const core::YamlNode& iNode, const std::array<float, Count>& iFallback)
		-> std::array<float, Count> {
	auto values = iFallback;
	if (iNode.isSequence() && iNode.size() == Count) {
		for (size_t index = 0; index < Count; ++index) values[index] = iNode[index].as<float>(values[index]);
	}
	return values;
}

auto readValue(const core::YamlNode& iNode, const script::ScriptPropertyType iType) -> script::ScriptValue {
	using Type = script::ScriptPropertyType;
	switch (iType) {
		case Type::Float:
			return iNode.as<float>();
		case Type::Int:
			return iNode.as<int64_t>();
		case Type::String:
			return iNode.as<std::string>();
		case Type::Bool:
			return iNode.as<bool>();
		case Type::Color:
			return readComponents<4>(iNode, {1.f, 1.f, 1.f, 1.f});
		case Type::Vec2:
			return readComponents<2>(iNode, {0.f, 0.f});
		case Type::Vec3:
			return readComponents<3>(iNode, {0.f, 0.f, 0.f});
		case Type::Entity:
			return iNode.as<uint64_t>();
		case Type::Numbers:
			{
				std::vector<double> numbers;
				if (iNode.isSequence())
					for (const auto item: iNode) numbers.push_back(item.as<double>());
				return numbers;
			}
		case Type::Strings:
			{
				std::vector<std::string> strings;
				if (iNode.isSequence())
					for (const auto item: iNode) strings.push_back(item.as<std::string>());
				return strings;
			}
	}
	return 0.0f;
}

void writeFields(YAML::Emitter& ioEmitter, const LuaScript& iScript) {
	ioEmitter << YAML::BeginMap;
	ioEmitter << YAML::Key << "scriptPath" << YAML::Value << iScript.scriptPath;
	if (!iScript.properties.empty()) {
		ioEmitter << YAML::Key << "properties" << YAML::Value << YAML::BeginSeq;
		for (const auto& prop: iScript.properties) {
			ioEmitter << YAML::BeginMap;
			ioEmitter << YAML::Key << "name" << YAML::Value << prop.name;
			ioEmitter << YAML::Key << "type" << YAML::Value
					  << std::string{script::ScriptEngine::propertyTypeName(prop.type)};
			ioEmitter << YAML::Key << "value" << YAML::Value;
			writeValue(ioEmitter, prop);
			ioEmitter << YAML::EndMap;
		}
		ioEmitter << YAML::EndSeq;
	}
	ioEmitter << YAML::EndMap;
}

}// namespace

void LuaScript::serialize(const core::Serializer& iOut) const { serializeAs(iOut, key()); }

void LuaScript::serializeAs(const core::Serializer& iOut, const char* iKey) const {
	iOut.getImpl()->emitter << YAML::Key << iKey;
	writeFields(iOut.getImpl()->emitter, *this);
}

auto LuaScript::toYaml() const -> std::string {
	YAML::Emitter emitter;
	writeFields(emitter, *this);
	return emitter.c_str();
}

auto LuaScript::fromYaml(const std::string& iText) -> std::optional<LuaScript> {
	try {
		const auto document = mkShared<const core::YamlDocument>(iText, "script");
		if (!document->getRoot().isMap())
			return std::nullopt;
		const core::Serializer reader;
		reader.getImpl()->source = document;
		reader.getImpl()->node = document->getRoot();
		LuaScript script;
		script.deserialize(reader);
		return script;
	} catch (const std::exception&) { return std::nullopt; }
}

void LuaScript::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["scriptPath"])
		scriptPath = iNode.getImpl()->node["scriptPath"].as<std::string>();
	properties.clear();
	if (const auto propsNode = iNode.getImpl()->node["properties"]; propsNode && propsNode.isSequence()) {
		for (const auto propNode: propsNode) {
			script::ScriptProperty prop;
			if (propNode["name"])
				prop.name = propNode["name"].as<std::string>();
			if (propNode["type"]) {
				if (const auto type = script::ScriptEngine::parsePropertyType(propNode["type"].as<std::string>())) {
					prop.type = *type;
					prop.value = script::ScriptEngine::defaultValueFor(*type);
					if (propNode["value"])
						prop.value = readValue(propNode["value"], *type);
					prop.defaultValue = prop.value;
				}
			}
			if (!prop.name.empty())
				properties.push_back(std::move(prop));
		}
	}
	instance.reset();
}

}// namespace owl::scene::component
