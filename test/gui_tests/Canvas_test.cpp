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
#include <scene/component/UIRect.h>
#include <scene/component/components.h>

using namespace owl;
using namespace owl::scene;

TEST(Canvas, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("UIRoot");
	auto& canvas = entity.addComponent<component::Canvas>();

	EXPECT_EQ(canvas.space, component::Canvas::Space::ScreenOverlay);
	EXPECT_EQ(canvas.sortOrder, 0);

	core::Log::invalidate();
}

TEST(UIRect, createAndDefaults) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<Scene>();
	auto entity = scn->createEntity("UIElement");
	auto& rect = entity.addComponent<component::UIRect>();

	EXPECT_EQ(rect.anchor, component::UIRect::Anchor::Center);
	EXPECT_NEAR(rect.pivot.x(), 0.5f, 0.01f);
	EXPECT_NEAR(rect.pivot.y(), 0.5f, 0.01f);
	EXPECT_NEAR(rect.size.x(), 100.f, 0.01f);
	EXPECT_NEAR(rect.size.y(), 100.f, 0.01f);
	EXPECT_NEAR(rect.anchorOffset.x(), 0.f, 0.01f);
	EXPECT_NEAR(rect.anchorOffset.y(), 0.f, 0.01f);

	core::Log::invalidate();
}

TEST(UIRect, computePositionCenter) {
	component::UIRect rect;
	rect.anchor = component::UIRect::Anchor::Center;
	rect.size = {200.f, 100.f};
	rect.pivot = {0.5f, 0.5f};
	rect.anchorOffset = {0.f, 0.f};

	const math::vec2 parentSize = {800.f, 600.f};
	const auto pos = rect.computePosition(parentSize);

	// Centre of parent = (400, 300). Pivot at centre → pos = centre.
	EXPECT_NEAR(pos.x(), 400.f, 0.01f);
	EXPECT_NEAR(pos.y(), 300.f, 0.01f);
}

TEST(UIRect, computePositionTopLeft) {
	component::UIRect rect;
	rect.anchor = component::UIRect::Anchor::TopLeft;
	rect.size = {200.f, 100.f};
	rect.pivot = {0.f, 1.f};// top-left pivot
	rect.anchorOffset = {10.f, -10.f};

	const math::vec2 parentSize = {800.f, 600.f};
	const auto pos = rect.computePosition(parentSize);

	// TopLeft anchor = (0, 600). Pivot (0,1) → offset by (size.x*0.5, -size.y*0.5)
	// pos.x = 0 + 10 + 200*(0.5 - 0) = 110
	// pos.y = 600 - 10 + 100*(0.5 - 1) = 540
	EXPECT_NEAR(pos.x(), 110.f, 0.01f);
	EXPECT_NEAR(pos.y(), 540.f, 0.01f);
}

TEST(UIRect, computePositionBottomRight) {
	component::UIRect rect;
	rect.anchor = component::UIRect::Anchor::BottomRight;
	rect.size = {150.f, 50.f};
	rect.pivot = {1.f, 0.f};// bottom-right pivot
	rect.anchorOffset = {-20.f, 20.f};

	const math::vec2 parentSize = {800.f, 600.f};
	const auto pos = rect.computePosition(parentSize);

	// BottomRight anchor = (800, 0). Pivot (1,0) → offset by (size.x*(0.5-1), size.y*0.5)
	// pos.x = 800 - 20 + 150*(0.5 - 1) = 705
	// pos.y = 0 + 20 + 50*(0.5 - 0) = 45
	EXPECT_NEAR(pos.x(), 705.f, 0.01f);
	EXPECT_NEAR(pos.y(), 45.f, 0.01f);
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

		auto childEntity = scn->createEntity("UIChild");
		auto& rect = childEntity.addComponent<component::UIRect>();
		rect.anchor = component::UIRect::Anchor::TopCenter;
		rect.size = {300.f, 80.f};
		rect.anchorOffset = {0.f, -50.f};
		scn->setParent(childEntity, canvasEntity);

		const SceneSerializer serializer(scn);
		serializer.serialize(scenePath);
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
				foundCanvas = true;
			}
			if (entity.hasComponent<component::UIRect>()) {
				const auto& rect = entity.getComponent<component::UIRect>();
				EXPECT_EQ(rect.anchor, component::UIRect::Anchor::TopCenter);
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
