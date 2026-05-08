/**
 * @file EntityComponentCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/BackgroundTexture.h>
#include <scene/component/Camera.h>
#include <scene/component/Canvas.h>
#include <scene/component/CircleRenderer.h>
#include <scene/component/EntityLink.h>
#include <scene/component/LuaScript.h>
#include <scene/component/PhysicBody.h>
#include <scene/component/Player.h>
#include <scene/component/PrefabLink.h>
#include <scene/component/SoundListener.h>
#include <scene/component/SoundSource.h>
#include <scene/component/Text.h>
#include <scene/component/Trigger.h>
#include <scene/component/UiButton.h>
#include <scene/component/UiImage.h>
#include <scene/component/UiPanel.h>
#include <scene/component/UiProgressBar.h>
#include <scene/component/UiRect.h>
#include <scene/component/UiSlider.h>
#include <scene/component/UiText.h>

using namespace owl;
using namespace owl::scene;

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, Camera) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("CamEnt");

	EXPECT_FALSE(ent.hasComponent<component::Camera>());
	auto& cam = ent.addComponent<component::Camera>();
	cam.primary = false;
	cam.fixedAspectRatio = true;
	EXPECT_TRUE(ent.hasComponent<component::Camera>());
	EXPECT_FALSE(ent.getComponent<component::Camera>().primary);
	EXPECT_TRUE(ent.getComponent<component::Camera>().fixedAspectRatio);
	ent.removeComponent<component::Camera>();
	EXPECT_FALSE(ent.hasComponent<component::Camera>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// CircleRenderer
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, CircleRenderer) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("CircleEnt");

	EXPECT_FALSE(ent.hasComponent<component::CircleRenderer>());
	auto& circ = ent.addComponent<component::CircleRenderer>();
	circ.color = {0.1f, 0.2f, 0.3f, 0.4f};
	circ.thickness = 0.5f;
	circ.fade = 0.01f;
	EXPECT_TRUE(ent.hasComponent<component::CircleRenderer>());
	EXPECT_NEAR(ent.getComponent<component::CircleRenderer>().thickness, 0.5f, 0.01f);
	ent.removeComponent<component::CircleRenderer>();
	EXPECT_FALSE(ent.hasComponent<component::CircleRenderer>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, Text) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("TextEnt");

	EXPECT_FALSE(ent.hasComponent<component::Text>());
	auto& txt = ent.addComponent<component::Text>();
	txt.text = "Hello";
	txt.kerning = 1.5f;
	txt.lineSpacing = 2.0f;
	EXPECT_TRUE(ent.hasComponent<component::Text>());
	EXPECT_EQ(ent.getComponent<component::Text>().text, "Hello");
	ent.removeComponent<component::Text>();
	EXPECT_FALSE(ent.hasComponent<component::Text>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// PhysicBody
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, PhysicBody) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("PhysEnt");

	EXPECT_FALSE(ent.hasComponent<component::PhysicBody>());
	ent.addComponent<component::PhysicBody>();
	EXPECT_TRUE(ent.hasComponent<component::PhysicBody>());
	[[maybe_unused]] auto& body = ent.getComponent<component::PhysicBody>();
	ent.removeComponent<component::PhysicBody>();
	EXPECT_FALSE(ent.hasComponent<component::PhysicBody>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, Player) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("PlayerEnt");

	EXPECT_FALSE(ent.hasComponent<component::Player>());
	auto& pl = ent.addComponent<component::Player>();
	pl.primary = false;
	EXPECT_TRUE(ent.hasComponent<component::Player>());
	EXPECT_FALSE(ent.getComponent<component::Player>().primary);
	ent.removeComponent<component::Player>();
	EXPECT_FALSE(ent.hasComponent<component::Player>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Trigger
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, Trigger) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("TriggerEnt");

	EXPECT_FALSE(ent.hasComponent<component::Trigger>());
	ent.addComponent<component::Trigger>();
	EXPECT_TRUE(ent.hasComponent<component::Trigger>());
	[[maybe_unused]] auto& trig = ent.getComponent<component::Trigger>();
	ent.removeComponent<component::Trigger>();
	EXPECT_FALSE(ent.hasComponent<component::Trigger>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// EntityLink
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, EntityLink) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("LinkEnt");

	EXPECT_FALSE(ent.hasComponent<component::EntityLink>());
	auto& link = ent.addComponent<component::EntityLink>();
	link.linkedEntityName = "TargetEntity";
	EXPECT_TRUE(ent.hasComponent<component::EntityLink>());
	EXPECT_EQ(ent.getComponent<component::EntityLink>().linkedEntityName, "TargetEntity");
	ent.removeComponent<component::EntityLink>();
	EXPECT_FALSE(ent.hasComponent<component::EntityLink>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// BackgroundTexture
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, BackgroundTexture) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("BgEnt");

	EXPECT_FALSE(ent.hasComponent<component::BackgroundTexture>());
	auto& bg = ent.addComponent<component::BackgroundTexture>();
	bg.mode = component::BackgroundTexture::Mode::Skybox;
	bg.type = component::BackgroundTexture::Type::Gradient;
	bg.color = {0.1f, 0.2f, 0.3f, 1.0f};
	bg.topColor = {0.9f, 0.8f, 0.7f, 1.0f};
	EXPECT_TRUE(ent.hasComponent<component::BackgroundTexture>());
	EXPECT_EQ(ent.getComponent<component::BackgroundTexture>().mode, component::BackgroundTexture::Mode::Skybox);
	ent.removeComponent<component::BackgroundTexture>();
	EXPECT_FALSE(ent.hasComponent<component::BackgroundTexture>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// SoundSource
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, SoundSource) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("SndSrcEnt");

	EXPECT_FALSE(ent.hasComponent<component::SoundSource>());
	ent.addComponent<component::SoundSource>();
	EXPECT_TRUE(ent.hasComponent<component::SoundSource>());
	[[maybe_unused]] auto& snd = ent.getComponent<component::SoundSource>();
	ent.removeComponent<component::SoundSource>();
	EXPECT_FALSE(ent.hasComponent<component::SoundSource>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// SoundListener
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, SoundListener) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("SndListEnt");

	EXPECT_FALSE(ent.hasComponent<component::SoundListener>());
	auto& sl = ent.addComponent<component::SoundListener>();
	sl.primary = false;
	EXPECT_TRUE(ent.hasComponent<component::SoundListener>());
	EXPECT_FALSE(ent.getComponent<component::SoundListener>().primary);
	ent.removeComponent<component::SoundListener>();
	EXPECT_FALSE(ent.hasComponent<component::SoundListener>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// LuaScript
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, LuaScript) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("LuaEnt");

	EXPECT_FALSE(ent.hasComponent<component::LuaScript>());
	auto& lua = ent.addComponent<component::LuaScript>();
	lua.scriptPath = "scripts/test.lua";
	EXPECT_TRUE(ent.hasComponent<component::LuaScript>());
	EXPECT_EQ(ent.getComponent<component::LuaScript>().scriptPath, "scripts/test.lua");
	ent.removeComponent<component::LuaScript>();
	EXPECT_FALSE(ent.hasComponent<component::LuaScript>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// Canvas
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, Canvas) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("CanvasEnt");

	EXPECT_FALSE(ent.hasComponent<component::Canvas>());
	auto& cv = ent.addComponent<component::Canvas>();
	cv.sortOrder = 5;
	EXPECT_TRUE(ent.hasComponent<component::Canvas>());
	EXPECT_EQ(ent.getComponent<component::Canvas>().sortOrder, 5);
	ent.removeComponent<component::Canvas>();
	EXPECT_FALSE(ent.hasComponent<component::Canvas>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiRect
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiRect) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UIRectEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiRect>());
	auto& rect = ent.addComponent<component::UiRect>();
	rect.anchor = component::UiRect::Anchor::TopLeft;
	rect.size = {200.f, 50.f};
	rect.anchorOffset = {10.f, 20.f};
	EXPECT_TRUE(ent.hasComponent<component::UiRect>());
	EXPECT_EQ(ent.getComponent<component::UiRect>().anchor, component::UiRect::Anchor::TopLeft);
	EXPECT_NEAR(ent.getComponent<component::UiRect>().size.x(), 200.f, 0.01f);
	ent.removeComponent<component::UiRect>();
	EXPECT_FALSE(ent.hasComponent<component::UiRect>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiText
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiText) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UITextEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiText>());
	auto& ut = ent.addComponent<component::UiText>();
	ut.text = "UI Label";
	ut.fontSize = 24.f;
	ut.alignment = component::UiText::Alignment::Center;
	EXPECT_TRUE(ent.hasComponent<component::UiText>());
	EXPECT_EQ(ent.getComponent<component::UiText>().text, "UI Label");
	EXPECT_NEAR(ent.getComponent<component::UiText>().fontSize, 24.f, 0.01f);
	ent.removeComponent<component::UiText>();
	EXPECT_FALSE(ent.hasComponent<component::UiText>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiImage
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiImage) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UIImageEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiImage>());
	auto& img = ent.addComponent<component::UiImage>();
	img.tint = {0.5f, 0.5f, 0.5f, 1.0f};
	EXPECT_TRUE(ent.hasComponent<component::UiImage>());
	EXPECT_NEAR(ent.getComponent<component::UiImage>().tint.x(), 0.5f, 0.01f);
	ent.removeComponent<component::UiImage>();
	EXPECT_FALSE(ent.hasComponent<component::UiImage>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiPanel
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiPanel) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UIPanelEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiPanel>());
	auto& panel = ent.addComponent<component::UiPanel>();
	panel.layout = component::UiPanel::Layout::Vertical;
	panel.spacing = 5.f;
	panel.padding = 10.f;
	panel.borderWidth = 2.f;
	EXPECT_TRUE(ent.hasComponent<component::UiPanel>());
	EXPECT_EQ(ent.getComponent<component::UiPanel>().layout, component::UiPanel::Layout::Vertical);
	EXPECT_NEAR(ent.getComponent<component::UiPanel>().spacing, 5.f, 0.01f);
	ent.removeComponent<component::UiPanel>();
	EXPECT_FALSE(ent.hasComponent<component::UiPanel>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiButton
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiButton) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UIButtonEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiButton>());
	auto& btn = ent.addComponent<component::UiButton>();
	btn.onClickCallback = "on_click_handler";
	btn.normalColor = {0.3f, 0.3f, 0.3f, 1.f};
	EXPECT_TRUE(ent.hasComponent<component::UiButton>());
	EXPECT_EQ(ent.getComponent<component::UiButton>().onClickCallback, "on_click_handler");
	ent.removeComponent<component::UiButton>();
	EXPECT_FALSE(ent.hasComponent<component::UiButton>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiSlider
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiSlider) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UISliderEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiSlider>());
	auto& slider = ent.addComponent<component::UiSlider>();
	slider.value = 0.5f;
	slider.minValue = 0.f;
	slider.maxValue = 100.f;
	slider.onValueChangedCallback = "on_slider_change";
	EXPECT_TRUE(ent.hasComponent<component::UiSlider>());
	EXPECT_NEAR(ent.getComponent<component::UiSlider>().value, 0.5f, 0.01f);
	EXPECT_NEAR(ent.getComponent<component::UiSlider>().maxValue, 100.f, 0.01f);
	ent.removeComponent<component::UiSlider>();
	EXPECT_FALSE(ent.hasComponent<component::UiSlider>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// UiProgressBar
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, UiProgressBar) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("UIProgressEnt");

	EXPECT_FALSE(ent.hasComponent<component::UiProgressBar>());
	auto& bar = ent.addComponent<component::UiProgressBar>();
	bar.value = 0.75f;
	bar.fillColor = {0.f, 1.f, 0.f, 1.f};
	EXPECT_TRUE(ent.hasComponent<component::UiProgressBar>());
	EXPECT_NEAR(ent.getComponent<component::UiProgressBar>().value, 0.75f, 0.01f);
	ent.removeComponent<component::UiProgressBar>();
	EXPECT_FALSE(ent.hasComponent<component::UiProgressBar>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// PrefabLink
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, PrefabLink) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("PrefabEnt");

	EXPECT_FALSE(ent.hasComponent<component::PrefabLink>());
	auto& pl = ent.addComponent<component::PrefabLink>();
	pl.prefabAssetPath = "prefabs/enemy.owlprefab";
	pl.syncedVersion = 3;
	pl.uuidMapping.push_back({100, 200});
	pl.overriddenComponents.push_back("200:Transform");
	EXPECT_TRUE(ent.hasComponent<component::PrefabLink>());
	EXPECT_EQ(ent.getComponent<component::PrefabLink>().prefabAssetPath, "prefabs/enemy.owlprefab");
	EXPECT_EQ(ent.getComponent<component::PrefabLink>().syncedVersion, 3u);
	EXPECT_EQ(ent.getComponent<component::PrefabLink>().uuidMapping.size(), 1u);
	EXPECT_EQ(ent.getComponent<component::PrefabLink>().overriddenComponents.size(), 1u);
	ent.removeComponent<component::PrefabLink>();
	EXPECT_FALSE(ent.hasComponent<component::PrefabLink>());

	core::Log::invalidate();
}

// ---------------------------------------------------------------------------
// addOrReplaceComponent for less-common types
// ---------------------------------------------------------------------------

TEST(EntityComponentCoverage, addOrReplaceVariousComponents) {
	core::Log::init(core::Log::Level::Off);
	Scene sc;
	auto ent = sc.createEntity("ReplaceEnt");

	// First add, then replace to exercise addOrReplaceComponent.
	ent.addComponent<component::CircleRenderer>().color = {1.f, 0.f, 0.f, 1.f};
	ent.addOrReplaceComponent<component::CircleRenderer>().color = {0.f, 1.f, 0.f, 1.f};
	EXPECT_NEAR(ent.getComponent<component::CircleRenderer>().color.y(), 1.f, 0.01f);

	ent.addComponent<component::UiProgressBar>().value = 0.1f;
	ent.addOrReplaceComponent<component::UiProgressBar>().value = 0.9f;
	EXPECT_NEAR(ent.getComponent<component::UiProgressBar>().value, 0.9f, 0.01f);

	ent.addComponent<component::SoundListener>().primary = true;
	ent.addOrReplaceComponent<component::SoundListener>().primary = false;
	EXPECT_FALSE(ent.getComponent<component::SoundListener>().primary);

	core::Log::invalidate();
}
