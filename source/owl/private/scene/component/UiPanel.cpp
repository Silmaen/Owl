/**
 * @file UiPanel.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiPanel.h"

namespace owl::scene::component {

void UiPanel::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
	iOut.getImpl()->emitter << YAML::Key << "borderColor" << YAML::Value << borderColor;
	iOut.getImpl()->emitter << YAML::Key << "borderWidth" << YAML::Value << borderWidth;
	iOut.getImpl()->emitter << YAML::Key << "clipChildren" << YAML::Value << clipChildren;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiPanel::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["backgroundColor"])
		backgroundColor = iNode.getImpl()->node["backgroundColor"].as<math::vec4>();
	if (iNode.getImpl()->node["borderColor"])
		borderColor = iNode.getImpl()->node["borderColor"].as<math::vec4>();
	if (iNode.getImpl()->node["borderWidth"])
		borderWidth = iNode.getImpl()->node["borderWidth"].as<float>();
	if (iNode.getImpl()->node["clipChildren"])
		clipChildren = iNode.getImpl()->node["clipChildren"].as<bool>();
}

}// namespace owl::scene::component
