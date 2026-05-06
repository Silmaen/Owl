/**
 * @file UiProgressBar.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiProgressBar.h"

namespace owl::scene::component {

void UiProgressBar::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << value;
	iOut.getImpl()->emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
	iOut.getImpl()->emitter << YAML::Key << "fillColor" << YAML::Value << fillColor;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiProgressBar::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["value"])
		value = iNode.getImpl()->node["value"].as<float>();
	if (iNode.getImpl()->node["backgroundColor"])
		backgroundColor = iNode.getImpl()->node["backgroundColor"].as<math::vec4>();
	if (iNode.getImpl()->node["fillColor"])
		fillColor = iNode.getImpl()->node["fillColor"].as<math::vec4>();
}

}// namespace owl::scene::component
