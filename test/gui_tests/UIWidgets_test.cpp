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
#include <scene/component/UIImage.h>
#include <scene/component/UIPanel.h>
#include <scene/component/UIRect.h>
#include <scene/component/UIText.h>

using namespace owl;
using namespace owl::scene;

TEST(UIText, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("TextWidget");
	auto& text = entity.addComponent<component::UIText>();

	EXPECT_TRUE(text.text.empty());
	EXPECT_NEAR(text.fontSize, 16.f, 0.01f);
	EXPECT_EQ(text.alignment, component::UIText::Alignment::Left);
	EXPECT_NEAR(text.color.x(), 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UIImage, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("ImageWidget");
	auto& img = entity.addComponent<component::UIImage>();

	EXPECT_EQ(img.texture, nullptr);
	EXPECT_NEAR(img.tint.x(), 1.f, 0.01f);
	EXPECT_NEAR(img.tint.w(), 1.f, 0.01f);

	core::Log::invalidate();
}

TEST(UIPanel, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("PanelWidget");
	auto& panel = entity.addComponent<component::UIPanel>();

	EXPECT_NEAR(panel.backgroundColor.w(), 0.8f, 0.01f);
	EXPECT_EQ(panel.layout, component::UIPanel::Layout::None);
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
		textEnt.addComponent<component::UIRect>();
		auto& text = textEnt.addComponent<component::UIText>();
		text.text = "Hello UI";
		text.fontSize = 24.f;
		text.alignment = component::UIText::Alignment::Center;
		scn->setParent(textEnt, canvas);

		auto imgEnt = scn->createEntity("Image");
		imgEnt.addComponent<component::UIRect>();
		auto& img = imgEnt.addComponent<component::UIImage>();
		img.tint = {1.f, 0.5f, 0.f, 1.f};
		scn->setParent(imgEnt, canvas);

		auto panelEnt = scn->createEntity("Panel");
		panelEnt.addComponent<component::UIRect>();
		auto& panel = panelEnt.addComponent<component::UIPanel>();
		panel.layout = component::UIPanel::Layout::Vertical;
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
			if (entity.hasComponent<component::UIText>()) {
				const auto& text = entity.getComponent<component::UIText>();
				EXPECT_EQ(text.text, "Hello UI");
				EXPECT_NEAR(text.fontSize, 24.f, 0.01f);
				EXPECT_EQ(text.alignment, component::UIText::Alignment::Center);
				foundText = true;
			}
			if (entity.hasComponent<component::UIImage>()) {
				const auto& img = entity.getComponent<component::UIImage>();
				EXPECT_NEAR(img.tint.y(), 0.5f, 0.01f);
				foundImage = true;
			}
			if (entity.hasComponent<component::UIPanel>()) {
				const auto& panel = entity.getComponent<component::UIPanel>();
				EXPECT_EQ(panel.layout, component::UIPanel::Layout::Vertical);
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
