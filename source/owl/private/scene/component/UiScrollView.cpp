/**
 * @file UiScrollView.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiScrollView.h"

#include "Vec2Yaml.h"

namespace owl::scene::component {

void UiScrollView::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "vertical" << YAML::Value << vertical;
	emitter << YAML::Key << "horizontal" << YAML::Value << horizontal;
	emitVec2(emitter, "contentSize", contentSize);
	emitVec2(emitter, "scrollOffset", scrollOffset);
	emitter << YAML::Key << "wheelStep" << YAML::Value << wheelStep;
	emitter << YAML::Key << "showScrollBar" << YAML::Value << showScrollBar;
	emitter << YAML::Key << "scrollBarWidth" << YAML::Value << scrollBarWidth;
	emitter << YAML::Key << "trackColor" << YAML::Value << trackColor;
	emitter << YAML::Key << "thumbColor" << YAML::Value << thumbColor;
	emitter << YAML::EndMap;
}

void UiScrollView::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["vertical"])
		vertical = node["vertical"].as<bool>();
	if (node["horizontal"])
		horizontal = node["horizontal"].as<bool>();
	readVec2(node, "contentSize", contentSize);
	readVec2(node, "scrollOffset", scrollOffset);
	if (node["wheelStep"])
		wheelStep = node["wheelStep"].as<float>();
	if (node["showScrollBar"])
		showScrollBar = node["showScrollBar"].as<bool>();
	if (node["scrollBarWidth"])
		scrollBarWidth = node["scrollBarWidth"].as<float>();
	if (node["trackColor"])
		trackColor = node["trackColor"].as<math::vec4>();
	if (node["thumbColor"])
		thumbColor = node["thumbColor"].as<math::vec4>();
}

}// namespace owl::scene::component
