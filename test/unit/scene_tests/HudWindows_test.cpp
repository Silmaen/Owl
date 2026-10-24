/**
 * @file HudWindows_test.cpp
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/Renderer.h>
#include <renderer/Renderer2D.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>
#include <scene/ui/HudPass.h>

#include <cstddef>
#include <cstdint>

using namespace owl;
using scene::component::Canvas;
using scene::component::UiLayout;
using scene::component::UiRect;
using scene::component::UiWindow;

namespace {

constexpr math::vec2ui g_viewport{800u, 600u};

class HudWindowsTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

auto makeCanvas(scene::Scene& ioScene) -> scene::Entity {
	auto canvas = ioScene.createEntity("Canvas");
	canvas.addComponent<Canvas>().scaleMode = Canvas::ScaleMode::ConstantPixels;
	return canvas;
}

// A widget pinned to the top-left of its parent: `iPosition` is its corner.
auto makeWidget(scene::Scene& ioScene, const scene::Entity& iParent, const math::vec2& iPosition,
				const math::vec2& iSize) -> scene::Entity {
	auto widget = ioScene.createEntity("Widget");
	auto& rect = widget.addComponent<UiRect>();
	rect.setAnchor(UiRect::Anchor::TopLeft);
	rect.pivot = {0.f, 0.f};
	rect.size = iSize;
	rect.anchorOffset = iPosition;
	ioScene.setParent(widget, iParent);
	return widget;
}

auto makeWindow(scene::Scene& ioScene, const scene::Entity& iParent, const math::vec2& iPosition,
				const math::vec2& iSize, const float iBar = 20.f) -> scene::Entity {
	auto window = makeWidget(ioScene, iParent, iPosition, iSize);
	window.addComponent<UiWindow>().titleBarHeight = iBar;
	return window;
}

auto nodeOf(scene::Scene& ioScene, const scene::Entity& iEntity) -> const scene::ui::HudNode* {
	auto& layout = ioScene.getHudLayout();
	layout.compute(ioScene, g_viewport, false);
	return layout.findNode(static_cast<entt::entity>(iEntity));
}

void expectCorner(const scene::ui::HudNode* iNode, const math::vec2& iPosition) {
	ASSERT_NE(iNode, nullptr);
	EXPECT_NEAR(iNode->bounds.position.x(), iPosition.x(), 1e-3f);
	EXPECT_NEAR(iNode->bounds.position.y(), iPosition.y(), 1e-3f);
}

void expectSize(const scene::ui::HudNode* iNode, const math::vec2& iSize) {
	ASSERT_NE(iNode, nullptr);
	EXPECT_NEAR(iNode->bounds.size.x(), iSize.x(), 1e-3f);
	EXPECT_NEAR(iNode->bounds.size.y(), iSize.y(), 1e-3f);
}

auto addButton(scene::Scene& ioScene, const scene::Entity& iParent, const math::vec2& iPosition,
			   const math::vec2& iSize) -> scene::Entity {
	auto button = makeWidget(ioScene, iParent, iPosition, iSize);
	button.addComponent<scene::component::UiButton>();
	return button;
}

// One mouse sample: a release then a press edge at the point, so the second call is a click.
void press(scene::Scene& ioScene, const math::vec2& iPoint, const float iDeltaTime = 0.f) {
	scene::UiInputSystem::update(&ioScene, g_viewport, iPoint, true, iDeltaTime);
}

void hover(scene::Scene& ioScene, const math::vec2& iPoint, const float iDeltaTime = 0.f) {
	scene::UiInputSystem::update(&ioScene, g_viewport, iPoint, false, iDeltaTime);
}

}// namespace

TEST_F(HudWindowsTest, windowContentStartsUnderTheTitleBar) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {10.f, 10.f}, {100.f, 80.f}, 20.f);
	const auto child = makeWidget(scn, window, {5.f, 5.f}, {10.f, 10.f});
	expectCorner(nodeOf(scn, window), {10.f, 10.f});
	const auto* node = nodeOf(scn, window);
	EXPECT_NEAR(node->content.position.y(), 30.f, 1e-3f);
	EXPECT_NEAR(node->content.size.y(), 60.f, 1e-3f);
	expectCorner(nodeOf(scn, child), {15.f, 35.f});
}

TEST_F(HudWindowsTest, titleBarScalesWithTheCanvas) {
	scene::Scene scn;
	auto canvas = scn.createEntity("Canvas");
	auto& component = canvas.addComponent<Canvas>();
	component.scaleMode = Canvas::ScaleMode::ScaleWithHeight;
	component.referenceResolution = {400.f, 300.f};
	auto window = makeWindow(scn, canvas, {10.f, 10.f}, {100.f, 80.f}, 20.f);
	const auto child = makeWidget(scn, window, {5.f, 5.f}, {10.f, 10.f});
	// 600 / 300: everything doubles, the bar included.
	expectCorner(nodeOf(scn, child), {30.f, 70.f});
}

TEST_F(HudWindowsTest, windowStaysInsideTheScreen) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	const auto right = makeWindow(scn, canvas, {790.f, 590.f}, {100.f, 80.f});
	const auto left = makeWindow(scn, canvas, {-50.f, -50.f}, {100.f, 80.f});
	expectCorner(nodeOf(scn, right), {700.f, 520.f});
	expectCorner(nodeOf(scn, left), {0.f, 0.f});
	// The component itself is left as authored.
	EXPECT_FLOAT_EQ(right.getComponent<UiRect>().anchorOffset.x(), 790.f);
}

TEST_F(HudWindowsTest, columnLayoutStacksTheChildren) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto box = makeWidget(scn, canvas, {100.f, 100.f}, {100.f, 200.f});
	auto& layout = box.addComponent<UiLayout>();
	layout.arrangement = UiLayout::Arrangement::Column;
	layout.spacing = 4.f;
	layout.padding = 6.f;
	const auto first = makeWidget(scn, box, {70.f, 70.f}, {50.f, 10.f});
	const auto second = makeWidget(scn, box, {0.f, 0.f}, {30.f, 20.f});
	const auto third = makeWidget(scn, box, {0.f, 0.f}, {40.f, 10.f});
	expectCorner(nodeOf(scn, first), {106.f, 106.f});
	expectCorner(nodeOf(scn, second), {106.f, 120.f});
	expectCorner(nodeOf(scn, third), {106.f, 144.f});
	expectSize(nodeOf(scn, second), {30.f, 20.f});
	layout.crossAlign = UiLayout::CrossAlign::End;
	expectCorner(nodeOf(scn, first), {144.f, 106.f});
	expectCorner(nodeOf(scn, third), {154.f, 144.f});
}

TEST_F(HudWindowsTest, rowLayoutCentersAcrossTheLine) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto box = makeWidget(scn, canvas, {0.f, 0.f}, {200.f, 60.f});
	auto& layout = box.addComponent<UiLayout>();
	layout.arrangement = UiLayout::Arrangement::Row;
	layout.crossAlign = UiLayout::CrossAlign::Center;
	layout.spacing = 4.f;
	layout.padding = 6.f;
	const auto first = makeWidget(scn, box, {0.f, 0.f}, {30.f, 10.f});
	const auto second = makeWidget(scn, box, {0.f, 0.f}, {20.f, 40.f});
	expectCorner(nodeOf(scn, first), {6.f, 25.f});
	expectCorner(nodeOf(scn, second), {40.f, 10.f});
}

TEST_F(HudWindowsTest, gridLayoutUsesTheLargestChildAsCell) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto box = makeWidget(scn, canvas, {0.f, 0.f}, {200.f, 200.f});
	auto& layout = box.addComponent<UiLayout>();
	layout.arrangement = UiLayout::Arrangement::Grid;
	layout.columns = 2;
	layout.spacing = 2.f;
	layout.padding = 0.f;
	const auto first = makeWidget(scn, box, {0.f, 0.f}, {20.f, 10.f});
	const auto second = makeWidget(scn, box, {0.f, 0.f}, {30.f, 12.f});
	const auto third = makeWidget(scn, box, {0.f, 0.f}, {10.f, 10.f});
	expectCorner(nodeOf(scn, first), {0.f, 0.f});
	expectCorner(nodeOf(scn, second), {32.f, 0.f});
	expectCorner(nodeOf(scn, third), {0.f, 14.f});
	layout.cellSize = {40.f, 0.f};
	layout.crossAlign = UiLayout::CrossAlign::Center;
	expectCorner(nodeOf(scn, second), {42.f + 5.f, 0.f});
	expectCorner(nodeOf(scn, third), {15.f, 15.f});
}

TEST_F(HudWindowsTest, hiddenChildrenTakeNoRoomInALayout) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto box = makeWidget(scn, canvas, {0.f, 0.f}, {100.f, 200.f});
	auto& layout = box.addComponent<UiLayout>();
	layout.spacing = 0.f;
	layout.padding = 0.f;
	makeWidget(scn, box, {0.f, 0.f}, {10.f, 10.f});
	const auto hidden = makeWidget(scn, box, {0.f, 0.f}, {10.f, 30.f});
	const auto last = makeWidget(scn, box, {0.f, 0.f}, {10.f, 10.f});
	hidden.getComponent<scene::component::Visibility>().gameVisible = false;
	expectCorner(nodeOf(scn, last), {0.f, 10.f});
	EXPECT_EQ(nodeOf(scn, hidden), nullptr);
}

TEST_F(HudWindowsTest, layoutInsideAWindowFollowsItsContentArea) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {50.f, 50.f}, {100.f, 100.f}, 24.f);
	auto& layout = window.addComponent<UiLayout>();
	layout.padding = 4.f;
	const auto child = makeWidget(scn, window, {0.f, 0.f}, {20.f, 20.f});
	expectCorner(nodeOf(scn, child), {54.f, 78.f});
}

TEST_F(HudWindowsTest, windowsAndClippingPanelsClipTheirDescendants) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {10.f, 10.f}, {100.f, 80.f}, 20.f);
	const auto inside = makeWidget(scn, window, {0.f, 0.f}, {300.f, 300.f});
	const auto after = makeWidget(scn, canvas, {400.f, 400.f}, {50.f, 50.f});
	auto panel = makeWidget(scn, canvas, {200.f, 200.f}, {60.f, 40.f});
	panel.addComponent<scene::component::UiPanel>().clipChildren = true;
	const auto cut = makeWidget(scn, panel, {-10.f, -10.f}, {200.f, 200.f});
	const auto* windowNode = nodeOf(scn, window);
	const auto& layout = scn.getHudLayout();
	const auto* insideNode = layout.findNode(static_cast<entt::entity>(inside));
	const auto* afterNode = layout.findNode(static_cast<entt::entity>(after));
	const auto* cutNode = layout.findNode(static_cast<entt::entity>(cut));
	ASSERT_NE(insideNode, nullptr);
	EXPECT_FALSE(windowNode->clipped);
	EXPECT_TRUE(insideNode->clipped);
	EXPECT_NEAR(insideNode->clip.position.y(), 30.f, 1e-3f);
	EXPECT_NEAR(insideNode->clip.size.x(), 100.f, 1e-3f);
	EXPECT_NEAR(insideNode->clip.size.y(), 60.f, 1e-3f);
	EXPECT_FALSE(afterNode->clipped);
	ASSERT_NE(cutNode, nullptr);
	EXPECT_TRUE(cutNode->clipped);
	EXPECT_NEAR(cutNode->clip.position.x(), 200.f, 1e-3f);
	EXPECT_NEAR(cutNode->clip.size.y(), 40.f, 1e-3f);
	// Windows number their subtree.
	EXPECT_EQ(insideNode->window, static_cast<size_t>(windowNode - layout.getNodes().data()));
	EXPECT_EQ(afterNode->window, size_t{scene::ui::HudNode::noParent});
}

TEST_F(HudWindowsTest, hitTestStopsAtTheClipRectangle) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {0.f, 0.f}, {100.f, 80.f}, 20.f);
	const auto big = makeWidget(scn, window, {0.f, 0.f}, {300.f, 300.f});
	auto& layout = scn.getHudLayout();
	layout.compute(scn, g_viewport, false);
	const auto* inside = layout.hitTest({50.f, 50.f});
	ASSERT_NE(inside, nullptr);
	EXPECT_EQ(inside->entity, static_cast<entt::entity>(big));
	EXPECT_EQ(layout.hitTest({150.f, 50.f}), nullptr);
	// The title bar belongs to the window, not to the content child.
	const auto* bar = layout.hitTest({50.f, 10.f});
	ASSERT_NE(bar, nullptr);
	EXPECT_EQ(bar->entity, static_cast<entt::entity>(window));
}

TEST_F(HudWindowsTest, aWindowBlocksTheWidgetsUnderIt) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	const auto under = addButton(scn, canvas, {100.f, 100.f}, {100.f, 100.f});
	makeWindow(scn, canvas, {120.f, 120.f}, {200.f, 100.f});
	hover(scn, {150.f, 150.f});
	EXPECT_EQ(under.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming(scn));
	hover(scn, {110.f, 110.f});
	EXPECT_EQ(under.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Hovered);
	hover(scn, {700.f, 500.f});
	EXPECT_FALSE(scene::UiInputSystem::isUIConsuming(scn));
}

TEST_F(HudWindowsTest, aModalWindowTakesTheMouseAndTheFocus) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	const auto outside = addButton(scn, canvas, {500.f, 500.f}, {100.f, 50.f});
	auto other = makeWindow(scn, canvas, {20.f, 20.f}, {100.f, 100.f});
	const auto inOther = addButton(scn, other, {10.f, 10.f}, {60.f, 30.f});
	auto dialog = makeWindow(scn, canvas, {200.f, 200.f}, {200.f, 150.f});
	dialog.getComponent<UiWindow>().modal = true;
	const auto inDialog = addButton(scn, dialog, {10.f, 10.f}, {60.f, 30.f});
	hover(scn, {520.f, 520.f});
	EXPECT_EQ(outside.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming(scn));
	hover(scn, {40.f, 60.f});
	EXPECT_EQ(inOther.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);
	hover(scn, {220.f, 240.f});
	EXPECT_EQ(inDialog.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Hovered);
	// Hidden, the dialog lets the mouse through again.
	dialog.getComponent<scene::component::Visibility>().gameVisible = false;
	hover(scn, {520.f, 520.f});
	EXPECT_EQ(outside.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Hovered);
}

TEST_F(HudWindowsTest, aButtonInsideAWindowStillClicks) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f}, 20.f);
	const auto button = addButton(scn, window, {10.f, 10.f}, {60.f, 30.f});
	hover(scn, {120.f, 140.f});
	EXPECT_EQ(button.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Hovered);
	// A button cut away by the window clip cannot be reached.
	const auto far = addButton(scn, window, {10.f, 110.f}, {60.f, 60.f});
	hover(scn, {120.f, 270.f});
	EXPECT_EQ(far.getComponent<scene::component::UiButton>().state, scene::component::UiButton::State::Normal);
}

TEST_F(HudWindowsTest, clickingAWindowRaisesIt) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	const auto lower = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f});
	const auto upper = makeWindow(scn, canvas, {250.f, 150.f}, {200.f, 150.f});
	const auto& order = canvas.getComponent<scene::component::Hierarchy>().childrenIds;
	ASSERT_EQ(order.size(), 2u);
	EXPECT_EQ(order.back(), upper.getUUID());
	// Where the two overlap the upper one takes the click.
	hover(scn, {270.f, 200.f});
	press(scn, {270.f, 200.f});
	EXPECT_EQ(order.back(), upper.getUUID());
	hover(scn, {130.f, 200.f});
	press(scn, {130.f, 200.f});
	EXPECT_EQ(order.back(), lower.getUUID());
	// A single click raises once: the order is stable under a held button.
	press(scn, {130.f, 200.f});
	EXPECT_EQ(order.back(), lower.getUUID());
}

TEST_F(HudWindowsTest, raiseWindowReportsAChange) {
	scene::Scene scn;
	const auto canvas = makeCanvas(scn);
	const auto first = makeWindow(scn, canvas, {0.f, 0.f}, {10.f, 10.f});
	const auto second = makeWindow(scn, canvas, {0.f, 0.f}, {10.f, 10.f});
	EXPECT_FALSE(scene::UiInputSystem::raiseWindow(scn, static_cast<entt::entity>(second)));
	EXPECT_TRUE(scene::UiInputSystem::raiseWindow(scn, static_cast<entt::entity>(first)));
	EXPECT_EQ(canvas.getComponent<scene::component::Hierarchy>().childrenIds.back(), first.getUUID());
	EXPECT_FALSE(scene::UiInputSystem::raiseWindow(scn, static_cast<entt::entity>(canvas)));
}

TEST_F(HudWindowsTest, theTitleBarDragsTheWindowInsideTheScreen) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f}, 20.f);
	// The click lands on the title bar, 30 px from the left and 10 px from the top of the window.
	hover(scn, {130.f, 110.f});
	press(scn, {130.f, 110.f});
	press(scn, {160.f, 130.f});
	EXPECT_NEAR(window.getComponent<UiRect>().anchorOffset.x(), 130.f, 1e-3f);
	EXPECT_NEAR(window.getComponent<UiRect>().anchorOffset.y(), 120.f, 1e-3f);
	EXPECT_TRUE(scene::UiInputSystem::isUIConsuming(scn));
	// Far past the corner: the window stops at the screen border, whatever the mouse does.
	press(scn, {5000.f, 5000.f});
	EXPECT_NEAR(window.getComponent<UiRect>().anchorOffset.x(), 600.f, 1e-3f);
	EXPECT_NEAR(window.getComponent<UiRect>().anchorOffset.y(), 450.f, 1e-3f);
	// Coming back moves it at once, with no dead zone.
	press(scn, {500.f, 300.f});
	EXPECT_LT(window.getComponent<UiRect>().anchorOffset.x(), 600.f);
	// Released, the window stays where it is.
	hover(scn, {300.f, 300.f});
	const auto offset = window.getComponent<UiRect>().anchorOffset;
	hover(scn, {350.f, 350.f});
	EXPECT_EQ(window.getComponent<UiRect>().anchorOffset, offset);
}

TEST_F(HudWindowsTest, aFixedWindowIsNotDragged) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f}, 20.f);
	window.getComponent<UiWindow>().draggable = false;
	hover(scn, {130.f, 110.f});
	press(scn, {130.f, 110.f});
	press(scn, {200.f, 200.f});
	EXPECT_NEAR(window.getComponent<UiRect>().anchorOffset.x(), 100.f, 1e-3f);
}

TEST_F(HudWindowsTest, theCloseButtonHidesTheWindow) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f}, 20.f);
	// The close button is the square at the right end of the bar.
	hover(scn, {285.f, 110.f});
	press(scn, {285.f, 110.f});
	EXPECT_FALSE(window.getComponent<scene::component::Visibility>().gameVisible);
	EXPECT_EQ(nodeOf(scn, window), nullptr);
}

TEST_F(HudWindowsTest, aWindowWithoutCloseButtonStaysOpen) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {100.f, 100.f}, {200.f, 150.f}, 20.f);
	window.getComponent<UiWindow>().closable = false;
	hover(scn, {285.f, 110.f});
	press(scn, {285.f, 110.f});
	EXPECT_TRUE(window.getComponent<scene::component::Visibility>().gameVisible);
}

TEST_F(HudWindowsTest, tooltipWaitsForTheHoverDelay) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto panel = makeWidget(scn, canvas, {100.f, 100.f}, {100.f, 100.f});
	panel.addComponent<scene::component::UiTooltip>().text = "Build a road";
	makeWidget(scn, panel, {10.f, 10.f}, {20.f, 20.f});
	auto& state = scn.getUiInputState();
	hover(scn, {50.f, 50.f}, 0.1f);
	EXPECT_EQ(state.tooltipWidget, entt::null);
	hover(scn, {150.f, 150.f}, 0.1f);
	EXPECT_EQ(state.tooltipWidget, static_cast<entt::entity>(panel));
	EXPECT_NEAR(state.hoverTime, 0.f, 1e-6f);
	hover(scn, {150.f, 150.f}, 0.3f);
	hover(scn, {150.f, 150.f}, 0.3f);
	EXPECT_NEAR(state.hoverTime, 0.6f, 1e-5f);
	// A descendant without a tooltip of its own shows its ancestor's, and keeps the timer running.
	hover(scn, {115.f, 115.f}, 0.2f);
	EXPECT_EQ(state.tooltipWidget, static_cast<entt::entity>(panel));
	EXPECT_NEAR(state.hoverTime, 0.8f, 1e-5f);
	// Leaving the widget forgets it.
	hover(scn, {600.f, 500.f}, 0.1f);
	EXPECT_EQ(state.tooltipWidget, entt::null);
	EXPECT_NEAR(state.hoverTime, 0.f, 1e-6f);
}

TEST_F(HudWindowsTest, aTooltipHidesWhileTheMouseIsPressed) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto panel = makeWidget(scn, canvas, {100.f, 100.f}, {100.f, 100.f});
	panel.addComponent<scene::component::UiTooltip>().text = "Hint";
	hover(scn, {150.f, 150.f}, 0.1f);
	hover(scn, {150.f, 150.f}, 1.f);
	EXPECT_GT(scn.getUiInputState().hoverTime, 0.5f);
	press(scn, {150.f, 150.f}, 0.1f);
	EXPECT_NEAR(scn.getUiInputState().hoverTime, 0.f, 1e-6f);
}

TEST_F(HudWindowsTest, windowComponentsRoundTripThroughYaml) {
	const auto original = mkShared<scene::Scene>();
	auto& scn = *original;
	const auto canvas = makeCanvas(scn);
	auto window = makeWindow(scn, canvas, {1.f, 2.f}, {3.f, 4.f}, 11.f);
	auto& data = window.getComponent<UiWindow>();
	data.title = "Depot";
	data.closable = false;
	data.draggable = false;
	data.onCloseCallback = "on_close";
	data.modal = true;
	auto& layout = window.addComponent<UiLayout>();
	layout.arrangement = UiLayout::Arrangement::Grid;
	layout.crossAlign = UiLayout::CrossAlign::End;
	layout.columns = 5;
	layout.cellSize = {7.f, 8.f};
	window.addComponent<scene::component::UiTooltip>().text = "Hint";
	window.addComponent<scene::component::UiToolCursor>().hotspot = {0.25f, 0.75f};
	const auto copy = scene::Scene::copy(original);
	const auto found = copy->findEntityByUUID(window.getUUID());
	ASSERT_TRUE(found);
	EXPECT_EQ(found.getComponent<UiWindow>().title, "Depot");
	EXPECT_FALSE(found.getComponent<UiWindow>().closable);
	EXPECT_EQ(found.getComponent<UiWindow>().onCloseCallback, "on_close");
	EXPECT_TRUE(found.getComponent<UiWindow>().modal);
	EXPECT_EQ(found.getComponent<UiLayout>().columns, 5u);
	EXPECT_EQ(found.getComponent<UiLayout>().arrangement, UiLayout::Arrangement::Grid);
	EXPECT_EQ(found.getComponent<scene::component::UiTooltip>().text, "Hint");
	EXPECT_NEAR(found.getComponent<scene::component::UiToolCursor>().hotspot.y(), 0.75f, 1e-6f);
}

namespace {

class HudWindowsPassTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														  .frameLogFrequency = 0,
														  .name = "hudWindowsPass",
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

}// namespace

TEST_F(HudWindowsPassTest, aWindowDrawsItsBodyBarAndCloseButton) {
	scene::Scene scn;
	auto canvas = makeCanvas(scn);
	const auto window = makeWindow(scn, canvas, {10.f, 10.f}, {100.f, 80.f});
	scene::ui::HudPass::render(scn, g_viewport, false);
	const auto closable = renderer::Renderer2D::getStats().quadCount;
	renderer::Renderer2D::resetStats();
	window.getComponent<UiWindow>().closable = false;
	scene::ui::HudPass::render(scn, g_viewport, false);
	const auto fixed = renderer::Renderer2D::getStats().quadCount;
	// Body and title bar, then the close button and its mark.
	EXPECT_GE(fixed, 2u);
	EXPECT_GT(closable, fixed);
}

TEST_F(HudWindowsPassTest, eachWindowIsABatchOfItsOwn) {
	scene::Scene plain;
	const auto plainCanvas = makeCanvas(plain);
	for (const auto x: {10.f, 40.f})
		makeWidget(plain, plainCanvas, {x, 10.f}, {50.f, 50.f}).addComponent<scene::component::UiPanel>();
	scene::ui::HudPass::render(plain, g_viewport, false);
	const auto plainCalls = renderer::Renderer2D::getStats().drawCalls;
	renderer::Renderer2D::resetStats();
	scene::Scene windows;
	const auto windowCanvas = makeCanvas(windows);
	for (const auto x: {10.f, 40.f}) makeWindow(windows, windowCanvas, {x, 10.f}, {50.f, 50.f});
	scene::ui::HudPass::render(windows, g_viewport, false);
	EXPECT_GE(renderer::Renderer2D::getStats().drawCalls, plainCalls + 1u);
}

TEST_F(HudWindowsPassTest, clippedContentStillDraws) {
	scene::Scene scn;
	auto canvas = makeCanvas(scn);
	const auto window = makeWindow(scn, canvas, {10.f, 10.f}, {100.f, 80.f});
	scene::ui::HudPass::render(scn, g_viewport, false);
	const auto empty = renderer::Renderer2D::getStats().quadCount;
	renderer::Renderer2D::resetStats();
	makeWidget(scn, window, {0.f, 0.f}, {500.f, 500.f}).addComponent<scene::component::UiPanel>();
	scene::ui::HudPass::render(scn, g_viewport, false);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, empty + 1u);
}

TEST_F(HudWindowsPassTest, theToolCursorAndTheTooltipDrawOverTheHud) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	auto canvas = makeCanvas(scn);
	auto panel = makeWidget(scn, canvas, {100.f, 100.f}, {100.f, 100.f});
	panel.addComponent<scene::component::UiTooltip>().text = "Hint";
	auto cursor = scn.createEntity("Cursor");
	cursor.addComponent<scene::component::UiToolCursor>();
	// Nothing is drawn before the first mouse sample.
	scene::ui::HudPass::render(scn, g_viewport, false);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 0u);
	hover(scn, {150.f, 150.f}, 0.1f);
	scene::ui::HudPass::render(scn, g_viewport, false);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, 0u);
	hover(scn, {150.f, 150.f}, 1.f);
	scene::ui::HudPass::render(scn, g_viewport, false);
	const auto withTooltip = renderer::Renderer2D::getStats().quadCount;
	EXPECT_GE(withTooltip, 1u);
	renderer::Renderer2D::resetStats();
	cursor.getComponent<scene::component::UiToolCursor>().active = true;
	scene::ui::HudPass::render(scn, g_viewport, false);
	EXPECT_EQ(renderer::Renderer2D::getStats().quadCount, withTooltip + 1u);
}
