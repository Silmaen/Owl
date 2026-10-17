/**
 * @file Text.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "app/Application.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/Text.h"

namespace owl::scene::component {

void Text::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	iOut.getImpl()->emitter << YAML::Key << "kerning" << YAML::Value << kerning;
	iOut.getImpl()->emitter << YAML::Key << "lineSpacing" << YAML::Value << lineSpacing;
	iOut.getImpl()->emitter << YAML::Key << "text" << YAML::Value << text;
	if (outlineWidth > 0.f) {
		iOut.getImpl()->emitter << YAML::Key << "outlineColor" << YAML::Value << outlineColor;
		iOut.getImpl()->emitter << YAML::Key << "outlineWidth" << YAML::Value << outlineWidth;
	}
	if (backgroundColor.a() > 0.f) {
		iOut.getImpl()->emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
		iOut.getImpl()->emitter << YAML::Key << "backgroundPadding" << YAML::Value << backgroundPadding;
	}
	if (font && !font->isDefault()) {
		iOut.getImpl()->emitter << YAML::Key << "font" << YAML::Value << font->getName();
	}
	iOut.getImpl()->emitter << YAML::EndMap;
}

void Text::deserialize(const core::Serializer& iNode) {
	color = iNode.getImpl()->node["color"].as<math::vec4>();
	kerning = iNode.getImpl()->node["kerning"].as<float>();
	lineSpacing = iNode.getImpl()->node["lineSpacing"].as<float>();
	text = iNode.getImpl()->node["text"].as<std::string>();
	const auto& node = iNode.getImpl()->node;
	if (node["outlineColor"])
		outlineColor = node["outlineColor"].as<math::vec4>();
	if (node["outlineWidth"])
		outlineWidth = node["outlineWidth"].as<float>();
	if (node["backgroundColor"])
		backgroundColor = node["backgroundColor"].as<math::vec4>();
	if (node["backgroundPadding"])
		backgroundPadding = node["backgroundPadding"].as<float>();
	if (app::Application::instanced()) {
		auto& lib = app::Application::get().getFontLibrary();
		if (iNode.getImpl()->node["font"]) {
			font = lib.getFont(iNode.getImpl()->node["font"].as<std::string>());
		} else {
			font = lib.getDefaultFont();
		}
	}
}

}// namespace owl::scene::component
