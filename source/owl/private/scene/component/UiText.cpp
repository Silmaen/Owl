/**
 * @file UiText.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/Application.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiText.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::scene::component {

void UiText::serialize(const core::Serializer& iOut) const {
	iOut.getImpl()->emitter << YAML::Key << key();
	iOut.getImpl()->emitter << YAML::BeginMap;
	iOut.getImpl()->emitter << YAML::Key << "text" << YAML::Value << text;
	iOut.getImpl()->emitter << YAML::Key << "color" << YAML::Value << color;
	iOut.getImpl()->emitter << YAML::Key << "fontSize" << YAML::Value << fontSize;
	iOut.getImpl()->emitter << YAML::Key << "alignment" << YAML::Value << std::string(magic_enum::enum_name(alignment));
	iOut.getImpl()->emitter << YAML::Key << "kerning" << YAML::Value << kerning;
	iOut.getImpl()->emitter << YAML::Key << "lineSpacing" << YAML::Value << lineSpacing;
	if (font && !font->isDefault())
		iOut.getImpl()->emitter << YAML::Key << "font" << YAML::Value << font->getName();
	iOut.getImpl()->emitter << YAML::EndMap;
}

void UiText::deserialize(const core::Serializer& iNode) {
	if (iNode.getImpl()->node["text"])
		text = iNode.getImpl()->node["text"].as<std::string>();
	if (iNode.getImpl()->node["color"])
		color = iNode.getImpl()->node["color"].as<math::vec4>();
	if (iNode.getImpl()->node["fontSize"])
		fontSize = iNode.getImpl()->node["fontSize"].as<float>();
	if (iNode.getImpl()->node["alignment"])
		alignment = magic_enum::enum_cast<Alignment>(iNode.getImpl()->node["alignment"].as<std::string>())
							.value_or(Alignment::Left);
	if (iNode.getImpl()->node["kerning"])
		kerning = iNode.getImpl()->node["kerning"].as<float>();
	if (iNode.getImpl()->node["lineSpacing"])
		lineSpacing = iNode.getImpl()->node["lineSpacing"].as<float>();
	if (core::Application::instanced()) {
		auto& lib = core::Application::get().getFontLibrary();
		if (iNode.getImpl()->node["font"])
			font = lib.getFont(iNode.getImpl()->node["font"].as<std::string>());
		else
			font = lib.getDefaultFont();
	}
}

}// namespace owl::scene::component
