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
#include <scene/UiInputSystem.h>
#include <scene/component/Canvas.h>
#include <scene/component/UiButton.h>
#include <scene/component/UiProgressBar.h>
#include <scene/component/UiRect.h>
#include <scene/component/UiSlider.h>

using namespace owl;
using namespace owl::scene;

TEST(UiButton, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Button");
	auto& button = entity.addComponent<component::UiButton>();

	EXPECT_EQ(button.state, component::UiButton::State::Normal);
	EXPECT_TRUE(button.onClickCallback.empty());
	EXPECT_NEAR(button.normalColor.x(), 0.3f, 0.01f);

	core::Log::invalidate();
}

TEST(UiButton, getCurrentColor) {
	component::UiButton button;
	button.normalColor = {1, 0, 0, 1};
	button.hoverColor = {0, 1, 0, 1};
	button.pressedColor = {0, 0, 1, 1};
	button.disabledColor = {0.5f, 0.5f, 0.5f, 0.5f};

	button.state = component::UiButton::State::Normal;
	EXPECT_NEAR(button.getCurrentColor().x(), 1.f, 0.01f);

	button.state = component::UiButton::State::Hovered;
	EXPECT_NEAR(button.getCurrentColor().y(), 1.f, 0.01f);

	button.state = component::UiButton::State::Pressed;
	EXPECT_NEAR(button.getCurrentColor().z(), 1.f, 0.01f);

	button.state = component::UiButton::State::Disabled;
	EXPECT_NEAR(button.getCurrentColor().w(), 0.5f, 0.01f);
}

TEST(UiSlider, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Slider");
	auto& slider = entity.addComponent<component::UiSlider>();

	EXPECT_NEAR(slider.value, 0.f, 0.01f);
	EXPECT_NEAR(slider.minValue, 0.f, 0.01f);
	EXPECT_NEAR(slider.maxValue, 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UiSlider, getNormalized) {
	component::UiSlider slider;
	slider.minValue = 10.f;
	slider.maxValue = 20.f;
	slider.value = 15.f;
	EXPECT_NEAR(slider.getNormalized(), 0.5f, 0.01f);

	slider.value = 10.f;
	EXPECT_NEAR(slider.getNormalized(), 0.f, 0.01f);

	slider.value = 20.f;
	EXPECT_NEAR(slider.getNormalized(), 1.f, 0.01f);
}

TEST(UiProgressBar, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("Progress");
	auto& bar = entity.addComponent<component::UiProgressBar>();

	EXPECT_NEAR(bar.value, 0.5f, 0.01f);

	core::Log::invalidate();
}

TEST(UiInputSystem, resetAndConsuming) {
	UiInputSystem::reset();
	EXPECT_FALSE(UiInputSystem::isUIConsuming());
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
		btnEnt.addComponent<component::UiRect>();
		auto& btn = btnEnt.addComponent<component::UiButton>();
		btn.onClickCallback = "on_start_clicked";
		btn.hoverColor = {0.8f, 0.8f, 0.8f, 1.f};
		scn->setParent(btnEnt, canvas);

		auto sliderEnt = scn->createEntity("Slider");
		sliderEnt.addComponent<component::UiRect>();
		auto& slider = sliderEnt.addComponent<component::UiSlider>();
		slider.value = 0.75f;
		slider.minValue = 0.f;
		slider.maxValue = 100.f;
		scn->setParent(sliderEnt, canvas);

		auto barEnt = scn->createEntity("Bar");
		barEnt.addComponent<component::UiRect>();
		auto& bar = barEnt.addComponent<component::UiProgressBar>();
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
			if (entity.hasComponent<component::UiButton>()) {
				const auto& btn = entity.getComponent<component::UiButton>();
				EXPECT_EQ(btn.onClickCallback, "on_start_clicked");
				EXPECT_NEAR(btn.hoverColor.x(), 0.8f, 0.01f);
				EXPECT_EQ(btn.state, component::UiButton::State::Normal);
				foundButton = true;
			}
			if (entity.hasComponent<component::UiSlider>()) {
				const auto& slider = entity.getComponent<component::UiSlider>();
				EXPECT_NEAR(slider.value, 0.75f, 0.01f);
				EXPECT_NEAR(slider.maxValue, 100.f, 0.01f);
				foundSlider = true;
			}
			if (entity.hasComponent<component::UiProgressBar>()) {
				const auto& bar = entity.getComponent<component::UiProgressBar>();
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
