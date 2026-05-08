/**
 * @file UIWidgets_test.cpp
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
#include <scene/component/Canvas.h>
#include <scene/component/UiImage.h>
#include <scene/component/UiPanel.h>
#include <scene/component/UiRect.h>
#include <scene/component/UiText.h>

using namespace owl;
using namespace owl::scene;

TEST(UiText, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("TextWidget");
	auto& text = entity.addComponent<component::UiText>();

	EXPECT_TRUE(text.text.empty());
	EXPECT_NEAR(text.fontSize, 16.f, 0.01f);
	EXPECT_EQ(text.alignment, component::UiText::Alignment::Left);
	EXPECT_NEAR(text.color.x(), 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UiImage, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("ImageWidget");
	auto& img = entity.addComponent<component::UiImage>();

	EXPECT_EQ(img.texture, nullptr);
	EXPECT_NEAR(img.tint.x(), 1.f, 0.01f);
	EXPECT_NEAR(img.tint.w(), 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UiPanel, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("PanelWidget");
	auto& panel = entity.addComponent<component::UiPanel>();

	EXPECT_NEAR(panel.backgroundColor.w(), 0.8f, 0.01f);
	EXPECT_EQ(panel.layout, component::UiPanel::Layout::None);
	EXPECT_NEAR(panel.borderWidth, 0.f, 0.01f);
	EXPECT_NEAR(panel.spacing, 0.f, 0.01f);
	EXPECT_NEAR(panel.padding, 0.f, 0.01f);

	core::Log::invalidate();
}

TEST(UIWidgets, serializeDeserializeViaScene) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_ui_widgets_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "ui_widgets_test.owl";

	{
		auto scn = mkShared<Scene>();
		auto canvas = scn->createEntity("Canvas");
		canvas.addComponent<component::Canvas>();

		auto textEnt = scn->createEntity("Text");
		textEnt.addComponent<component::UiRect>();
		auto& text = textEnt.addComponent<component::UiText>();
		text.text = "Hello UI";
		text.fontSize = 24.f;
		text.alignment = component::UiText::Alignment::Center;
		scn->setParent(textEnt, canvas);

		auto imgEnt = scn->createEntity("Image");
		imgEnt.addComponent<component::UiRect>();
		auto& img = imgEnt.addComponent<component::UiImage>();
		img.tint = {1.f, 0.5f, 0.f, 1.f};
		scn->setParent(imgEnt, canvas);

		auto panelEnt = scn->createEntity("Panel");
		panelEnt.addComponent<component::UiRect>();
		auto& panel = panelEnt.addComponent<component::UiPanel>();
		panel.layout = component::UiPanel::Layout::Vertical;
		panel.spacing = 10.f;
		panel.padding = 5.f;
		scn->setParent(panelEnt, canvas);

		const SceneSerializer serializer(scn);
		serializer.serialize(scenePath);
	}
	{
		auto scn = mkShared<Scene>();
		const SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(scenePath));

		bool foundText = false;
		bool foundImage = false;
		bool foundPanel = false;
		for (const auto& entity: scn->getAllEntities()) {
			if (entity.hasComponent<component::UiText>()) {
				const auto& text = entity.getComponent<component::UiText>();
				EXPECT_EQ(text.text, "Hello UI");
				EXPECT_NEAR(text.fontSize, 24.f, 0.01f);
				EXPECT_EQ(text.alignment, component::UiText::Alignment::Center);
				foundText = true;
			}
			if (entity.hasComponent<component::UiImage>()) {
				const auto& img = entity.getComponent<component::UiImage>();
				EXPECT_NEAR(img.tint.y(), 0.5f, 0.01f);
				foundImage = true;
			}
			if (entity.hasComponent<component::UiPanel>()) {
				const auto& panel = entity.getComponent<component::UiPanel>();
				EXPECT_EQ(panel.layout, component::UiPanel::Layout::Vertical);
				EXPECT_NEAR(panel.spacing, 10.f, 0.01f);
				EXPECT_NEAR(panel.padding, 5.f, 0.01f);
				foundPanel = true;
			}
		}
		EXPECT_TRUE(foundText);
		EXPECT_TRUE(foundImage);
		EXPECT_TRUE(foundPanel);
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
