/**
 * @file UISlider.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UISlider.h"

namespace owl::scene::component {

void UISlider::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "value" << YAML::Value << value;
	iOut.getImpl()->emitter << YAML::Key << "minValue" << YAML::Value << minValue;
	iOut.getImpl()->emitter << YAML::Key << "maxValue" << YAML::Value << maxValue;
	iOut.getImpl()->emitter << YAML::Key << "trackColor" << YAML::Value << trackColor;
	iOut.getImpl()->emitter << YAML::Key << "fillColor" << YAML::Value << fillColor;
	iOut.getImpl()->emitter << YAML::Key << "handleColor" << YAML::Value << handleColor;
	iOut.getImpl()->emitter << YAML::Key << "onValueChangedCallback" << YAML::Value << onValueChangedCallback;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UISlider::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["value"])
		value = iNode.getImpl()->node["value"].as<float>();
	if (iNode.getImpl()->node["minValue"])
		minValue = iNode.getImpl()->node["minValue"].as<float>();
	if (iNode.getImpl()->node["maxValue"])
		maxValue = iNode.getImpl()->node["maxValue"].as<float>();
	if (iNode.getImpl()->node["trackColor"])
		trackColor = iNode.getImpl()->node["trackColor"].as<math::vec4>();
	if (iNode.getImpl()->node["fillColor"])
		fillColor = iNode.getImpl()->node["fillColor"].as<math::vec4>();
	if (iNode.getImpl()->node["handleColor"])
		handleColor = iNode.getImpl()->node["handleColor"].as<math::vec4>();
	if (iNode.getImpl()->node["onValueChangedCallback"])
		onValueChangedCallback = iNode.getImpl()->node["onValueChangedCallback"].as<std::string>();
}

}// namespace owl::scene::component
