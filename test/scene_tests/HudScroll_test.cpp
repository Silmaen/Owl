/**
 * @file HudScroll_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <input/ActionFrame.h>
#include <input/ActionMap.h>
#include <input/InputFrame.h>
#include <input/KeyCodes.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>
#include <scene/ui/HudPass.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using scene::component::Canvas;
using scene::component::UiButton;
using scene::component::UiLayout;
using scene::component::UiList;
using scene::component::UiRect;
using scene::component::UiScrollView;

namespace {

constexpr math::vec2ui g_viewport{800u, 600u};

class HudScrollTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto makeCanvas(scene::Scene& ioScene) -> scene::Entity {
	auto canvas = ioScene.createEntity("Canvas");
	canvas.addComponent<Canvas>().scaleMode = Canvas::ScaleMode::ConstantPixels;
	return canvas;
}

auto makeWidget(scene::Scene& ioScene, const scene::Entity& iParent, const std::string& iName,
				const math::vec2& iPosition, const math::vec2& iSize) -> scene::Entity {
	auto widget = ioScene.createEntity(iName);
	auto& rect = widget.addComponent<UiRect>();
	rect.setAnchor(UiRect::Anchor::TopLeft);
	rect.pivot = {0.f, 0.f};
	rect.size = iSize;
	rect.anchorOffset = iPosition;
	ioScene.setParent(widget, iParent);
	return widget;
}

// A scroll view at (100, 100), 200 x 100, its content `iContent` high (bars are 8 wide).
auto makeView(scene::Scene& ioScene, const scene::Entity& iParent, const float iContent) -> scene::Entity {
	auto view = makeWidget(ioScene, iParent, "View", {100.f, 100.f}, {200.f, 100.f});
	view.addComponent<UiScrollView>().contentSize = {0.f, iContent};
	return view;
}

auto makeList(scene::Scene& ioScene, const scene::Entity& iParent, const uint32_t iCount) -> scene::Entity {
	auto list = makeWidget(ioScene, iParent, "List", {100.f, 100.f}, {200.f, 100.f});
	auto& data = list.addComponent<UiList>();
	data.rowHeight = 20.f;
	data.rowSpacing = 4.f;
	data.setCount(iCount);
	return list;
}

auto layoutOf(scene::Scene& ioScene) -> const scene::ui::HudLayout& {
	auto& layout = ioScene.getHudLayout();
	layout.compute(ioScene, g_viewport, false);
	return layout;
}

auto nodeOf(scene::Scene& ioScene, const scene::Entity& iEntity) -> const scene::ui::HudNode* {
	return layoutOf(ioScene).findNode(static_cast<entt::entity>(iEntity));
}

auto scrollOf(scene::Scene& ioScene, const scene::Entity& iEntity) -> const scene::ui::HudScroll* {
	const auto& layout = layoutOf(ioScene);
	const auto* node = layout.findNode(static_cast<entt::entity>(iEntity));
	if (node == nullptr || node->scroll == scene::ui::HudNode::noParent)
		return nullptr;
	return &layout.getScrolls()[node->scroll];
}

void play(scene::Scene& ioScene, const math::vec2& iPoint, const bool iPressed = false,
		  const math::vec2& iWheel = {0.f, 0.f}) {
	scene::UiInputSystem::update(&ioScene, g_viewport, iPoint, iPressed, 0.f, iWheel);
}

auto keyFrame(const input::KeyCode iKey) -> input::ActionFrame {
	input::InputFrame frame;
	frame.current.setKey(iKey, true);
	frame.previous.setKey(iKey, false);
	input::ActionFrame actions;
	actions.update(input::ActionMap::getDefault(), frame);
	return actions;
}

void tap(scene::Scene& ioScene, const input::KeyCode iKey) {
	scene::UiInputSystem::navigate(ioScene, g_viewport, keyFrame(iKey));
	scene::UiInputSystem::navigate(ioScene, g_viewport, input::ActionFrame{});
}

}// namespace

TEST_F(HudScrollTest, childrenAreShiftedByTheOffsetAndClippedToTheViewport) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	const auto child = makeWidget(scn, view, "Child", {10.f, 250.f}, {50.f, 20.f});
	const auto* scroll = scrollOf(scn, view);
	ASSERT_NE(scroll, nullptr);
	// The bar takes 8 of the 200.
	EXPECT_NEAR(scroll->geometry.viewport.size.x(), 192.f, 1e-3f);
	EXPECT_NEAR(scroll->geometry.maxOffset.y(), 300.f, 1e-3f);
	const auto* childNode = nodeOf(scn, child);
	ASSERT_NE(childNode, nullptr);
	EXPECT_NEAR(childNode->bounds.position.y(), 350.f, 1e-3f);
	EXPECT_TRUE(childNode->clipped);
	EXPECT_NEAR(childNode->clip.size.x(), 192.f, 1e-3f);
	EXPECT_NEAR(childNode->clip.size.y(), 100.f, 1e-3f);
	view.getComponent<UiScrollView>().scrollOffset = {0.f, 250.f};
	childNode = nodeOf(scn, child);
	EXPECT_NEAR(childNode->bounds.position.x(), 110.f, 1e-3f);
	EXPECT_NEAR(childNode->bounds.position.y(), 100.f, 1e-3f);
}

TEST_F(HudScrollTest, theOffsetIsKeptInsideTheContentAndWrittenBack) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	view.getComponent<UiScrollView>().scrollOffset = {80.f, 900.f};
	ASSERT_NE(scrollOf(scn, view), nullptr);
	// Vertical only: the horizontal offset goes, the vertical one stops at the end.
	EXPECT_NEAR(view.getComponent<UiScrollView>().scrollOffset.x(), 0.f, 1e-3f);
	EXPECT_NEAR(view.getComponent<UiScrollView>().scrollOffset.y(), 300.f, 1e-3f);
	view.getComponent<UiScrollView>().scrollOffset = {0.f, -20.f};
	ASSERT_NE(scrollOf(scn, view), nullptr);
	EXPECT_NEAR(view.getComponent<UiScrollView>().scrollOffset.y(), 0.f, 1e-3f);
}

TEST_F(HudScrollTest, aContentThatFitsHasNoBarAndNoRoomTaken) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 60.f);
	const auto* scroll = scrollOf(scn, view);
	ASSERT_NE(scroll, nullptr);
	EXPECT_FALSE(scroll->geometry.verticalBar);
	EXPECT_NEAR(scroll->geometry.viewport.size.x(), 200.f, 1e-3f);
	view.getComponent<UiScrollView>().showScrollBar = false;
	view.getComponent<UiScrollView>().contentSize = {0.f, 400.f};
	scroll = scrollOf(scn, view);
	EXPECT_FALSE(scroll->geometry.verticalBar);
	EXPECT_NEAR(scroll->geometry.viewport.size.x(), 200.f, 1e-3f);
	EXPECT_NEAR(scroll->geometry.maxOffset.y(), 300.f, 1e-3f);
}

TEST_F(HudScrollTest, theContentTakesTheExtentOfTheLayoutChildren) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeWidget(scn, canvas, "View", {100.f, 100.f}, {200.f, 100.f});
	view.addComponent<UiScrollView>();
	view.addComponent<UiLayout>();
	for (int i = 0; i < 10; ++i) makeWidget(scn, view, std::format("Row{}", i), {0.f, 0.f}, {100.f, 20.f});
	const auto* scroll = scrollOf(scn, view);
	ASSERT_NE(scroll, nullptr);
	// Padding 4, ten rows of 20 and nine gaps of 4, padding 4.
	EXPECT_NEAR(scroll->geometry.content.y(), 244.f, 1e-3f);
	EXPECT_NEAR(scroll->geometry.maxOffset.y(), 144.f, 1e-3f);
}

TEST_F(HudScrollTest, aHorizontalViewScrollsSideways) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 0.f);
	auto& data = view.getComponent<UiScrollView>();
	data.vertical = false;
	data.horizontal = true;
	data.contentSize = {500.f, 40.f};
	data.scrollOffset = {120.f, 0.f};
	const auto* scroll = scrollOf(scn, view);
	ASSERT_NE(scroll, nullptr);
	EXPECT_TRUE(scroll->geometry.horizontalBar);
	EXPECT_NEAR(scroll->geometry.viewport.size.y(), 92.f, 1e-3f);
	EXPECT_NEAR(scroll->geometry.maxOffset.x(), 300.f, 1e-3f);
	EXPECT_NEAR(data.scrollOffset.x(), 120.f, 1e-3f);
}

TEST_F(HudScrollTest, aWidgetScrolledOutIsNotHitButTheViewIs) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	const auto child = makeWidget(scn, view, "Child", {10.f, 250.f}, {50.f, 20.f});
	const auto& layout = layoutOf(scn);
	// Where the child sits in the unscrolled content, 150 below the view: nothing but the world.
	EXPECT_EQ(layout.hitTest({120.f, 360.f}), nullptr);
	EXPECT_EQ(layout.hitTest({120.f, 150.f})->entity, static_cast<entt::entity>(view));
	view.getComponent<UiScrollView>().scrollOffset = {0.f, 250.f};
	const auto& scrolled = layoutOf(scn);
	ASSERT_NE(scrolled.hitTest({120.f, 110.f}), nullptr);
	EXPECT_EQ(scrolled.hitTest({120.f, 110.f})->entity, static_cast<entt::entity>(child));
	// Half out of the view: only the visible half answers.
	view.getComponent<UiScrollView>().scrollOffset = {0.f, 160.f};
	const auto& partial = layoutOf(scn);
	EXPECT_EQ(partial.hitTest({120.f, 195.f})->entity, static_cast<entt::entity>(child));
	EXPECT_EQ(partial.hitTest({120.f, 205.f}), nullptr);
}

TEST_F(HudScrollTest, aButtonScrolledOutDoesNotClick) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	auto child = makeWidget(scn, view, "Child", {10.f, 250.f}, {50.f, 20.f});
	child.addComponent<UiButton>();
	play(scn, {120.f, 360.f});
	EXPECT_EQ(child.getComponent<UiButton>().state, UiButton::State::Normal);
	view.getComponent<UiScrollView>().scrollOffset = {0.f, 250.f};
	play(scn, {120.f, 110.f});
	EXPECT_EQ(child.getComponent<UiButton>().state, UiButton::State::Hovered);
}

TEST_F(HudScrollTest, theWheelScrollsTheViewUnderTheMouse) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	auto child = makeWidget(scn, view, "Child", {10.f, 10.f}, {50.f, 20.f});
	child.addComponent<UiButton>();
	const auto offset = [&view]() -> float { return view.getComponent<UiScrollView>().scrollOffset.y(); };
	// A notch down over the view, and over a button inside it: the wheel bubbles up to the view.
	play(scn, {200.f, 150.f}, false, {0.f, -1.f});
	EXPECT_NEAR(offset(), 48.f, 1e-3f);
	play(scn, {120.f, 115.f}, false, {0.f, -2.f});
	EXPECT_NEAR(offset(), 144.f, 1e-3f);
	// Outside the view nothing moves.
	play(scn, {500.f, 400.f}, false, {0.f, -1.f});
	EXPECT_NEAR(offset(), 144.f, 1e-3f);
	// It stops at both ends.
	play(scn, {200.f, 150.f}, false, {0.f, -100.f});
	EXPECT_NEAR(offset(), 300.f, 1e-3f);
	play(scn, {200.f, 150.f}, false, {0.f, 100.f});
	EXPECT_NEAR(offset(), 0.f, 1e-3f);
}

TEST_F(HudScrollTest, theWheelScrollsSidewaysWhenTheViewOnlyDoes) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 0.f);
	auto& data = view.getComponent<UiScrollView>();
	data.vertical = false;
	data.horizontal = true;
	data.contentSize = {500.f, 40.f};
	play(scn, {200.f, 150.f}, false, {0.f, -1.f});
	EXPECT_NEAR(data.scrollOffset.x(), 48.f, 1e-3f);
	play(scn, {200.f, 150.f}, false, {-1.f, 0.f});
	EXPECT_NEAR(data.scrollOffset.x(), 96.f, 1e-3f);
}

TEST_F(HudScrollTest, theThumbIsDraggedAndTheTrackJumps) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	const auto offset = [&view]() -> float { return view.getComponent<UiScrollView>().scrollOffset.y(); };
	// The thumb is 25 long at the top of the track (x 292 to 300, y 100 to 200).
	play(scn, {296.f, 110.f});
	play(scn, {296.f, 110.f}, true);
	EXPECT_NEAR(offset(), 0.f, 1e-3f);
	EXPECT_NE(scn.getUiInputState().dragScroll, entt::null);
	play(scn, {296.f, 160.f}, true);
	// The thumb moved 50 of the 75 it can: two thirds of 300.
	EXPECT_NEAR(offset(), 200.f, 1e-3f);
	// The mouse leaves the track: the thumb still follows, and stops at the end.
	play(scn, {50.f, 500.f}, true);
	EXPECT_NEAR(offset(), 300.f, 1e-3f);
	play(scn, {50.f, 500.f}, false);
	EXPECT_EQ(scn.getUiInputState().dragScroll, entt::null);
	// A click on the track below the thumb brings its centre there; click above goes back.
	view.getComponent<UiScrollView>().scrollOffset = {0.f, 0.f};
	play(scn, {50.f, 500.f});
	play(scn, {296.f, 150.f}, true);
	EXPECT_NEAR(offset(), 150.f, 1e-3f);
	play(scn, {296.f, 150.f}, false);
}

TEST_F(HudScrollTest, aBarOverAWidgetBelowTheViewTakesTheClick) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto below = makeWidget(scn, canvas, "Below", {100.f, 100.f}, {300.f, 100.f});
	below.addComponent<UiButton>();
	const auto view = makeView(scn, canvas, 400.f);
	play(scn, {296.f, 150.f});
	play(scn, {296.f, 150.f}, true);
	EXPECT_NE(scn.getUiInputState().dragScroll, entt::null);
	EXPECT_EQ(below.getComponent<UiButton>().state, UiButton::State::Normal);
	EXPECT_GT(view.getComponent<UiScrollView>().scrollOffset.y(), 0.f);
}

namespace {

// A view of ten buttons placed by a column layout (they are 20 high, 24 apart).
struct ButtonColumn {
	scene::Scene scn;
	scene::Entity canvas;
	scene::Entity view;
	std::vector<scene::Entity> buttons;

	ButtonColumn() {
		scn.status = scene::Scene::Status::Playing;
		canvas = makeCanvas(scn);
		view = makeWidget(scn, canvas, "View", {100.f, 100.f}, {200.f, 100.f});
		view.addComponent<UiScrollView>();
		view.addComponent<UiLayout>();
		for (int i = 0; i < 10; ++i) {
			buttons.push_back(makeWidget(scn, view, std::format("B{}", i), {0.f, 0.f}, {100.f, 20.f}));
			buttons.back().addComponent<UiButton>();
		}
	}

	[[nodiscard]] auto offset() const -> float { return view.getComponent<UiScrollView>().scrollOffset.y(); }

	[[nodiscard]] auto focusName() -> std::string {
		const auto focus = scene::UiInputSystem::getFocus(scn);
		return focus ? focus.getName() : std::string{};
	}
};

}// namespace

TEST_F(HudScrollTest, theFocusReachesAndScrollsIntoViewAWidgetOutOfView) {
	ButtonColumn column;
	ASSERT_TRUE(scene::UiInputSystem::setFocus(column.scn, column.buttons.front()));
	for (int i = 0; i < 5; ++i) tap(column.scn, input::key::Down);
	EXPECT_EQ(column.focusName(), "B5");
	// B5 spans 124 to 144 in the content: the view moves just enough to show its bottom.
	EXPECT_NEAR(column.offset(), 44.f, 1e-3f);
	const auto* node = nodeOf(column.scn, column.buttons[5]);
	ASSERT_NE(node, nullptr);
	EXPECT_LE(node->bounds.position.y() + node->bounds.size.y(), 200.f + 1e-3f);
	for (int i = 0; i < 4; ++i) tap(column.scn, input::key::Down);
	EXPECT_EQ(column.focusName(), "B9");
	// B9 spans 220 to 240 of the 244.
	EXPECT_NEAR(column.offset(), 140.f, 1e-3f);
	for (int i = 0; i < 9; ++i) tap(column.scn, input::key::Up);
	EXPECT_EQ(column.focusName(), "B0");
	EXPECT_NEAR(column.offset(), 4.f, 1e-3f);
}

TEST_F(HudScrollTest, theMouseDoesNotScrollToTheWidgetItFocuses) {
	ButtonColumn column;
	play(column.scn, {600.f, 500.f});
	play(column.scn, {120.f, 190.f});
	EXPECT_EQ(column.focusName(), "B3");
	EXPECT_NEAR(column.offset(), 0.f, 1e-3f);
}

TEST_F(HudScrollTest, theScrollActionMovesTheViewHoldingTheFocus) {
	ButtonColumn column;
	ASSERT_TRUE(scene::UiInputSystem::setFocus(column.scn, column.buttons.front()));
	scene::UiInputSystem::navigate(column.scn, g_viewport, input::ActionFrame{});
	scene::UiInputSystem::navigate(column.scn, g_viewport, keyFrame(input::key::PageDown), 0.25f);
	// 480 reference pixels per second.
	EXPECT_NEAR(column.offset(), 120.f, 1e-3f);
	scene::UiInputSystem::navigate(column.scn, g_viewport, keyFrame(input::key::PageDown), 0.25f);
	EXPECT_NEAR(column.offset(), 144.f, 1e-3f);
	scene::UiInputSystem::navigate(column.scn, g_viewport, keyFrame(input::key::PageUp), 0.1f);
	EXPECT_NEAR(column.offset(), 96.f, 1e-3f);
}

TEST_F(HudScrollTest, aWidgetCutByAPlainClippingPanelStaysUnreachable) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto panel = makeWidget(scn, canvas, "Panel", {100.f, 100.f}, {200.f, 100.f});
	panel.addComponent<scene::component::UiPanel>().clipChildren = true;
	makeWidget(scn, panel, "Hidden", {0.f, 300.f}, {100.f, 20.f}).addComponent<UiButton>();
	makeWidget(scn, canvas, "Free", {500.f, 300.f}, {100.f, 20.f}).addComponent<UiButton>();
	tap(scn, input::key::Down);
	EXPECT_EQ(scene::UiInputSystem::getFocus(scn).getName(), "Free");
	tap(scn, input::key::Up);
	EXPECT_EQ(scene::UiInputSystem::getFocus(scn).getName(), "Free");
}

TEST_F(HudScrollTest, scrollViewsAndListsRoundTripThroughYaml) {
	const auto original = mkShared<scene::Scene>();
	const auto canvas = makeCanvas(*original);
	auto view = makeView(*original, canvas, 400.f);
	auto& data = view.getComponent<UiScrollView>();
	data.horizontal = true;
	data.scrollOffset = {0.f, 33.f};
	data.wheelStep = 12.f;
	data.showScrollBar = false;
	data.scrollBarWidth = 5.f;
	data.thumbColor = {0.1f, 0.2f, 0.3f, 0.4f};
	auto list = makeList(*original, canvas, 40);
	auto& rows = list.getComponent<UiList>();
	rows.selectedIndex = 7;
	rows.scrollOffset = 48.f;
	rows.fontSize = 11.f;
	rows.selectedColor = {0.9f, 0.8f, 0.7f, 1.f};
	rows.onSelectCallback = "on_pick";
	rows.onBindCallback = "on_bind";
	const auto copy = scene::Scene::copy(original);
	const auto viewCopy = copy->findEntityByUUID(view.getUUID());
	ASSERT_TRUE(viewCopy);
	const auto& viewData = viewCopy.getComponent<UiScrollView>();
	EXPECT_TRUE(viewData.horizontal);
	EXPECT_FALSE(viewData.showScrollBar);
	EXPECT_FLOAT_EQ(viewData.contentSize.y(), 400.f);
	EXPECT_FLOAT_EQ(viewData.scrollOffset.y(), 33.f);
	EXPECT_FLOAT_EQ(viewData.wheelStep, 12.f);
	EXPECT_FLOAT_EQ(viewData.scrollBarWidth, 5.f);
	EXPECT_FLOAT_EQ(viewData.thumbColor.z(), 0.3f);
	const auto listCopy = copy->findEntityByUUID(list.getUUID());
	ASSERT_TRUE(listCopy);
	const auto& listData = listCopy.getComponent<UiList>();
	EXPECT_EQ(listData.itemCount, 40u);
	EXPECT_EQ(listData.selectedIndex, 7);
	EXPECT_FLOAT_EQ(listData.scrollOffset, 48.f);
	EXPECT_FLOAT_EQ(listData.rowHeight, 20.f);
	EXPECT_FLOAT_EQ(listData.rowSpacing, 4.f);
	EXPECT_FLOAT_EQ(listData.fontSize, 11.f);
	EXPECT_FLOAT_EQ(listData.selectedColor.y(), 0.8f);
	EXPECT_EQ(listData.onSelectCallback, "on_pick");
	EXPECT_EQ(listData.onBindCallback, "on_bind");
}

TEST_F(HudScrollTest, aListGeometryCoversEveryRowButOnlyTheVisibleOnesAreListed) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	const auto list = makeList(scn, canvas, 1000);
	const auto* scroll = scrollOf(scn, list);
	ASSERT_NE(scroll, nullptr);
	EXPECT_TRUE(scroll->isList);
	EXPECT_EQ(scroll->rowCount, 1000u);
	EXPECT_NEAR(scroll->geometry.content.y(), 23996.f, 1e-2f);
	EXPECT_NEAR(scroll->geometry.maxOffset.y(), 23896.f, 1e-2f);
	const auto range = scene::ui::visibleRows(scroll->offset.y(), scroll->geometry.viewport.size.y(), scroll->rowHeight,
											  scroll->rowSpacing, scroll->rowCount);
	EXPECT_EQ(range.first, 0u);
	EXPECT_EQ(range.size(), 5u);
}

TEST_F(HudScrollTest, aClickSelectsTheRowUnderTheMouse) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 100);
	auto& data = list.getComponent<UiList>();
	// Row 2 spans 148 to 168; the gap to row 3 is 168 to 172.
	play(scn, {150.f, 160.f});
	EXPECT_EQ(data.hoverIndex, 2);
	play(scn, {150.f, 160.f}, true);
	EXPECT_EQ(data.selectedIndex, 2);
	play(scn, {150.f, 160.f}, false);
	play(scn, {150.f, 170.f});
	EXPECT_EQ(data.hoverIndex, -1);
	play(scn, {150.f, 170.f}, true);
	EXPECT_EQ(data.selectedIndex, 2);
	play(scn, {150.f, 170.f}, false);
	// Scrolled by 30, the row under y = 110 is the one holding 40 in the content: row 1 (24 to 44).
	data.scrollOffset = 30.f;
	play(scn, {150.f, 110.f});
	EXPECT_EQ(data.hoverIndex, 1);
	play(scn, {150.f, 110.f}, true);
	EXPECT_EQ(data.selectedIndex, 1);
	play(scn, {50.f, 50.f}, false);
	EXPECT_EQ(data.hoverIndex, -1);
}

TEST_F(HudScrollTest, aClickOnTheBarOfAListDoesNotSelect) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 100);
	play(scn, {296.f, 110.f});
	play(scn, {296.f, 110.f}, true);
	EXPECT_EQ(list.getComponent<UiList>().selectedIndex, -1);
	EXPECT_NE(scn.getUiInputState().dragScroll, entt::null);
}

TEST_F(HudScrollTest, theWheelScrollsAList) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 100);
	play(scn, {150.f, 150.f}, false, {0.f, -1.f});
	EXPECT_NEAR(list.getComponent<UiList>().scrollOffset, 56.f, 1e-3f);
	play(scn, {150.f, 150.f}, false, {0.f, 5.f});
	EXPECT_NEAR(list.getComponent<UiList>().scrollOffset, 0.f, 1e-3f);
}

TEST_F(HudScrollTest, theArrowKeysMoveTheSelectionOfTheFocusedListAndScrollIt) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 3);
	auto below = makeWidget(scn, canvas, "Below", {100.f, 300.f}, {100.f, 30.f});
	below.addComponent<UiButton>();
	auto& data = list.getComponent<UiList>();
	ASSERT_TRUE(scene::UiInputSystem::setFocus(scn, list));
	tap(scn, input::key::Down);
	EXPECT_EQ(data.selectedIndex, 0);
	tap(scn, input::key::Down);
	tap(scn, input::key::Down);
	EXPECT_EQ(data.selectedIndex, 2);
	tap(scn, input::key::Up);
	EXPECT_EQ(data.selectedIndex, 1);
	tap(scn, input::key::Down);
	tap(scn, input::key::Down);
	// Past the last row the focus goes on to the next widget.
	EXPECT_EQ(data.selectedIndex, 2);
	EXPECT_EQ(scene::UiInputSystem::getFocus(scn).getName(), "Below");
}

TEST_F(HudScrollTest, theSelectedRowIsScrolledIntoView) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 100);
	auto& data = list.getComponent<UiList>();
	ASSERT_TRUE(scene::UiInputSystem::setFocus(scn, list));
	for (int i = 0; i < 6; ++i) tap(scn, input::key::Down);
	EXPECT_EQ(data.selectedIndex, 5);
	ASSERT_NE(scrollOf(scn, list), nullptr);
	// Row 5 spans 120 to 140: the list moves by the 40 that bring its bottom to the 100 of the view.
	EXPECT_NEAR(data.scrollOffset, 40.f, 1e-3f);
	data.select(60);
	ASSERT_NE(scrollOf(scn, list), nullptr);
	EXPECT_NEAR(data.scrollOffset, 60.f * 24.f + 20.f - 100.f, 1e-3f);
	data.select(0);
	ASSERT_NE(scrollOf(scn, list), nullptr);
	EXPECT_NEAR(data.scrollOffset, 0.f, 1e-3f);
}

TEST_F(HudScrollTest, theModelKeepsItsRowsAndDropsASelectionPastTheEnd) {
	UiList list;
	list.setCount(10);
	list.rows.resize(10);
	list.rows[8].bound = true;
	list.rows[8].text = "eight";
	EXPECT_TRUE(list.select(8));
	EXPECT_FALSE(list.select(8));
	EXPECT_TRUE(list.revealSelected);
	list.setCount(9);
	EXPECT_EQ(list.selectedIndex, 8);
	ASSERT_NE(list.findRow(8), nullptr);
	list.setCount(5);
	EXPECT_EQ(list.selectedIndex, -1);
	EXPECT_EQ(list.findRow(8), nullptr);
	EXPECT_FALSE(list.select(5));
	EXPECT_TRUE(list.select(4));
	EXPECT_TRUE(list.select(-3));
	EXPECT_EQ(list.selectedIndex, -1);
	list.rows.resize(5);
	list.rows[1].bound = true;
	list.invalidateRows();
	EXPECT_EQ(list.findRow(1), nullptr);
}

namespace {

class HudScrollPassTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "hudScrollPass",
														  .assetsPattern = "",
														  .icon = "",
														  .width = 0,
														  .height = 0,
														  .argCount = 0,
														  .renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
	}

	static void TearDownTestSuite() {
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override { renderer::Renderer2D::resetStats(); }

	inline static shared<app::Application> s_app;
};

auto quadsOf(scene::Scene& ioScene) -> uint32_t {
	renderer::Renderer2D::resetStats();
	scene::ui::HudPass::render(ioScene, g_viewport, false);
	return renderer::Renderer2D::getStats().quadCount;
}

}// namespace

TEST_F(HudScrollPassTest, aThousandRowListDrawsOnlyTheRowsInView) {
	scene::Scene big;
	const auto bigCanvas = makeCanvas(big);
	makeList(big, bigCanvas, 1000);
	scene::Scene small;
	const auto smallCanvas = makeCanvas(small);
	makeList(small, smallCanvas, 50);
	const auto bigQuads = quadsOf(big);
	EXPECT_GT(bigQuads, 0u);
	EXPECT_EQ(bigQuads, quadsOf(small));
	EXPECT_LT(bigQuads, 200u);
	// Five rows in view, the bar track and its thumb, and the texts: far below one quad per row.
	scene::Scene empty;
	const auto emptyCanvas = makeCanvas(empty);
	makeList(empty, emptyCanvas, 0);
	EXPECT_EQ(quadsOf(empty), 0u);
}

TEST_F(HudScrollPassTest, scrollingChangesTheRowsDrawnNotTheirNumber) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto list = makeList(scn, canvas, 1000);
	// Rows 100 to 104, then 200 to 204: five rows with a three digit number each time.
	list.getComponent<UiList>().scrollOffset = 2400.f;
	const auto first = quadsOf(scn);
	list.getComponent<UiList>().scrollOffset = 4800.f;
	EXPECT_EQ(quadsOf(scn), first);
	EXPECT_NEAR(list.getComponent<UiList>().scrollOffset, 4800.f, 1e-2f);
}

TEST_F(HudScrollPassTest, aScrollViewDrawsItsContentAndItsBar) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto view = makeView(scn, canvas, 400.f);
	const auto bare = quadsOf(scn);
	// The bar: its track and its thumb.
	EXPECT_EQ(bare, 2u);
	view.getComponent<UiScrollView>().showScrollBar = false;
	EXPECT_EQ(quadsOf(scn), 0u);
	view.getComponent<UiScrollView>().showScrollBar = true;
	makeWidget(scn, view, "Panel", {0.f, 0.f}, {500.f, 500.f}).addComponent<scene::component::UiPanel>();
	EXPECT_EQ(quadsOf(scn), 3u);
}
