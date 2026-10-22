/**
 * @file LuaScript.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "scene/component/LuaScript.h"

#include <cstdint>
#include <exception>
#include <optional>
#include <string>

namespace owl::scene::component {

namespace {

void writeFields(YAML::Emitter& ioEmitter, const LuaScript& iScript) {
	ioEmitter << YAML::BeginMap;
	ioEmitter << YAML::Key << "scriptPath" << YAML::Value << iScript.scriptPath;
	if (!iScript.properties.empty()) {
		ioEmitter << YAML::Key << "properties" << YAML::Value << YAML::BeginSeq;
		for (const auto& prop: iScript.properties) {
			ioEmitter << YAML::BeginMap;
			ioEmitter << YAML::Key << "name" << YAML::Value << prop.name;
			switch (prop.type) {
				case script::ScriptPropertyType::Float:
					ioEmitter << YAML::Key << "type" << YAML::Value << "float";
					ioEmitter << YAML::Key << "value" << YAML::Value << std::get<float>(prop.value);
					break;
				case script::ScriptPropertyType::Int:
					ioEmitter << YAML::Key << "type" << YAML::Value << "int";
					ioEmitter << YAML::Key << "value" << YAML::Value << std::get<int64_t>(prop.value);
					break;
				case script::ScriptPropertyType::String:
					ioEmitter << YAML::Key << "type" << YAML::Value << "string";
					ioEmitter << YAML::Key << "value" << YAML::Value << std::get<std::string>(prop.value);
					break;
				case script::ScriptPropertyType::Bool:
					ioEmitter << YAML::Key << "type" << YAML::Value << "bool";
					ioEmitter << YAML::Key << "value" << YAML::Value << std::get<bool>(prop.value);
					break;
			}
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
		core::Serializer reader;
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
				const auto typeName = propNode["type"].as<std::string>();
				if (typeName == "float") {
					prop.type = script::ScriptPropertyType::Float;
					if (propNode["value"])
						prop.value = propNode["value"].as<float>();
				} else if (typeName == "int") {
					prop.type = script::ScriptPropertyType::Int;
					if (propNode["value"])
						prop.value = propNode["value"].as<int64_t>();
				} else if (typeName == "string") {
					prop.type = script::ScriptPropertyType::String;
					if (propNode["value"])
						prop.value = propNode["value"].as<std::string>();
				} else if (typeName == "bool") {
					prop.type = script::ScriptPropertyType::Bool;
					if (propNode["value"])
						prop.value = propNode["value"].as<bool>();
				}
			}
			if (!prop.name.empty())
				properties.push_back(std::move(prop));
		}
	}
	instance.reset();
}

}// namespace owl::scene::component
