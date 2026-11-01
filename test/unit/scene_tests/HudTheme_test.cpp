/**
 * @file HudTheme_test.cpp
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/HudTheme.h>
#include <scene/HudThemeLibrary.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <scene/ui/HudPass.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::scene;
using component::styleBit;
using component::UiStyleProperty;

namespace {

class HudThemeTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_hud_theme_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	// A theme of one style `button` setting the four state colours and the corner radius.
	static auto buttonTheme() -> HudTheme {
		HudTheme theme;
		HudStyle style;
		style.name = "button";
		style.background = {0.8f, 0.5f, 0.2f, 1.f};
		style.hover = {0.9f, 0.6f, 0.3f, 1.f};
		style.pressed = {0.5f, 0.3f, 0.1f, 1.f};
		style.disabled = {0.3f, 0.3f, 0.3f, 0.5f};
		style.box.cornerRadius = 9.f;
		for (const auto property: {UiStyleProperty::Background, UiStyleProperty::Hover, UiStyleProperty::Pressed,
								   UiStyleProperty::Disabled, UiStyleProperty::CornerRadius})
			style.setDefined(property, true);
		theme.styles.push_back(style);
		return theme;
	}

	std::filesystem::path m_dir;
};

}// namespace

TEST_F(HudThemeTest, theDefaultThemeRoundTripsThroughItsFile) {
	const auto theme = HudTheme::makeDefault();
	const auto path = m_dir / "default.owlhudtheme";
	ASSERT_TRUE(theme.saveToFile(path));
	const auto loaded = HudTheme::load(path);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, theme);
	EXPECT_NE(loaded->findStyle("button"), nullptr);
	EXPECT_EQ(loaded->findStyleIndex("panel"), size_t{0});
	EXPECT_FALSE(loaded->findStyleIndex("missing").has_value());
}

TEST_F(HudThemeTest, aStyleWritesOnlyThePropertiesItDefines) {
	HudStyle style;
	style.name = "flat";
	style.fontSize = 30.f;
	style.setDefined(UiStyleProperty::FontSize, true);
	const auto text = HudTheme::serializeStyle(style);
	EXPECT_NE(text.find("fontSize"), std::string::npos);
	EXPECT_EQ(text.find("background"), std::string::npos);
	const auto read = HudTheme::deserializeStyle(text);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->defined, styleBit(UiStyleProperty::FontSize));
	EXPECT_FLOAT_EQ(read->fontSize, 30.f);
	EXPECT_FALSE(HudTheme::deserializeStyle("[1, 2]").has_value());
}

TEST_F(HudThemeTest, anUnversionedThemeLoadsAndANewerOneIsRefused) {
	const auto unversioned =
			HudTheme::deserializeFromString("HudTheme: Old\nStyles:\n  - name: panel\n    cornerRadius: 4\n");
	ASSERT_TRUE(unversioned.has_value());
	EXPECT_EQ(unversioned->name, "Old");
	ASSERT_EQ(unversioned->styles.size(), 1u);
	EXPECT_TRUE(unversioned->styles.front().defines(UiStyleProperty::CornerRadius));
	EXPECT_EQ(HudTheme::format().currentVersion(), 1u);
	const auto newer = HudTheme::deserializeFromString("HudTheme: Future\nFormatVersion: 99\nStyles: []\n");
	ASSERT_FALSE(newer.has_value());
	EXPECT_EQ(newer.error(), HudThemeError::Unsupported);
	const auto other = HudTheme::deserializeFromString("Scene: not a theme\n");
	ASSERT_FALSE(other.has_value());
	EXPECT_EQ(other.error(), HudThemeError::Malformed);
	EXPECT_EQ(HudTheme::load(m_dir / "missing.owlhudtheme").error(), HudThemeError::NotFound);
}

TEST_F(HudThemeTest, theStyleWinsOverTheWidgetUnlessTheWidgetOverridesIt) {
	Scene scene;
	auto styled = scene.createEntity("Styled");
	auto& button = styled.addComponent<component::UiButton>();
	button.style.name = "button";
	button.hoverColor = {0.f, 1.f, 0.f, 1.f};
	button.style.overrides = styleBit(UiStyleProperty::Hover);
	auto plain = scene.createEntity("Plain");
	auto& other = plain.addComponent<component::UiButton>();
	other.normalColor = {0.f, 0.f, 1.f, 1.f};
	auto unknown = scene.createEntity("Unknown");
	auto& lost = unknown.addComponent<component::UiButton>();
	lost.style.name = "nope";
	lost.normalColor = {1.f, 0.f, 1.f, 1.f};

	applyHudTheme(scene, buttonTheme());

	EXPECT_FLOAT_EQ(button.normalColor.x(), 0.8f);
	EXPECT_FLOAT_EQ(button.pressedColor.x(), 0.5f);
	EXPECT_FLOAT_EQ(button.box.cornerRadius, 9.f);
	// Overridden: the widget keeps its own; not defined by the style (border): the widget keeps its own.
	EXPECT_FLOAT_EQ(button.hoverColor.y(), 1.f);
	EXPECT_FLOAT_EQ(button.box.borderWidth, 0.f);
	EXPECT_FLOAT_EQ(other.normalColor.z(), 1.f);
	EXPECT_FLOAT_EQ(lost.normalColor.x(), 1.f);
	EXPECT_TRUE(other.box.isPlain());
}

TEST_F(HudThemeTest, aStyleSetsEveryKindOfWidget) {
	Scene scene;
	HudTheme theme;
	HudStyle style;
	style.name = "all";
	style.background = {0.1f, 0.2f, 0.3f, 1.f};
	style.accent = {0.4f, 0.5f, 0.6f, 1.f};
	style.secondary = {0.7f, 0.8f, 0.9f, 1.f};
	style.textColor = {1.f, 0.9f, 0.8f, 1.f};
	style.fontSize = 33.f;
	style.outlineWidth = 2.f;
	style.padding = 12.f;
	style.box.shadowColor = {0.f, 0.f, 0.f, 0.5f};
	for (uint8_t index = 0; index < component::g_uiStylePropertyCount; ++index)
		style.setDefined(static_cast<UiStyleProperty>(index), true);
	style.setDefined(UiStyleProperty::Font, false);
	theme.styles.push_back(style);
	auto entity = scene.createEntity("Everything");
	auto& panel = entity.addComponent<component::UiPanel>();
	auto& layout = entity.addComponent<component::UiLayout>();
	auto& text = entity.addComponent<component::UiText>();
	auto& slider = entity.addComponent<component::UiSlider>();
	auto& bar = entity.addComponent<component::UiProgressBar>();
	auto& list = entity.addComponent<component::UiList>();
	auto& window = entity.addComponent<component::UiWindow>();
	for (auto* ref: {&panel.style, &text.style, &slider.style, &bar.style, &list.style, &window.style})
		ref->name = "all";

	applyHudTheme(scene, theme);

	EXPECT_FLOAT_EQ(panel.backgroundColor.y(), 0.2f);
	EXPECT_FLOAT_EQ(layout.padding, 12.f);
	EXPECT_FLOAT_EQ(panel.box.shadowColor.w(), 0.5f);
	EXPECT_FLOAT_EQ(text.fontSize, 33.f);
	EXPECT_FLOAT_EQ(text.outlineWidth, 2.f);
	EXPECT_FLOAT_EQ(text.color.y(), 0.9f);
	EXPECT_FLOAT_EQ(slider.fillColor.x(), 0.4f);
	EXPECT_FLOAT_EQ(slider.handleColor.x(), 0.7f);
	EXPECT_FLOAT_EQ(bar.fillColor.y(), 0.5f);
	EXPECT_FLOAT_EQ(list.rowAltColor.y(), 0.8f);
	EXPECT_FLOAT_EQ(list.selectedColor.z(), 0.6f);
	EXPECT_FLOAT_EQ(window.titleColor.x(), 0.4f);
	EXPECT_FLOAT_EQ(window.titleFontSize, 33.f);
}

TEST_F(HudThemeTest, aSceneFollowsItsOwnThemeElseTheProjectDefault) {
	const auto path = m_dir / "demo.owlhudtheme";
	ASSERT_TRUE(buttonTheme().saveToFile(path));
	app::EngineContext context;
	Scene scene;
	scene.setEngineContext(&context);
	EXPECT_EQ(scene.getHudTheme(), nullptr);
	context.setDefaultHudTheme(path.string());
	ASSERT_NE(scene.getHudTheme(), nullptr);
	EXPECT_EQ(context.getHudThemeLibrary().getLoadCount(), 1u);
	std::ignore = scene.getHudTheme();
	EXPECT_EQ(context.getHudThemeLibrary().getLoadCount(), 1u);
	scene.setHudThemePath(std::string{});
	EXPECT_EQ(scene.getHudTheme(), nullptr);
	scene.setHudThemePath(std::string{"missing.owlhudtheme"});
	EXPECT_EQ(scene.getHudTheme(), nullptr);
	HudTheme edited = buttonTheme();
	edited.name = "Edited";
	context.getHudThemeLibrary().set(path.string(), edited);
	scene.setHudThemePath(std::nullopt);
	ASSERT_NE(scene.getHudTheme(), nullptr);
	EXPECT_EQ(scene.getHudTheme()->name, "Edited");
	context.getHudThemeLibrary().invalidate(path.string());
	EXPECT_EQ(scene.getHudTheme()->name, "Theme");
}

TEST_F(HudThemeTest, theThemeKeyOfASceneAndAHudRoundTrips) {
	for (const auto kind: {DocumentKind::Scene, DocumentKind::Hud}) {
		const auto scene = mkShared<Scene>();
		scene->setHudThemePath(std::string{"huds/demo.owlhudtheme"});
		auto entity = scene->createEntity("Button");
		auto& button = entity.addComponent<component::UiButton>();
		button.style = {.name = "button", .overrides = styleBit(UiStyleProperty::Hover)};
		button.box.cornerRadius = 7.f;
		const auto text = SceneSerializer(scene, kind).serializeToString();
		EXPECT_NE(text.find("HudTheme: huds/demo.owlhudtheme"), std::string::npos) << text;
		const auto loaded = mkShared<Scene>();
		const std::vector<uint8_t> bytes(text.begin(), text.end());
		ASSERT_TRUE(SceneSerializer(loaded, kind).deserializeFromBuffer(bytes, "theme"));
		ASSERT_TRUE(loaded->getHudThemePath().has_value());
		EXPECT_EQ(*loaded->getHudThemePath(), "huds/demo.owlhudtheme");
		const auto found = loaded->findEntityByUUID(entity.getUUID());
		ASSERT_TRUE(found);
		const auto& read = found.getComponent<component::UiButton>();
		EXPECT_EQ(read.style, button.style);
		EXPECT_FLOAT_EQ(read.box.cornerRadius, 7.f);
	}
	const auto plain = mkShared<Scene>();
	EXPECT_EQ(SceneSerializer(plain).serializeToString().find("HudTheme"), std::string::npos);
}

TEST_F(HudThemeTest, aWidgetBoxScalesItsLengthsAndFlipsTheShadow) {
	component::UiBox box;
	EXPECT_TRUE(box.isPlain());
	math::Transform transform;
	transform.scale() = {200.f, 50.f, 1.f};
	EXPECT_TRUE(ui::makeWidgetBox(transform, {1.f, 1.f, 1.f, 1.f}, box, ui::BoxPart::Whole, 2.f).plain);
	box.cornerRadius = 8.f;
	box.bottomTint = {0.5f, 0.5f, 0.5f, 1.f};
	box.borderWidth = 2.f;
	box.shadowColor = {0.f, 0.f, 0.f, 0.5f};
	box.shadowOffset = {1.f, 4.f};
	box.shadowBlur = 6.f;
	const auto whole = ui::makeWidgetBox(transform, {0.8f, 0.6f, 0.4f, 1.f}, box, ui::BoxPart::Whole, 2.f);
	ASSERT_FALSE(whole.plain);
	EXPECT_FLOAT_EQ(whole.box.size.x(), 200.f);
	EXPECT_FLOAT_EQ(whole.box.cornerRadius, 16.f);
	EXPECT_FLOAT_EQ(whole.box.borderWidth, 4.f);
	EXPECT_FLOAT_EQ(whole.box.shadowOffset.x(), 2.f);
	EXPECT_FLOAT_EQ(whole.box.shadowOffset.y(), -8.f);
	EXPECT_FLOAT_EQ(whole.box.shadowBlur, 12.f);
	EXPECT_FLOAT_EQ(whole.box.bottomColor.x(), 0.4f);
	EXPECT_FLOAT_EQ(whole.box.topColor.x(), 0.8f);
	// A fill lies inside the border: no shadow, no border, corners following the inner edge.
	const auto inner = ui::makeWidgetBox(transform, {1.f, 1.f, 1.f, 1.f}, box, ui::BoxPart::Inner, 2.f);
	ASSERT_FALSE(inner.plain);
	EXPECT_FLOAT_EQ(inner.box.size.x(), 192.f);
	EXPECT_FLOAT_EQ(inner.box.size.y(), 42.f);
	EXPECT_FLOAT_EQ(inner.box.cornerRadius, 12.f);
	EXPECT_FLOAT_EQ(inner.box.borderWidth, 0.f);
	EXPECT_FLOAT_EQ(inner.box.shadowColor.w(), 0.f);
	const auto row = ui::makeWidgetBox(transform, {1.f, 1.f, 1.f, 1.f}, box, ui::BoxPart::NoShadow, 1.f);
	EXPECT_FLOAT_EQ(row.box.shadowColor.w(), 0.f);
	EXPECT_FLOAT_EQ(row.box.borderWidth, 2.f);
}
