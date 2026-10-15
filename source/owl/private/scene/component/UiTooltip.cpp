/**
 * @file UiTooltip.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiTooltip.h"

#include "Vec2Yaml.h"

namespace owl::scene::component {

void UiTooltip::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "text" << YAML::Value << text;
	emitter << YAML::Key << "delay" << YAML::Value << delay;
	emitter << YAML::Key << "fontSize" << YAML::Value << fontSize;
	emitter << YAML::Key << "padding" << YAML::Value << padding;
	emitVec2(emitter, "offset", offset);
	emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
	emitter << YAML::Key << "textColor" << YAML::Value << textColor;
	emitter << YAML::EndMap;
}

void UiTooltip::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["text"])
		text = node["text"].as<std::string>();
	if (node["delay"])
		delay = node["delay"].as<float>();
	if (node["fontSize"])
		fontSize = node["fontSize"].as<float>();
	if (node["padding"])
		padding = node["padding"].as<float>();
	readVec2(node, "offset", offset);
	if (node["backgroundColor"])
		backgroundColor = node["backgroundColor"].as<math::vec4>();
	if (node["textColor"])
		textColor = node["textColor"].as<math::vec4>();
}

}// namespace owl::scene::component
