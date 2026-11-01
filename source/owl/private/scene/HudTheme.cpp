/**
 * @file HudTheme.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/HudTheme.h"

#include "app/Application.h"
#include "core/FormatVersionYaml.h"
#include "core/Macros.h"
#include "core/YamlNode.h"
#include "core/external/yaml.h"
#include "data/assets/AssetSearchPaths.h"
#include "math/YamlSerializers.h"
#include "platform/AtomicFile.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "scene/component/UiButton.h"
#include "scene/component/UiLayout.h"
#include "scene/component/UiList.h"
#include "scene/component/UiPanel.h"
#include "scene/component/UiProgressBar.h"
#include "scene/component/UiSlider.h"
#include "scene/component/UiText.h"
#include "scene/component/UiWindow.h"

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace owl::scene {

namespace {

using component::UiStyleProperty;

constexpr std::array<core::MigrationStep, 0> g_themeMigrations{};
constexpr core::DocumentFormat g_themeFormat{.name = "HudTheme", .migrations = g_themeMigrations};

void emitVec2(YAML::Emitter& ioOut, const char* iKey, const math::vec2& iValue) {
	ioOut << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq << iValue.x() << iValue.y()
		  << YAML::EndSeq;
}

void emitStyle(YAML::Emitter& ioOut, const HudStyle& iStyle) {
	const auto has = [&iStyle](const UiStyleProperty iProperty) -> bool { return iStyle.defines(iProperty); };
	ioOut << YAML::BeginMap;
	ioOut << YAML::Key << "name" << YAML::Value << iStyle.name;
	if (has(UiStyleProperty::Background))
		ioOut << YAML::Key << "background" << YAML::Value << iStyle.background;
	if (has(UiStyleProperty::Hover))
		ioOut << YAML::Key << "hover" << YAML::Value << iStyle.hover;
	if (has(UiStyleProperty::Pressed))
		ioOut << YAML::Key << "pressed" << YAML::Value << iStyle.pressed;
	if (has(UiStyleProperty::Disabled))
		ioOut << YAML::Key << "disabled" << YAML::Value << iStyle.disabled;
	if (has(UiStyleProperty::Accent))
		ioOut << YAML::Key << "accent" << YAML::Value << iStyle.accent;
	if (has(UiStyleProperty::Secondary))
		ioOut << YAML::Key << "secondary" << YAML::Value << iStyle.secondary;
	if (has(UiStyleProperty::TextColor))
		ioOut << YAML::Key << "textColor" << YAML::Value << iStyle.textColor;
	if (has(UiStyleProperty::Font))
		ioOut << YAML::Key << "font" << YAML::Value << iStyle.font;
	if (has(UiStyleProperty::FontSize))
		ioOut << YAML::Key << "fontSize" << YAML::Value << iStyle.fontSize;
	if (has(UiStyleProperty::Outline)) {
		ioOut << YAML::Key << "outlineColor" << YAML::Value << iStyle.outlineColor;
		ioOut << YAML::Key << "outlineWidth" << YAML::Value << iStyle.outlineWidth;
	}
	if (has(UiStyleProperty::Padding))
		ioOut << YAML::Key << "padding" << YAML::Value << iStyle.padding;
	const auto& box = iStyle.box;
	if (has(UiStyleProperty::CornerRadius))
		ioOut << YAML::Key << "cornerRadius" << YAML::Value << box.cornerRadius;
	if (has(UiStyleProperty::Gradient))
		ioOut << YAML::Key << "bottomTint" << YAML::Value << box.bottomTint;
	if (has(UiStyleProperty::Border)) {
		ioOut << YAML::Key << "borderColor" << YAML::Value << box.borderColor;
		ioOut << YAML::Key << "borderWidth" << YAML::Value << box.borderWidth;
	}
	if (has(UiStyleProperty::Shadow)) {
		ioOut << YAML::Key << "shadowColor" << YAML::Value << box.shadowColor;
		emitVec2(ioOut, "shadowOffset", box.shadowOffset);
		ioOut << YAML::Key << "shadowBlur" << YAML::Value << box.shadowBlur;
	}
	if (has(UiStyleProperty::NineSlice)) {
		ioOut << YAML::Key << "texture" << YAML::Value << (box.texture ? box.texture->getSerializeString() : "");
		ioOut << YAML::Key << "sliceInsets" << YAML::Value << box.sliceInsets;
	}
	ioOut << YAML::EndMap;
}

template<typename Value>
void readDefined(const core::YamlNode& iNode, const char* iKey, HudStyle& ioStyle, const UiStyleProperty iProperty,
				 Value& oValue) {
	if (const auto value = iNode[iKey]; value) {
		oValue = value.as<Value>();
		ioStyle.setDefined(iProperty, true);
	}
}

void readVec2(const core::YamlNode& iNode, const char* iKey, math::vec2& oValue) {
	if (const auto value = iNode[iKey]; value.isSequence() && value.size() >= 2)
		oValue = {value[0].as<float>(), value[1].as<float>()};
}

auto readStyle(const core::YamlNode& iNode) -> HudStyle {
	HudStyle style;
	style.name = iNode["name"].as<std::string>(std::string{});
	readDefined(iNode, "background", style, UiStyleProperty::Background, style.background);
	readDefined(iNode, "hover", style, UiStyleProperty::Hover, style.hover);
	readDefined(iNode, "pressed", style, UiStyleProperty::Pressed, style.pressed);
	readDefined(iNode, "disabled", style, UiStyleProperty::Disabled, style.disabled);
	readDefined(iNode, "accent", style, UiStyleProperty::Accent, style.accent);
	readDefined(iNode, "secondary", style, UiStyleProperty::Secondary, style.secondary);
	readDefined(iNode, "textColor", style, UiStyleProperty::TextColor, style.textColor);
	readDefined(iNode, "font", style, UiStyleProperty::Font, style.font);
	readDefined(iNode, "fontSize", style, UiStyleProperty::FontSize, style.fontSize);
	readDefined(iNode, "outlineColor", style, UiStyleProperty::Outline, style.outlineColor);
	readDefined(iNode, "outlineWidth", style, UiStyleProperty::Outline, style.outlineWidth);
	readDefined(iNode, "padding", style, UiStyleProperty::Padding, style.padding);
	auto& box = style.box;
	readDefined(iNode, "cornerRadius", style, UiStyleProperty::CornerRadius, box.cornerRadius);
	readDefined(iNode, "bottomTint", style, UiStyleProperty::Gradient, box.bottomTint);
	readDefined(iNode, "borderColor", style, UiStyleProperty::Border, box.borderColor);
	readDefined(iNode, "borderWidth", style, UiStyleProperty::Border, box.borderWidth);
	readDefined(iNode, "shadowColor", style, UiStyleProperty::Shadow, box.shadowColor);
	readDefined(iNode, "shadowBlur", style, UiStyleProperty::Shadow, box.shadowBlur);
	readVec2(iNode, "shadowOffset", box.shadowOffset);
	readDefined(iNode, "sliceInsets", style, UiStyleProperty::NineSlice, box.sliceInsets);
	if (const auto texture = iNode["texture"]; texture) {
		style.setDefined(UiStyleProperty::NineSlice, true);
		if (const auto reference = texture.as<std::string>(std::string{}); !reference.empty())
			box.texture = renderer::gpu::Texture2D::createFromSerializedForDeserialize(reference);
	}
	return style;
}

auto namedStyle(const std::string_view iName) -> HudStyle {
	HudStyle style;
	style.name = iName;
	return style;
}

// --- Application to the widgets --------------------------------------------

template<typename Value>
void take(const component::UiStyleRef& iRef, const HudStyle& iStyle, const UiStyleProperty iProperty, Value& oField,
		  const Value& iValue) {
	if (iRef.follows(iProperty) && iStyle.defines(iProperty))
		oField = iValue;
}

void applyBox(const component::UiStyleRef& iRef, const HudStyle& iStyle, component::UiBox& ioBox) {
	const auto& box = iStyle.box;
	take(iRef, iStyle, UiStyleProperty::CornerRadius, ioBox.cornerRadius, box.cornerRadius);
	take(iRef, iStyle, UiStyleProperty::Gradient, ioBox.bottomTint, box.bottomTint);
	take(iRef, iStyle, UiStyleProperty::Border, ioBox.borderColor, box.borderColor);
	take(iRef, iStyle, UiStyleProperty::Border, ioBox.borderWidth, box.borderWidth);
	take(iRef, iStyle, UiStyleProperty::Shadow, ioBox.shadowColor, box.shadowColor);
	take(iRef, iStyle, UiStyleProperty::Shadow, ioBox.shadowOffset, box.shadowOffset);
	take(iRef, iStyle, UiStyleProperty::Shadow, ioBox.shadowBlur, box.shadowBlur);
	take(iRef, iStyle, UiStyleProperty::NineSlice, ioBox.texture, box.texture);
	take(iRef, iStyle, UiStyleProperty::NineSlice, ioBox.sliceInsets, box.sliceInsets);
}

void apply(const HudStyle& iStyle, component::UiPanel& ioPanel, component::UiLayout* ioLayout) {
	const auto& ref = ioPanel.style;
	take(ref, iStyle, UiStyleProperty::Background, ioPanel.backgroundColor, iStyle.background);
	applyBox(ref, iStyle, ioPanel.box);
	if (ioLayout != nullptr)
		take(ref, iStyle, UiStyleProperty::Padding, ioLayout->padding, iStyle.padding);
}

void apply(const HudStyle& iStyle, component::UiButton& ioButton) {
	const auto& ref = ioButton.style;
	take(ref, iStyle, UiStyleProperty::Background, ioButton.normalColor, iStyle.background);
	take(ref, iStyle, UiStyleProperty::Hover, ioButton.hoverColor, iStyle.hover);
	take(ref, iStyle, UiStyleProperty::Pressed, ioButton.pressedColor, iStyle.pressed);
	take(ref, iStyle, UiStyleProperty::Disabled, ioButton.disabledColor, iStyle.disabled);
	applyBox(ref, iStyle, ioButton.box);
}

void apply(const HudStyle& iStyle, component::UiText& ioText) {
	const auto& ref = ioText.style;
	take(ref, iStyle, UiStyleProperty::TextColor, ioText.color, iStyle.textColor);
	take(ref, iStyle, UiStyleProperty::Font, ioText.font, iStyle.fontCache);
	take(ref, iStyle, UiStyleProperty::FontSize, ioText.fontSize, iStyle.fontSize);
	take(ref, iStyle, UiStyleProperty::Outline, ioText.outlineColor, iStyle.outlineColor);
	take(ref, iStyle, UiStyleProperty::Outline, ioText.outlineWidth, iStyle.outlineWidth);
}

void apply(const HudStyle& iStyle, component::UiSlider& ioSlider) {
	const auto& ref = ioSlider.style;
	take(ref, iStyle, UiStyleProperty::Background, ioSlider.trackColor, iStyle.background);
	take(ref, iStyle, UiStyleProperty::Accent, ioSlider.fillColor, iStyle.accent);
	take(ref, iStyle, UiStyleProperty::Secondary, ioSlider.handleColor, iStyle.secondary);
	applyBox(ref, iStyle, ioSlider.box);
}

void apply(const HudStyle& iStyle, component::UiProgressBar& ioBar) {
	const auto& ref = ioBar.style;
	take(ref, iStyle, UiStyleProperty::Background, ioBar.backgroundColor, iStyle.background);
	take(ref, iStyle, UiStyleProperty::Accent, ioBar.fillColor, iStyle.accent);
	applyBox(ref, iStyle, ioBar.box);
}

void apply(const HudStyle& iStyle, component::UiList& ioList) {
	const auto& ref = ioList.style;
	take(ref, iStyle, UiStyleProperty::Background, ioList.rowColor, iStyle.background);
	take(ref, iStyle, UiStyleProperty::Secondary, ioList.rowAltColor, iStyle.secondary);
	take(ref, iStyle, UiStyleProperty::Hover, ioList.hoverColor, iStyle.hover);
	take(ref, iStyle, UiStyleProperty::Accent, ioList.selectedColor, iStyle.accent);
	take(ref, iStyle, UiStyleProperty::TextColor, ioList.textColor, iStyle.textColor);
	take(ref, iStyle, UiStyleProperty::FontSize, ioList.fontSize, iStyle.fontSize);
	applyBox(ref, iStyle, ioList.box);
}

void apply(const HudStyle& iStyle, component::UiWindow& ioWindow) {
	const auto& ref = ioWindow.style;
	take(ref, iStyle, UiStyleProperty::Background, ioWindow.bodyColor, iStyle.background);
	take(ref, iStyle, UiStyleProperty::Accent, ioWindow.titleColor, iStyle.accent);
	take(ref, iStyle, UiStyleProperty::TextColor, ioWindow.titleTextColor, iStyle.textColor);
	take(ref, iStyle, UiStyleProperty::FontSize, ioWindow.titleFontSize, iStyle.fontSize);
	applyBox(ref, iStyle, ioWindow.box);
}

template<typename Widget>
void applyAll(Scene& ioScene, const HudTheme& iTheme) {
	for (const auto view = ioScene.registry.view<Widget>(); const auto entity: view) {
		auto& widget = view.template get<Widget>(entity);
		if (widget.style.name.empty())
			continue;
		const auto* style = iTheme.findStyle(widget.style.name);
		if (style == nullptr)
			continue;
		if constexpr (std::is_same_v<Widget, component::UiPanel>)
			apply(*style, widget, ioScene.registry.try_get<component::UiLayout>(entity));
		else
			apply(*style, widget);
	}
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto HudStyle::operator==(const HudStyle& iOther) const -> bool = default;

auto HudTheme::operator==(const HudTheme& iOther) const -> bool = default;
OWL_DIAG_POP

auto HudTheme::isTheme(const std::filesystem::path& iPath) -> bool { return iPath.extension() == fileExtension(); }

auto HudTheme::format() -> const core::DocumentFormat& { return g_themeFormat; }

auto HudTheme::findStyle(const std::string_view iName) const -> const HudStyle* {
	for (const auto& style: styles)
		if (style.name == iName)
			return &style;
	return nullptr;
}

auto HudTheme::findStyleIndex(const std::string_view iName) const -> std::optional<size_t> {
	for (size_t index = 0; index < styles.size(); ++index)
		if (styles[index].name == iName)
			return index;
	return std::nullopt;
}

void HudTheme::resolveFonts() {
	if (!app::Application::instanced())
		return;
	auto& library = app::Application::get().getFontLibrary();
	for (auto& style: styles)
		style.fontCache = style.font.empty() ? library.getDefaultFont() : library.getFont(style.font);
}

auto HudTheme::serializeToString() const -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "HudTheme" << YAML::Value << name;
	core::emitFormatVersion(out, g_themeFormat);
	out << YAML::Key << "Styles" << YAML::Value << YAML::BeginSeq;
	for (const auto& style: styles) emitStyle(out, style);
	out << YAML::EndSeq;
	out << YAML::EndMap;
	return std::string{out.c_str()};
}

auto HudTheme::serializeStyle(const HudStyle& iStyle) -> std::string {
	YAML::Emitter out;
	emitStyle(out, iStyle);
	return std::string{out.c_str()};
}

auto HudTheme::deserializeStyle(const std::string_view iYaml) -> std::optional<HudStyle> {
	try {
		const core::YamlDocument document{iYaml, "<style>"};
		if (const auto root = document.getRoot(); root.isMap())
			return readStyle(root);
	} catch (const std::exception& e) {
		OWL_CORE_WARN("HudTheme: Invalid style: {}.", e.what())
		return std::nullopt;
	}
	OWL_CORE_WARN("HudTheme: A style is a YAML map.")
	return std::nullopt;
}

auto HudTheme::deserializeFromString(const std::string_view iYaml, const std::string_view iSourceName)
		-> expected<HudTheme, HudThemeError> {
	std::string text{iYaml};
	if (!core::isBinaryYaml(text)) {
		if (const auto upgraded = core::upgradeDocumentText(g_themeFormat, text, iSourceName); !upgraded) {
			OWL_CORE_ERROR("HudTheme: Cannot read '{}': {}.", iSourceName, core::describe(upgraded.error()))
			return unexpected{upgraded.error() == core::FormatError::InvalidVersion ? HudThemeError::Malformed
																					: HudThemeError::Unsupported};
		}
	}
	try {
		const core::YamlDocument document{text, iSourceName};
		const auto root = document.getRoot();
		if (!root.isMap() || !root["HudTheme"]) {
			OWL_CORE_WARN("HudTheme: '{}' is not a HUD theme.", iSourceName)
			return unexpected{HudThemeError::Malformed};
		}
		HudTheme theme;
		theme.name = root["HudTheme"].as<std::string>(std::string{"Theme"});
		for (const auto style: root["Styles"]) theme.styles.push_back(readStyle(style));
		theme.resolveFonts();
		return theme;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("HudTheme: Cannot parse '{}': {}.", iSourceName, e.what())
		return unexpected{HudThemeError::Malformed};
	}
}

auto HudTheme::saveToFile(const std::filesystem::path& iPath) const -> bool {
	if (const auto written = platform::writeFileAtomic(iPath, serializeToString()); !written) {
		OWL_CORE_ERROR("HudTheme: Failed to write '{}': {}.", iPath.string(), describe(written.error()))
		return false;
	}
	return true;
}

auto HudTheme::load(const std::filesystem::path& iPath) -> expected<HudTheme, HudThemeError> {
	const auto text = data::assets::readAssetText(iPath);
	if (!text) {
		OWL_CORE_WARN("HudTheme: '{}' not found in the pack nor the asset folders.", iPath.string())
		return unexpected{HudThemeError::NotFound};
	}
	return deserializeFromString(*text, iPath.string());
}

auto HudTheme::makeDefault() -> HudTheme {
	using enum UiStyleProperty;
	const auto define = [](HudStyle& ioStyle, std::initializer_list<UiStyleProperty> iProperties) -> void {
		for (const auto property: iProperties) ioStyle.setDefined(property, true);
	};
	HudTheme theme;
	theme.name = "Default";
	HudStyle panel = namedStyle("panel");
	panel.background = {0.12f, 0.13f, 0.17f, 0.92f};
	panel.box.cornerRadius = 10.f;
	panel.box.bottomTint = {0.8f, 0.8f, 0.85f, 1.f};
	panel.box.shadowColor = {0.f, 0.f, 0.f, 0.45f};
	define(panel, {Background, CornerRadius, Gradient, Shadow});
	HudStyle button = namedStyle("button");
	button.background = {0.24f, 0.36f, 0.62f, 1.f};
	button.hover = {0.32f, 0.46f, 0.76f, 1.f};
	button.pressed = {0.18f, 0.27f, 0.48f, 1.f};
	button.textColor = {1.f, 1.f, 1.f, 1.f};
	button.fontSize = 24.f;
	button.box.cornerRadius = 8.f;
	button.box.bottomTint = {0.75f, 0.75f, 0.8f, 1.f};
	button.box.borderColor = {1.f, 1.f, 1.f, 0.25f};
	button.box.borderWidth = 1.f;
	button.box.shadowColor = {0.f, 0.f, 0.f, 0.35f};
	button.box.shadowOffset = {0.f, 3.f};
	button.box.shadowBlur = 5.f;
	define(button, {Background, Hover, Pressed, Disabled, TextColor, FontSize, CornerRadius, Gradient, Border, Shadow});
	HudStyle title = namedStyle("title");
	title.textColor = {1.f, 0.85f, 0.3f, 1.f};
	title.fontSize = 48.f;
	title.outlineWidth = 2.f;
	define(title, {TextColor, FontSize, Outline});
	HudStyle text = namedStyle("text");
	text.textColor = {0.9f, 0.9f, 0.92f, 1.f};
	text.fontSize = 18.f;
	define(text, {TextColor, FontSize});
	HudStyle slider = namedStyle("slider");
	slider.background = {0.1f, 0.1f, 0.12f, 1.f};
	slider.accent = {0.32f, 0.5f, 0.85f, 1.f};
	slider.secondary = {0.95f, 0.95f, 1.f, 1.f};
	slider.box.cornerRadius = 6.f;
	define(slider, {Background, Accent, Secondary, CornerRadius});
	HudStyle bar = slider;
	bar.name = "bar";
	HudStyle list = namedStyle("list");
	list.background = {0.14f, 0.15f, 0.19f, 0.95f};
	list.secondary = {0.17f, 0.18f, 0.23f, 0.95f};
	list.hover = {0.25f, 0.3f, 0.42f, 1.f};
	list.accent = {0.24f, 0.36f, 0.62f, 1.f};
	list.box.cornerRadius = 4.f;
	define(list, {Background, Secondary, Hover, Accent, CornerRadius});
	HudStyle window = namedStyle("window");
	window.background = {0.12f, 0.13f, 0.17f, 0.96f};
	window.accent = {0.24f, 0.36f, 0.62f, 1.f};
	window.box.cornerRadius = 8.f;
	window.box.shadowColor = {0.f, 0.f, 0.f, 0.45f};
	define(window, {Background, Accent, CornerRadius, Shadow});
	theme.styles = {panel, button, title, text, slider, bar, list, window};
	theme.resolveFonts();
	return theme;
}

void applyHudTheme(Scene& ioScene, const HudTheme& iTheme) {
	OWL_PROFILE_FUNCTION()

	applyAll<component::UiPanel>(ioScene, iTheme);
	applyAll<component::UiButton>(ioScene, iTheme);
	applyAll<component::UiText>(ioScene, iTheme);
	applyAll<component::UiSlider>(ioScene, iTheme);
	applyAll<component::UiProgressBar>(ioScene, iTheme);
	applyAll<component::UiList>(ioScene, iTheme);
	applyAll<component::UiWindow>(ioScene, iTheme);
}

}// namespace owl::scene
