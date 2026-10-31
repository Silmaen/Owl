/**
 * @file HudMarker.cpp
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/HudMarker.h"

#include "Vec2Yaml.h"

namespace owl::scene::component {

void HudMarker::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "enabled" << YAML::Value << enabled;
	if (icon)
		emitter << YAML::Key << "icon" << YAML::Value << icon->getSerializeString();
	emitter << YAML::Key << "color" << YAML::Value << color;
	emitVec2(emitter, "size", size);
	emitter << YAML::Key << "offset" << YAML::Value << offset;
	if (!label.empty())
		emitter << YAML::Key << "label" << YAML::Value << label;
	emitter << YAML::Key << "labelColor" << YAML::Value << labelColor;
	emitter << YAML::Key << "fontSize" << YAML::Value << fontSize;
	emitter << YAML::Key << "showDistance" << YAML::Value << showDistance;
	emitter << YAML::Key << "maxDistance" << YAML::Value << maxDistance;
	emitter << YAML::Key << "offscreenIndicator" << YAML::Value << offscreenIndicator;
	emitter << YAML::Key << "edgeMargin" << YAML::Value << edgeMargin;
	if (arrow)
		emitter << YAML::Key << "arrow" << YAML::Value << arrow->getSerializeString();
	emitter << YAML::EndMap;
}

void HudMarker::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["enabled"])
		enabled = node["enabled"].as<bool>();
	if (node["icon"])
		icon = renderer::gpu::Texture2D::createFromSerializedForDeserialize(node["icon"].as<std::string>());
	if (node["color"])
		color = node["color"].as<math::vec4>();
	readVec2(node, "size", size);
	if (node["offset"])
		offset = node["offset"].as<math::vec3>();
	if (node["label"])
		label = node["label"].as<std::string>();
	if (node["labelColor"])
		labelColor = node["labelColor"].as<math::vec4>();
	if (node["fontSize"])
		fontSize = node["fontSize"].as<float>();
	if (node["showDistance"])
		showDistance = node["showDistance"].as<bool>();
	if (node["maxDistance"])
		maxDistance = node["maxDistance"].as<float>();
	if (node["offscreenIndicator"])
		offscreenIndicator = node["offscreenIndicator"].as<bool>();
	if (node["edgeMargin"])
		edgeMargin = node["edgeMargin"].as<float>();
	if (node["arrow"])
		arrow = renderer::gpu::Texture2D::createFromSerializedForDeserialize(node["arrow"].as<std::string>());
}

}// namespace owl::scene::component
