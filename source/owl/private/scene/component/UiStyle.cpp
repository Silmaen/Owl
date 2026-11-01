/**
 * @file UiStyle.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "UiStyleYaml.h"
#include "Vec2Yaml.h"
#include "core/Macros.h"
#include "math/YamlSerializers.h"
#include "scene/component/UiStyle.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace owl::scene::component {

namespace {

constexpr std::array<std::string_view, g_uiStylePropertyCount> g_propertyNames{
		"Background", "Hover",   "Pressed", "Disabled",     "Accent",   "Secondary", "TextColor", "Font",
		"FontSize",   "Outline", "Padding", "CornerRadius", "Gradient", "Border",    "Shadow",    "NineSlice"};

const UiBox g_plainBox{};

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto UiBox::operator==(const UiBox& iOther) const -> bool = default;
OWL_DIAG_POP

auto getStylePropertyName(const UiStyleProperty iProperty) -> std::string_view {
	return g_propertyNames[static_cast<size_t>(iProperty)];
}

namespace utils {

void emitBoxFields(YAML::Emitter& ioEmitter, const UiBox& iBox) {
	if (iBox.cornerRadius > 0.f)
		ioEmitter << YAML::Key << "cornerRadius" << YAML::Value << iBox.cornerRadius;
	if (iBox.bottomTint != g_plainBox.bottomTint)
		ioEmitter << YAML::Key << "bottomTint" << YAML::Value << iBox.bottomTint;
	if (iBox.borderWidth > 0.f) {
		ioEmitter << YAML::Key << "borderColor" << YAML::Value << iBox.borderColor;
		ioEmitter << YAML::Key << "borderWidth" << YAML::Value << iBox.borderWidth;
	}
	if (iBox.shadowColor.a() > 0.f) {
		ioEmitter << YAML::Key << "shadowColor" << YAML::Value << iBox.shadowColor;
		emitVec2(ioEmitter, "shadowOffset", iBox.shadowOffset);
		ioEmitter << YAML::Key << "shadowBlur" << YAML::Value << iBox.shadowBlur;
	}
	if (iBox.texture) {
		ioEmitter << YAML::Key << "texture" << YAML::Value << iBox.texture->getSerializeString();
		ioEmitter << YAML::Key << "sliceInsets" << YAML::Value << iBox.sliceInsets;
	}
}

void emitBox(YAML::Emitter& ioEmitter, const UiBox& iBox) {
	if (iBox.isPlain())
		return;
	ioEmitter << YAML::Key << "Box" << YAML::Value << YAML::BeginMap;
	emitBoxFields(ioEmitter, iBox);
	ioEmitter << YAML::EndMap;
}

void readBoxFields(const core::YamlNode& iNode, UiBox& ioBox) {
	if (!iNode.isMap())
		return;
	ioBox.cornerRadius = iNode["cornerRadius"].as<float>(ioBox.cornerRadius);
	if (iNode["bottomTint"])
		ioBox.bottomTint = iNode["bottomTint"].as<math::vec4>();
	if (iNode["borderColor"])
		ioBox.borderColor = iNode["borderColor"].as<math::vec4>();
	ioBox.borderWidth = iNode["borderWidth"].as<float>(ioBox.borderWidth);
	if (iNode["shadowColor"])
		ioBox.shadowColor = iNode["shadowColor"].as<math::vec4>();
	readVec2(iNode, "shadowOffset", ioBox.shadowOffset);
	ioBox.shadowBlur = iNode["shadowBlur"].as<float>(ioBox.shadowBlur);
	if (iNode["texture"])
		ioBox.texture =
				renderer::gpu::Texture2D::createFromSerializedForDeserialize(iNode["texture"].as<std::string>());
	if (iNode["sliceInsets"])
		ioBox.sliceInsets = iNode["sliceInsets"].as<math::vec4>();
}

void readBox(const core::YamlNode& iNode, UiBox& ioBox) { readBoxFields(iNode["Box"], ioBox); }

void emitStyleMask(YAML::Emitter& ioEmitter, const char* iKey, const uint32_t iMask) {
	ioEmitter << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (uint8_t index = 0; index < g_uiStylePropertyCount; ++index) {
		const auto property = static_cast<UiStyleProperty>(index);
		if ((iMask & styleBit(property)) != 0)
			ioEmitter << std::string{getStylePropertyName(property)};
	}
	ioEmitter << YAML::EndSeq;
}

auto readStyleMask(const core::YamlNode& iNode) -> uint32_t {
	uint32_t mask = 0;
	if (!iNode.isSequence())
		return mask;
	for (const auto item: iNode) {
		const auto name = item.as<std::string>(std::string{});
		for (uint8_t index = 0; index < g_uiStylePropertyCount; ++index)
			if (g_propertyNames[index] == name)
				mask |= 1u << index;
	}
	return mask;
}

void emitStyleRef(YAML::Emitter& ioEmitter, const UiStyleRef& iStyle) {
	if (iStyle.name.empty() && iStyle.overrides == 0)
		return;
	ioEmitter << YAML::Key << "style" << YAML::Value << iStyle.name;
	if (iStyle.overrides != 0)
		emitStyleMask(ioEmitter, "styleOverrides", iStyle.overrides);
}

void readStyleRef(const core::YamlNode& iNode, UiStyleRef& ioStyle) {
	ioStyle.name = iNode["style"].as<std::string>(ioStyle.name);
	if (iNode["styleOverrides"])
		ioStyle.overrides = readStyleMask(iNode["styleOverrides"]);
}

}// namespace utils

}// namespace owl::scene::component
