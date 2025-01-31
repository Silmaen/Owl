/**
 * @file Text.cpp
 * @author Silmaen
 * @date 1/29/25
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

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
	if (core::Application::instanced()) {
		auto& lib = core::Application::get().getFontLibrary();
		if (iNode.getImpl()->node["font"]) {
			font = lib.getFont(iNode.getImpl()->node["font"].as<std::string>());
		} else {
			font = lib.getDefaultFont();
		}
	}
}

}// namespace owl::scene::component
