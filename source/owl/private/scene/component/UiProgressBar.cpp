/**
 * @file UiProgressBar.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UiStyleYaml.h"
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
	utils::emitBox(iOut.getImpl()->emitter, box);
	utils::emitStyleRef(iOut.getImpl()->emitter, style);
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiProgressBar::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["value"])
		value = iNode.getImpl()->node["value"].as<float>();
	if (iNode.getImpl()->node["backgroundColor"])
		backgroundColor = iNode.getImpl()->node["backgroundColor"].as<math::vec4>();
	if (iNode.getImpl()->node["fillColor"])
		fillColor = iNode.getImpl()->node["fillColor"].as<math::vec4>();
	utils::readBox(iNode.getImpl()->node, box);
	utils::readStyleRef(iNode.getImpl()->node, style);
}

}// namespace owl::scene::component
