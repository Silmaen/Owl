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

#include <magic_enum/magic_enum.hpp>

namespace owl::scene::component {

void UiPanel::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
	iOut.getImpl()->emitter << YAML::Key << "borderColor" << YAML::Value << borderColor;
	iOut.getImpl()->emitter << YAML::Key << "borderWidth" << YAML::Value << borderWidth;
	iOut.getImpl()->emitter << YAML::Key << "layout" << YAML::Value << std::string(magic_enum::enum_name(layout));
	iOut.getImpl()->emitter << YAML::Key << "spacing" << YAML::Value << spacing;
	iOut.getImpl()->emitter << YAML::Key << "padding" << YAML::Value << padding;
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiPanel::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["backgroundColor"])
		backgroundColor = iNode.getImpl()->node["backgroundColor"].as<math::vec4>();
	if (iNode.getImpl()->node["borderColor"])
		borderColor = iNode.getImpl()->node["borderColor"].as<math::vec4>();
	if (iNode.getImpl()->node["borderWidth"])
		borderWidth = iNode.getImpl()->node["borderWidth"].as<float>();
	if (iNode.getImpl()->node["layout"])
		layout = magic_enum::enum_cast<Layout>(iNode.getImpl()->node["layout"].as<std::string>())
						 .value_or(Layout::None);
	if (iNode.getImpl()->node["spacing"])
		spacing = iNode.getImpl()->node["spacing"].as<float>();
	if (iNode.getImpl()->node["padding"])
		padding = iNode.getImpl()->node["padding"].as<float>();
}

}// namespace owl::scene::component
