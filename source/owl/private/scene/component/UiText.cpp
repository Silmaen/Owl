/**
 * @file UiText.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "app/Application.h"
#include "core/SerializerImpl.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiText.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::scene::component {

namespace {

template<typename Enum>
void readEnum(const core::YamlNode& iNode, const char* iKey, Enum& oValue) {
	if (iNode[iKey])
		oValue = magic_enum::enum_cast<Enum>(iNode[iKey].as<std::string>()).value_or(oValue);
}

template<typename Value>
void readValue(const core::YamlNode& iNode, const char* iKey, Value& oValue) {
	if (iNode[iKey])
		oValue = iNode[iKey].as<Value>();
}

}// namespace

auto UiText::getLayout(const math::vec2& iRectSize) const -> data::fonts::TextLayout {
	return {.fontSize = fontSize,
			.boxSize = iRectSize,
			.horizontal = horizontalAlignment,
			.vertical = verticalAlignment,
			.wrap = wrap,
			.fit = fit,
			.minFontSize = minFontSize,
			.kerning = kerning,
			.lineSpacing = lineSpacing};
}

void UiText::serialize(const core::Serializer& iOut) const {
	auto& emitter = iOut.getImpl()->emitter;
	emitter << YAML::Key << key();
	emitter << YAML::BeginMap;
	emitter << YAML::Key << "text" << YAML::Value << text;
	emitter << YAML::Key << "color" << YAML::Value << color;
	emitter << YAML::Key << "fontSize" << YAML::Value << fontSize;
	emitter << YAML::Key << "horizontalAlignment" << YAML::Value
			<< std::string(magic_enum::enum_name(horizontalAlignment));
	emitter << YAML::Key << "verticalAlignment" << YAML::Value << std::string(magic_enum::enum_name(verticalAlignment));
	emitter << YAML::Key << "wrap" << YAML::Value << wrap;
	emitter << YAML::Key << "fit" << YAML::Value << std::string(magic_enum::enum_name(fit));
	emitter << YAML::Key << "minFontSize" << YAML::Value << minFontSize;
	emitter << YAML::Key << "kerning" << YAML::Value << kerning;
	emitter << YAML::Key << "lineSpacing" << YAML::Value << lineSpacing;
	if (outlineWidth > 0.f) {
		emitter << YAML::Key << "outlineColor" << YAML::Value << outlineColor;
		emitter << YAML::Key << "outlineWidth" << YAML::Value << outlineWidth;
	}
	if (backgroundColor.a() > 0.f) {
		emitter << YAML::Key << "backgroundColor" << YAML::Value << backgroundColor;
		emitter << YAML::Key << "backgroundPadding" << YAML::Value << backgroundPadding;
	}
	if (font && !font->isDefault())
		emitter << YAML::Key << "font" << YAML::Value << font->getName();
	emitter << YAML::EndMap;
}

void UiText::deserialize(const core::Serializer& iNode) {
	const auto& node = iNode.getImpl()->node;
	readValue(node, "text", text);
	readValue(node, "color", color);
	readValue(node, "fontSize", fontSize);
	// A text with the unapplied `alignment` key and no `horizontalAlignment` is centred, as the scene migration does.
	if (node["alignment"] && !node["horizontalAlignment"]) {
		horizontalAlignment = data::fonts::HorizontalAlignment::Center;
		verticalAlignment = data::fonts::VerticalAlignment::Middle;
	}
	readEnum(node, "horizontalAlignment", horizontalAlignment);
	readEnum(node, "verticalAlignment", verticalAlignment);
	readValue(node, "wrap", wrap);
	readEnum(node, "fit", fit);
	readValue(node, "minFontSize", minFontSize);
	readValue(node, "kerning", kerning);
	readValue(node, "lineSpacing", lineSpacing);
	readValue(node, "outlineColor", outlineColor);
	readValue(node, "outlineWidth", outlineWidth);
	readValue(node, "backgroundColor", backgroundColor);
	readValue(node, "backgroundPadding", backgroundPadding);
	if (app::Application::instanced()) {
		auto& lib = app::Application::get().getFontLibrary();
		if (node["font"])
			font = lib.getFont(node["font"].as<std::string>());
		else
			font = lib.getDefaultFont();
	}
}

}// namespace owl::scene::component
