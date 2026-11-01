/**
 * @file UiPanel.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UiStyleYaml.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiPanel.h"

namespace owl::scene::component {

void UiPanel::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
	emitter << YAML::Key << "clipChildren" << YAML::Value << clipChildren;
	utils::emitBox(emitter, box);
	utils::emitStyleRef(emitter, style);
	emitter << YAML::EndMap;
}

// Top-level `borderColor` / `borderWidth` keys are ignored: the border drawn is the one of the box.
void UiPanel::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["backgroundColor"])
		backgroundColor = node["backgroundColor"].as<math::vec4>();
	if (node["clipChildren"])
		clipChildren = node["clipChildren"].as<bool>();
	utils::readBox(node, box);
	utils::readStyleRef(node, style);
}

}// namespace owl::scene::component
