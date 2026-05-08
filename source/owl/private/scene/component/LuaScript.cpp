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

namespace owl::scene::component {

void LuaScript::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "scriptPath" << YAML::Value << scriptPath;
	if (!properties.empty()) {
		iOut.getImpl()->emitter << YAML::Key << "properties" << YAML::Value << YAML::BeginSeq;
		for (const auto& prop: properties) {
			iOut.getImpl()->emitter << YAML::BeginMap;
			iOut.getImpl()->emitter << YAML::Key << "name" << YAML::Value << prop.name;
			switch (prop.type) {
				case script::ScriptPropertyType::Float:
					iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "float";
					iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << std::get<float>(prop.value);
					break;
				case script::ScriptPropertyType::Int:
					iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "int";
					iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << std::get<int64_t>(prop.value);
					break;
				case script::ScriptPropertyType::String:
					iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "string";
					iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << std::get<std::string>(prop.value);
					break;
				case script::ScriptPropertyType::Bool:
					iOut.getImpl()->emitter << YAML::Key << "type" << YAML::Value << "bool";
					iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << std::get<bool>(prop.value);
					break;
			}
			iOut.getImpl()->emitter << YAML::EndMap;
		}
		iOut.getImpl()->emitter << YAML::EndSeq;
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

void LuaScript::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["scriptPath"])
		scriptPath = iNode.getImpl()->node["scriptPath"].as<std::string>();
	properties.clear();
	if (const auto propsNode = iNode.getImpl()->node["properties"]; propsNode && propsNode.IsSequence()) {
		for (const auto& propNode: propsNode) {
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
