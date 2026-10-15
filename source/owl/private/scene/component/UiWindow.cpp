/**
 * @file UiWindow.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiWindow.h"

namespace owl::scene::component {

void UiWindow::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "title" << YAML::Value << title;
	emitter << YAML::Key << "titleBarHeight" << YAML::Value << titleBarHeight;
	emitter << YAML::Key << "titleFontSize" << YAML::Value << titleFontSize;
	emitter << YAML::Key << "bodyColor" << YAML::Value << bodyColor;
	emitter << YAML::Key << "titleColor" << YAML::Value << titleColor;
	emitter << YAML::Key << "titleTextColor" << YAML::Value << titleTextColor;
	emitter << YAML::Key << "draggable" << YAML::Value << draggable;
	emitter << YAML::Key << "closable" << YAML::Value << closable;
	emitter << YAML::Key << "onCloseCallback" << YAML::Value << onCloseCallback;
	emitter << YAML::EndMap;
}

void UiWindow::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	if (node["title"])
		title = node["title"].as<std::string>();
	if (node["titleBarHeight"])
		titleBarHeight = node["titleBarHeight"].as<float>();
	if (node["titleFontSize"])
		titleFontSize = node["titleFontSize"].as<float>();
	if (node["bodyColor"])
		bodyColor = node["bodyColor"].as<math::vec4>();
	if (node["titleColor"])
		titleColor = node["titleColor"].as<math::vec4>();
	if (node["titleTextColor"])
		titleTextColor = node["titleTextColor"].as<math::vec4>();
	if (node["draggable"])
		draggable = node["draggable"].as<bool>();
	if (node["closable"])
		closable = node["closable"].as<bool>();
	if (node["onCloseCallback"])
		onCloseCallback = node["onCloseCallback"].as<std::string>();
}

}// namespace owl::scene::component
