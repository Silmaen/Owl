/**
 * @file HudUnits_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using scene::component::Canvas;
using scene::component::UiRect;
using Unit = UiRect::Unit;
using Axis = UiRect::Axis;

namespace {

class HudUnitsTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto makeCanvas(scene::Scene& ioScene, const Canvas::ScaleMode iMode) -> scene::Entity {
	auto canvas = ioScene.createEntity("Canvas");
	canvas.addComponent<Canvas>().scaleMode = iMode;
	return canvas;
}

// A top-left anchored, top-left pivoted widget, so its bounds read directly.
auto makeWidget(scene::Scene& ioScene, const scene::Entity& iParent, const math::vec2& iSize, const Unit iSizeUnitX,
				const Unit iSizeUnitY, const math::vec2& iOffset = {0.f, 0.f}, const Unit iOffsetUnitX = Unit::Pixels,
				const Unit iOffsetUnitY = Unit::Pixels) -> scene::Entity {
	auto widget = ioScene.createEntity("Widget");
	auto& rect = widget.addComponent<UiRect>();
	rect.setAnchor(UiRect::Anchor::TopLeft);
	rect.pivot = {0.f, 0.f};
	rect.size = iSize;
	rect.sizeUnit = {.x = iSizeUnitX, .y = iSizeUnitY};
	rect.anchorOffset = iOffset;
	rect.offsetUnit = {.x = iOffsetUnitX, .y = iOffsetUnitY};
	ioScene.setParent(widget, iParent);
	return widget;
}

auto boundsOf(const scene::ui::HudLayout& iLayout, const scene::Entity& iEntity) -> UiRect::Bounds {
	const auto* node = iLayout.findNode(static_cast<entt::entity>(iEntity));
	return node == nullptr ? UiRect::Bounds{.position = {-1.f, -1.f}, .size = {-1.f, -1.f}} : node->bounds;
}

void expectBounds(const UiRect::Bounds& iBounds, const math::vec2& iPosition, const math::vec2& iSize) {
	EXPECT_NEAR(iBounds.position.x(), iPosition.x(), 1e-3f);
	EXPECT_NEAR(iBounds.position.y(), iPosition.y(), 1e-3f);
	EXPECT_NEAR(iBounds.size.x(), iSize.x(), 1e-3f);
	EXPECT_NEAR(iBounds.size.y(), iSize.y(), 1e-3f);
}

}// namespace

TEST_F(HudUnitsTest, EveryUnitResolvesAgainstItsReferenceInConstantPixels) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	const auto panel = makeWidget(scn, canvas, {400.f, 300.f}, Unit::Pixels, Unit::Pixels, {100.f, 50.f});
	// Size: a half of the parent width, a tenth of the screen height; offset: a tenth of the parent height and of the
	// screen width.
	const auto kid = makeWidget(scn, panel, {0.5f, 0.1f}, Unit::ParentWidth, Unit::ScreenHeight, {0.1f, 0.1f},
								Unit::ParentHeight, Unit::ScreenWidth);
	const auto other = makeWidget(scn, panel, {0.25f, 0.5f}, Unit::ScreenWidth, Unit::ParentHeight);
	const auto third = makeWidget(scn, panel, {0.5f, 0.25f}, Unit::ParentHeight, Unit::ParentWidth);
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {800u, 600u}, false);
	expectBounds(boundsOf(layout, panel), {100.f, 50.f}, {400.f, 300.f});
	expectBounds(boundsOf(layout, kid), {100.f + 30.f, 50.f + 80.f}, {200.f, 60.f});
	expectBounds(boundsOf(layout, other), {100.f, 50.f}, {200.f, 150.f});
	expectBounds(boundsOf(layout, third), {100.f, 50.f}, {150.f, 100.f});
}

TEST_F(HudUnitsTest, ASquareWidgetUsesParentHeightForBothSizes) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	const auto square = makeWidget(scn, canvas, {0.25f, 0.25f}, Unit::ScreenHeight, Unit::ScreenHeight);
	auto& layout = scn.getHudLayout();
	for (const auto& viewport: {math::vec2ui{800u, 600u}, math::vec2ui{1600u, 600u}}) {
		layout.compute(scn, viewport, false);
		const auto bounds = boundsOf(layout, square);
		EXPECT_NEAR(bounds.size.x(), 150.f, 1e-3f);
		EXPECT_NEAR(bounds.size.y(), 150.f, 1e-3f);
	}
}

TEST_F(HudUnitsTest, RatiosAreARatioOfTheScreenInEveryScaleMode) {
	struct Case {
		Canvas::ScaleMode mode;
		math::vec2ui viewport;
	};
	for (const auto& [mode, viewport]: {Case{Canvas::ScaleMode::ConstantPixels, {800u, 600u}},
										Case{Canvas::ScaleMode::ScaleWithHeight, {1920u, 1080u}},
										Case{Canvas::ScaleMode::MatchSmallerSide, {1920u, 720u}}}) {
		scene::Scene scn;
		const auto canvas = makeCanvas(scn, mode);
		const auto widget = makeWidget(scn, canvas, {0.5f, 0.25f}, Unit::ScreenWidth, Unit::ScreenHeight, {0.1f, 0.2f},
									   Unit::ScreenWidth, Unit::ScreenHeight);
		const auto pixels = makeWidget(scn, canvas, {100.f, 100.f}, Unit::Pixels, Unit::Pixels);
		auto& layout = scn.getHudLayout();
		layout.compute(scn, viewport, false);
		const auto width = static_cast<float>(viewport.x());
		const auto height = static_cast<float>(viewport.y());
		expectBounds(boundsOf(layout, widget), {0.1f * width, 0.2f * height}, {0.5f * width, 0.25f * height});
		// A pixel size keeps following the scale mode.
		const auto* node = layout.findNode(static_cast<entt::entity>(pixels));
		ASSERT_NE(node, nullptr);
		EXPECT_NEAR(boundsOf(layout, pixels).size.x(), 100.f * node->scale, 1e-3f);
	}
}

TEST_F(HudUnitsTest, ParentRatiosFollowTheScaledParentAndNest) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ScaleWithHeight);
	// Screen 1920x1080: scale 1.5. The window of 640 reference px is 960 screen px, its child a half of that.
	const auto outer = makeWidget(scn, canvas, {640.f, 360.f}, Unit::Pixels, Unit::Pixels);
	const auto middle = makeWidget(scn, outer, {0.5f, 0.5f}, Unit::ParentWidth, Unit::ParentHeight);
	const auto inner = makeWidget(scn, middle, {0.5f, 1.f}, Unit::ParentWidth, Unit::ParentHeight, {0.25f, 0.f},
								  Unit::ParentWidth);
	auto& layout = scn.getHudLayout();
	layout.compute(scn, {1920u, 1080u}, false);
	expectBounds(boundsOf(layout, outer), {0.f, 0.f}, {960.f, 540.f});
	expectBounds(boundsOf(layout, middle), {0.f, 0.f}, {480.f, 270.f});
	expectBounds(boundsOf(layout, inner), {120.f, 0.f}, {240.f, 270.f});
}

TEST_F(HudUnitsTest, StretchedAnchorsAddARatioSizeToTheSpan) {
	UiRect rect;
	rect.anchorMin = {0.f, 0.f};
	rect.anchorMax = {1.f, 0.f};
	rect.pivot = {0.5f, 0.f};
	rect.size = {-0.1f, 0.25f};
	rect.sizeUnit = {.x = Unit::ParentWidth, .y = Unit::ScreenHeight};
	const auto bounds = rect.computeBounds({.position = {0.f, 0.f}, .size = {200.f, 100.f}}, {400.f, 400.f});
	expectBounds(bounds, {10.f, 0.f}, {180.f, 100.f});
}

TEST_F(HudUnitsTest, LayoutCellsUseTheRatioSizeOfTheirChildren) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn, Canvas::ScaleMode::ConstantPixels);
	auto row = makeWidget(scn, canvas, {400.f, 100.f}, Unit::Pixels, Unit::Pixels);
	auto& layout = row.addComponent<scene::component::UiLayout>();
	layout.arrangement = scene::component::UiLayout::Arrangement::Row;
	layout.spacing = 10.f;
	layout.padding = 0.f;
	const auto first = makeWidget(scn, row, {0.25f, 0.5f}, Unit::ParentWidth, Unit::ParentHeight);
	const auto second = makeWidget(scn, row, {50.f, 20.f}, Unit::Pixels, Unit::Pixels);
	auto& hud = scn.getHudLayout();
	hud.compute(scn, {800u, 600u}, false);
	expectBounds(boundsOf(hud, first), {0.f, 0.f}, {100.f, 50.f});
	expectBounds(boundsOf(hud, second), {110.f, 0.f}, {50.f, 20.f});
}

TEST_F(HudUnitsTest, ChangingTheUnitKeepsTheScreenRectangle) {
	const UiRect::Bounds parent{.position = {50.f, 20.f}, .size = {400.f, 200.f}};
	const UiRect::Frame frame{.parent = parent, .screen = {1280.f, 720.f}};
	UiRect rect;
	rect.setAnchor(UiRect::Anchor::TopRight);
	rect.pivot = {1.f, 0.f};
	rect.size = {160.f, 90.f};
	rect.anchorOffset = {-20.f, 30.f};
	const auto before = rect.computeBounds(parent, frame.screen);
	for (const auto unit:
		 {Unit::ParentWidth, Unit::ParentHeight, Unit::ScreenWidth, Unit::ScreenHeight, Unit::Pixels}) {
		for (const auto axis: {Axis::X, Axis::Y}) {
			rect.convertSizeUnit(axis, unit, frame);
			rect.convertOffsetUnit(axis, unit, frame);
			expectBounds(rect.computeBounds(parent, frame.screen), before.position, before.size);
		}
	}
	// Back to pixels: the original numbers.
	EXPECT_NEAR(rect.size.x(), 160.f, 1e-3f);
	EXPECT_NEAR(rect.anchorOffset.y(), 30.f, 1e-3f);
	EXPECT_TRUE(rect.sizeUnit.isPixels());
	EXPECT_TRUE(rect.offsetUnit.isPixels());
	rect.convertSizeUnit(Axis::X, Unit::ParentWidth, frame);
	EXPECT_NEAR(rect.size.x(), 0.4f, 1e-5f);
	EXPECT_EQ(rect.sizeUnit.x, Unit::ParentWidth);
	EXPECT_EQ(rect.sizeUnit.y, Unit::Pixels);
}

TEST_F(HudUnitsTest, ConvertingToARatioOfAnEmptyParentFallsBackToZero) {
	const UiRect::Frame frame{.parent = {.position = {0.f, 0.f}, .size = {0.f, 0.f}}, .screen = {1280.f, 720.f}};
	UiRect rect;
	rect.convertSizeUnit(Axis::X, Unit::ParentWidth, frame);
	EXPECT_FLOAT_EQ(rect.size.x(), 0.f);
}

TEST_F(HudUnitsTest, MovingByPixelsKeepsTheUnits) {
	const UiRect::Frame frame{.parent = {.position = {0.f, 0.f}, .size = {400.f, 200.f}}, .screen = {1280.f, 720.f}};
	UiRect rect;
	rect.anchorOffset = {0.1f, 5.f};
	rect.offsetUnit = {.x = Unit::ParentWidth, .y = Unit::Pixels};
	rect.moveBy({40.f, 3.f}, frame);
	EXPECT_NEAR(rect.anchorOffset.x(), 0.2f, 1e-5f);
	EXPECT_NEAR(rect.anchorOffset.y(), 8.f, 1e-5f);
	EXPECT_EQ(rect.offsetUnit.x, Unit::ParentWidth);
}

TEST_F(HudUnitsTest, UnitsRoundTripThroughTheScene) {
	auto scene = mkShared<scene::Scene>();
	auto entity = scene->createEntityWithUUID(42, "Widget");
	auto& rect = entity.addComponent<UiRect>();
	rect.size = {0.5f, 0.25f};
	rect.sizeUnit = {.x = Unit::ScreenWidth, .y = Unit::ParentHeight};
	rect.anchorOffset = {0.1f, -4.f};
	rect.offsetUnit = {.x = Unit::ParentWidth, .y = Unit::Pixels};
	const auto text = scene::SceneSerializer(scene).serializeToString();
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	auto loaded = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(loaded).deserializeFromBuffer(bytes, "test"));
	const auto entities = loaded->getAllEntities();
	ASSERT_EQ(entities.size(), 1u);
	const auto& back = entities.front().getComponent<UiRect>();
	EXPECT_EQ(back.sizeUnit, rect.sizeUnit);
	EXPECT_EQ(back.offsetUnit, rect.offsetUnit);
	EXPECT_NEAR(back.size.x(), 0.5f, 1e-5f);
	EXPECT_NEAR(back.anchorOffset.y(), -4.f, 1e-5f);
}

TEST_F(HudUnitsTest, PixelRectsWriteNoUnitAndALegacyFileLoadsAsPixels) {
	auto scene = mkShared<scene::Scene>();
	scene->createEntityWithUUID(7, "Widget").addComponent<UiRect>();
	const auto text = scene::SceneSerializer(scene).serializeToString();
	EXPECT_EQ(text.find("sizeUnit") + text.find("offsetUnit"), 2 * std::string::npos) << text;

	// A file written before the units: no key, so pixels, and the same rectangle.
	const std::string legacy =
			"Scene: legacy\nFormatVersion: " + std::to_string(scene::SceneSerializer::format().currentVersion()) +
			"\nEntities:\n  - Entity: 1\n    Tag: {tag: Hud}\n    Canvas: {space: ScreenOverlay, sortOrder: 0}\n"
			"  - Entity: 2\n    Tag: {tag: Bar}\n    Hierarchy: {parentId: 1}\n"
			"    UiRect: {anchorMin: [0, 0], anchorMax: [0, 0], pivot: [0, 0], size: [120, 40], offset: [10, 20]}\n";
	const std::vector<uint8_t> bytes(legacy.begin(), legacy.end());
	auto loaded = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(loaded).deserializeFromBuffer(bytes, "legacy"));
	for (const auto& entity: loaded->getAllEntities()) {
		if (!entity.hasComponent<UiRect>())
			continue;
		const auto& rect = entity.getComponent<UiRect>();
		EXPECT_TRUE(rect.sizeUnit.isPixels());
		EXPECT_TRUE(rect.offsetUnit.isPixels());
		expectBounds(rect.computeBounds({.position = {0.f, 0.f}, .size = {800.f, 600.f}}), {10.f, 20.f}, {120.f, 40.f});
	}
}

TEST_F(HudUnitsTest, AnUnknownUnitNameFallsBackToPixels) {
	const std::string text =
			"Scene: bad\nFormatVersion: " + std::to_string(scene::SceneSerializer::format().currentVersion()) +
			"\nEntities:\n  - Entity: 1\n    Tag: {tag: Hud}\n    Canvas: {space: ScreenOverlay, sortOrder: 0}\n"
			"  - Entity: 2\n    Tag: {tag: Bar}\n    Hierarchy: {parentId: 1}\n"
			"    UiRect: {size: [0.5, 40], sizeUnit: [Nonsense, ParentHeight]}\n";
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	auto loaded = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer(loaded).deserializeFromBuffer(bytes, "bad"));
	for (const auto& entity: loaded->getAllEntities()) {
		if (!entity.hasComponent<UiRect>())
			continue;
		const auto& rect = entity.getComponent<UiRect>();
		EXPECT_EQ(rect.sizeUnit.x, Unit::Pixels);
		EXPECT_EQ(rect.sizeUnit.y, Unit::ParentHeight);
	}
}
