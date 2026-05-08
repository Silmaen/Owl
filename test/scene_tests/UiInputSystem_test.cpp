/**
 * @file UiInputSystem_test.cpp
 * @author Silmaen
 * @date 07/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>

using namespace owl;

namespace {

class UiInputSystemTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		scene::UiInputSystem::reset();
	}
	void TearDown() override {
		scene::UiInputSystem::reset();
		core::Log::invalidate();
	}
};

auto makeButton(scene::Scene& iScene, const scene::Entity& iCanvas, const math::vec2& iCenter, const math::vec2& iSize)
		-> scene::Entity {
	auto ent = iScene.createEntity("button");
	auto& rect = ent.addComponent<scene::component::UiRect>();
	rect.anchor = scene::component::UiRect::Anchor::BottomLeft;
	rect.pivot = {0.5f, 0.5f};
	rect.size = iSize;
	rect.anchorOffset = iCenter;// since pivot 0.5,0.5 + size cancels, position = anchor + offset
	ent.addComponent<scene::component::UiButton>();
	iScene.setParent(ent, iCanvas);
	return ent;
}

}// namespace

TEST_F(UiInputSystemTest, NullSceneDoesNothing) {
	scene::UiInputSystem::update(nullptr, {800u, 600u}, {10.f, 10.f}, false);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
}

TEST_F(UiInputSystemTest, NotPlayingDoesNothing) {
	scene::Scene scn;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	auto button = makeButton(scn, canvas, {100.f, 100.f}, {80.f, 40.f});
	// status defaults to Editing; UiInputSystem only runs in Playing.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, false);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Normal);
}

TEST_F(UiInputSystemTest, ButtonHoverPressClick) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	auto button = makeButton(scn, canvas, {100.f, 100.f}, {80.f, 40.f});

	// Hover only — no press.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, false);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Hovered);

	// Press while hovered.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, true);
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Pressed);

	// Move outside while still pressed → Normal (not consuming).
	scene::UiInputSystem::update(&scn, {800u, 600u}, {500.f, 500.f}, true);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Normal);
}

TEST_F(UiInputSystemTest, DisabledButtonIgnoresPointer) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	auto button = makeButton(scn, canvas, {100.f, 100.f}, {80.f, 40.f});
	button.getComponent<scene::component::UiButton>().state = scene::component::UiButton::State::Disabled;

	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, true);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Disabled);
}

TEST_F(UiInputSystemTest, SliderUpdatesValueWhenDragged) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	auto sliderEnt = scn.createEntity("slider");
	auto& rect = sliderEnt.addComponent<scene::component::UiRect>();
	rect.anchor = scene::component::UiRect::Anchor::BottomLeft;
	rect.pivot = {0.5f, 0.5f};
	rect.size = {200.f, 20.f};
	rect.anchorOffset = {200.f, 100.f};// center at (200, 100)
	auto& slider = sliderEnt.addComponent<scene::component::UiSlider>();
	slider.minValue = 0.f;
	slider.maxValue = 100.f;
	slider.value = 0.f;
	scn.setParent(sliderEnt, canvas);

	// Click left edge → value≈0.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, true);
	EXPECT_NEAR(sliderEnt.getComponent<scene::component::UiSlider>().value, 0.f, 0.5f);

	// Drag to right edge → value≈100.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {300.f, 100.f}, true);
	EXPECT_NEAR(sliderEnt.getComponent<scene::component::UiSlider>().value, 100.f, 0.5f);

	// Hover but not pressed: no value change but consuming.
	scene::UiInputSystem::update(&scn, {800u, 600u}, {200.f, 100.f}, false);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming());
}

TEST_F(UiInputSystemTest, HiddenChildIsSkipped) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	auto button = makeButton(scn, canvas, {100.f, 100.f}, {80.f, 40.f});
	button.getComponent<scene::component::Visibility>().gameVisible = false;

	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, true);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Normal);
}

TEST_F(UiInputSystemTest, HiddenCanvasSkipsAllChildren) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = scn.createEntity("Canvas");
	canvas.addComponent<scene::component::Canvas>();
	canvas.getComponent<scene::component::Visibility>().gameVisible = false;
	auto button = makeButton(scn, canvas, {100.f, 100.f}, {80.f, 40.f});

	scene::UiInputSystem::update(&scn, {800u, 600u}, {100.f, 100.f}, true);
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming());
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state,
			  scene::component::UiButton::State::Normal);
}
