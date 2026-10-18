/**
 * @file Canvas_test.cpp
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
#include <scene/component/UiRect.h>
#include <scene/component/components.h>

#include <filesystem>

using namespace owl;
using namespace owl::scene;

TEST(Canvas, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("UIRoot");
	auto& canvas = entity.addComponent<component::Canvas>();

	EXPECT_EQ(canvas.space, component::Canvas::Space::ScreenOverlay);
	EXPECT_EQ(canvas.sortOrder, 0);
	EXPECT_NEAR(canvas.referenceResolution.x(), 1280.f, 0.01f);
	EXPECT_NEAR(canvas.referenceResolution.y(), 720.f, 0.01f);
	EXPECT_EQ(canvas.scaleMode, component::Canvas::ScaleMode::ScaleWithHeight);

	core::Log::invalidate();
}

TEST(UiRect, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("UiElement");
	auto& rect = entity.addComponent<component::UiRect>();

	EXPECT_EQ(rect.getAnchor(), component::UiRect::Anchor::Center);
	EXPECT_NEAR(rect.pivot.x(), 0.5f, 0.01f);
	EXPECT_NEAR(rect.pivot.y(), 0.5f, 0.01f);
	EXPECT_NEAR(rect.size.x(), 100.f, 0.01f);
	EXPECT_NEAR(rect.size.y(), 100.f, 0.01f);
	EXPECT_NEAR(rect.anchorOffset.x(), 0.f, 0.01f);
	EXPECT_NEAR(rect.anchorOffset.y(), 0.f, 0.01f);

	core::Log::invalidate();
}

TEST(UiRect, computeBoundsCenter) {
	component::UiRect rect;
	rect.setAnchor(component::UiRect::Anchor::Center);
	rect.size = {200.f, 100.f};

	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {800.f, 600.f}});
	EXPECT_NEAR(bounds.position.x(), 300.f, 0.01f);
	EXPECT_NEAR(bounds.position.y(), 250.f, 0.01f);
	EXPECT_NEAR(bounds.getCenter().x(), 400.f, 0.01f);
	EXPECT_NEAR(bounds.getCenter().y(), 300.f, 0.01f);
}

TEST(UiRect, computeBoundsTopLeftIsYDown) {
	component::UiRect rect;
	rect.setAnchor(component::UiRect::Anchor::TopLeft);
	rect.size = {200.f, 100.f};
	rect.pivot = {0.f, 0.f};
	rect.anchorOffset = {10.f, 10.f};

	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {800.f, 600.f}});
	EXPECT_NEAR(bounds.position.x(), 10.f, 0.01f);
	EXPECT_NEAR(bounds.position.y(), 10.f, 0.01f);
	EXPECT_NEAR(bounds.size.x(), 200.f, 0.01f);
	EXPECT_NEAR(bounds.size.y(), 100.f, 0.01f);
}

TEST(UiRect, computeBoundsBottomRight) {
	component::UiRect rect;
	rect.setAnchor(component::UiRect::Anchor::BottomRight);
	rect.size = {150.f, 50.f};
	rect.pivot = {1.f, 1.f};
	rect.anchorOffset = {-20.f, -20.f};

	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {800.f, 600.f}});
	EXPECT_NEAR(bounds.position.x(), 630.f, 0.01f);
	EXPECT_NEAR(bounds.position.y(), 530.f, 0.01f);
}

TEST(Canvas, scaleModes) {
	component::Canvas canvas;
	canvas.referenceResolution = {1280.f, 720.f};
	canvas.scaleMode = component::Canvas::ScaleMode::ConstantPixels;
	EXPECT_NEAR(canvas.getScale({640.f, 360.f}), 1.f, 1e-5f);
	canvas.scaleMode = component::Canvas::ScaleMode::ScaleWithHeight;
	EXPECT_NEAR(canvas.getScale({640.f, 360.f}), 0.5f, 1e-5f);
	EXPECT_NEAR(canvas.getScale({3000.f, 1440.f}), 2.f, 1e-5f);
	canvas.scaleMode = component::Canvas::ScaleMode::MatchSmallerSide;
	EXPECT_NEAR(canvas.getScale({640.f, 720.f}), 0.5f, 1e-5f);
	EXPECT_NEAR(canvas.getScale({2560.f, 720.f}), 1.f, 1e-5f);
	canvas.referenceResolution = {0.f, 720.f};
	EXPECT_NEAR(canvas.getScale({640.f, 360.f}), 1.f, 1e-5f);
	canvas.referenceResolution = {1280.f, 720.f};
	EXPECT_NEAR(canvas.getScale({0.f, 0.f}), 1.f, 1e-5f);
}

TEST(Canvas, serializeDeserializeViaScene) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_canvas_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto scenePath = dir / "canvas_test.owl";

	{
		auto scn = mkShared<Scene>();
		auto canvasEntity = scn->createEntity("UICanvas");
		auto& canvas = canvasEntity.addComponent<component::Canvas>();
		canvas.sortOrder = 5;
		canvas.referenceResolution = {800.f, 600.f};
		canvas.scaleMode = component::Canvas::ScaleMode::MatchSmallerSide;

		auto childEntity = scn->createEntity("UIChild");
		auto& rect = childEntity.addComponent<component::UiRect>();
		rect.setAnchor(component::UiRect::Anchor::TopCenter);
		rect.size = {300.f, 80.f};
		rect.anchorOffset = {0.f, -50.f};
		scn->setParent(childEntity, canvasEntity);

		const SceneSerializer serializer(scn);
		EXPECT_TRUE(serializer.serialize(scenePath));
	}
	{
		auto scn = mkShared<Scene>();
		const SceneSerializer serializer(scn);
		ASSERT_TRUE(serializer.deserialize(scenePath));

		bool foundCanvas = false;
		bool foundRect = false;
		for (const auto& entity: scn->getAllEntities()) {
			if (entity.hasComponent<component::Canvas>()) {
				const auto& canvas = entity.getComponent<component::Canvas>();
				EXPECT_EQ(canvas.sortOrder, 5);
				EXPECT_NEAR(canvas.referenceResolution.x(), 800.f, 0.01f);
				EXPECT_NEAR(canvas.referenceResolution.y(), 600.f, 0.01f);
				EXPECT_EQ(canvas.scaleMode, component::Canvas::ScaleMode::MatchSmallerSide);
				foundCanvas = true;
			}
			if (entity.hasComponent<component::UiRect>()) {
				const auto& rect = entity.getComponent<component::UiRect>();
				EXPECT_EQ(rect.getAnchor(), component::UiRect::Anchor::TopCenter);
				EXPECT_NEAR(rect.size.x(), 300.f, 0.01f);
				EXPECT_NEAR(rect.size.y(), 80.f, 0.01f);
				EXPECT_NEAR(rect.anchorOffset.y(), -50.f, 0.01f);
				foundRect = true;
			}
		}
		EXPECT_TRUE(foundCanvas);
		EXPECT_TRUE(foundRect);
	}

	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
