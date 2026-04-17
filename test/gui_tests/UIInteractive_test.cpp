/**
 * @file UIInteractive_test.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UIInputSystem.h>
#include <scene/component/Canvas.h>
#include <scene/component/UIButton.h>
#include <scene/component/UIProgressBar.h>
#include <scene/component/UIRect.h>
#include <scene/component/UISlider.h>

using namespace owl;
using namespace owl::scene;

TEST(UIButton, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Button");
	auto& button = entity.addComponent<component::UIButton>();

	EXPECT_EQ(button.state, component::UIButton::State::Normal);
	EXPECT_TRUE(button.onClickCallback.empty());
	EXPECT_NEAR(button.normalColor.x(), 0.3f, 0.01f);

	core::Log::invalidate();
}

TEST(UIButton, getCurrentColor) {
	component::UIButton button;
	button.normalColor = {1, 0, 0, 1};
	button.hoverColor = {0, 1, 0, 1};
	button.pressedColor = {0, 0, 1, 1};
	button.disabledColor = {0.5f, 0.5f, 0.5f, 0.5f};

	button.state = component::UIButton::State::Normal;
	EXPECT_NEAR(button.getCurrentColor().x(), 1.f, 0.01f);

	button.state = component::UIButton::State::Hovered;
	EXPECT_NEAR(button.getCurrentColor().y(), 1.f, 0.01f);

	button.state = component::UIButton::State::Pressed;
	EXPECT_NEAR(button.getCurrentColor().z(), 1.f, 0.01f);

	button.state = component::UIButton::State::Disabled;
	EXPECT_NEAR(button.getCurrentColor().w(), 0.5f, 0.01f);
}

TEST(UISlider, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Slider");
	auto& slider = entity.addComponent<component::UISlider>();

	EXPECT_NEAR(slider.value, 0.f, 0.01f);
	EXPECT_NEAR(slider.minValue, 0.f, 0.01f);
	EXPECT_NEAR(slider.maxValue, 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UISlider, getNormalized) {
	component::UISlider slider;
	slider.minValue = 10.f;
	slider.maxValue = 20.f;
	slider.value = 15.f;
	EXPECT_NEAR(slider.getNormalized(), 0.5f, 0.01f);

	slider.value = 10.f;
	EXPECT_NEAR(slider.getNormalized(), 0.f, 0.01f);

	slider.value = 20.f;
	EXPECT_NEAR(slider.getNormalized(), 1.f, 0.01f);
}

TEST(UIProgressBar, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Progress");
	auto& bar = entity.addComponent<component::UIProgressBar>();

	EXPECT_NEAR(bar.value, 0.5f, 0.01f);

	core::Log::invalidate();
}

TEST(UIInputSystem, resetAndConsuming) {
	UIInputSystem::reset();
	EXPECT_FALSE(UIInputSystem::isUIConsuming());
}

TEST(UIInteractive, serializeDeserializeViaScene) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_ui_interactive_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "ui_interactive_test.owl";

	{
		auto scn = mkShared<Scene>();
		auto canvas = scn->createEntity("Canvas");
		canvas.addComponent<component::Canvas>();

		auto btnEnt = scn->createEntity("Btn");
		btnEnt.addComponent<component::UIRect>();
		auto& btn = btnEnt.addComponent<component::UIButton>();
		btn.onClickCallback = "on_start_clicked";
		btn.hoverColor = {0.8f, 0.8f, 0.8f, 1.f};
		scn->setParent(btnEnt, canvas);

		auto sliderEnt = scn->createEntity("Slider");
		sliderEnt.addComponent<component::UIRect>();
		auto& slider = sliderEnt.addComponent<component::UISlider>();
		slider.value = 0.75f;
		slider.minValue = 0.f;
		slider.maxValue = 100.f;
		scn->setParent(sliderEnt, canvas);

		auto barEnt = scn->createEntity("Bar");
		barEnt.addComponent<component::UIRect>();
		auto& bar = barEnt.addComponent<component::UIProgressBar>();
		bar.value = 0.3f;
		scn->setParent(barEnt, canvas);

		const SceneSerializer serializer(scn);
		serializer.serialize(scenePath);
	}
	{
		auto scn = mkShared<Scene>();
		const SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(scenePath));

		bool foundButton = false;
		bool foundSlider = false;
		bool foundBar = false;
		for (const auto& entity: scn->getAllEntities()) {
			if (entity.hasComponent<component::UIButton>()) {
				const auto& btn = entity.getComponent<component::UIButton>();
				EXPECT_EQ(btn.onClickCallback, "on_start_clicked");
				EXPECT_NEAR(btn.hoverColor.x(), 0.8f, 0.01f);
				EXPECT_EQ(btn.state, component::UIButton::State::Normal);
				foundButton = true;
			}
			if (entity.hasComponent<component::UISlider>()) {
				const auto& slider = entity.getComponent<component::UISlider>();
				EXPECT_NEAR(slider.value, 0.75f, 0.01f);
				EXPECT_NEAR(slider.maxValue, 100.f, 0.01f);
				foundSlider = true;
			}
			if (entity.hasComponent<component::UIProgressBar>()) {
				const auto& bar = entity.getComponent<component::UIProgressBar>();
				EXPECT_NEAR(bar.value, 0.3f, 0.01f);
				foundBar = true;
			}
		}
		EXPECT_TRUE(foundButton);
		EXPECT_TRUE(foundSlider);
		EXPECT_TRUE(foundBar);
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
