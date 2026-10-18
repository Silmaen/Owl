/**
 * @file HudFocus_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <input/ActionFrame.h>
#include <input/ActionMap.h>
#include <input/InputFrame.h>
#include <input/KeyCodes.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>

#include <string>
#include <unordered_map>

using namespace owl;
using scene::component::Canvas;
using scene::component::UiButton;
using scene::component::UiFocus;
using scene::component::UiRect;
using scene::component::UiSlider;
using scene::component::UiWindow;

namespace {

constexpr math::vec2ui g_viewport{800u, 600u};

class HudFocusTest : public ::testing::Test {
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

auto addButton(scene::Scene& ioScene, const scene::Entity& iParent, const std::string& iName,
			   const math::vec2& iPosition) -> scene::Entity {
	auto button = makeWidget(ioScene, iParent, iName, iPosition, {100.f, 30.f});
	button.addComponent<UiButton>();
	return button;
}

auto addSlider(scene::Scene& ioScene, const scene::Entity& iParent, const std::string& iName,
			   const math::vec2& iPosition) -> scene::Entity {
	auto slider = makeWidget(ioScene, iParent, iName, iPosition, {200.f, 20.f});
	slider.addComponent<UiSlider>();
	return slider;
}

// The actions of one frame in which a key just went down (or stays down for the repeat).
auto keyFrame(const input::KeyCode iKey, const bool iHeldBefore = false) -> input::ActionFrame {
	input::InputFrame frame;
	frame.current.setKey(iKey, true);
	frame.previous.setKey(iKey, iHeldBefore);
	input::ActionFrame actions;
	actions.update(input::ActionMap::getDefault(), frame);
	return actions;
}

void tap(scene::Scene& ioScene, const input::KeyCode iKey) {
	scene::UiInputSystem::navigate(ioScene, g_viewport, keyFrame(iKey));
	// The next frame: released.
	scene::UiInputSystem::navigate(ioScene, g_viewport, input::ActionFrame{});
}

auto focusName(scene::Scene& ioScene) -> std::string {
	const auto focus = scene::UiInputSystem::getFocus(ioScene);
	return focus ? focus.getName() : std::string{};
}

// Three buttons in a column and a fourth on the right of the middle one.
struct Menu {
	scene::Scene scn;
	scene::Entity canvas;
	scene::Entity top;
	scene::Entity middle;
	scene::Entity bottom;
	scene::Entity side;

	Menu() {
		scn.status = scene::Scene::Status::Playing;
		canvas = makeCanvas(scn);
		top = addButton(scn, canvas, "Top", {100.f, 100.f});
		middle = addButton(scn, canvas, "Middle", {100.f, 160.f});
		bottom = addButton(scn, canvas, "Bottom", {100.f, 220.f});
		side = addButton(scn, canvas, "Side", {300.f, 160.f});
	}
};

}// namespace

TEST_F(HudFocusTest, noFocusUntilAKeyIsPressed) {
	Menu menu;
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
	EXPECT_FALSE(menu.scn.getUiInputState().focusVisible);
}

TEST_F(HudFocusTest, theFirstPressOnlyShowsTheFocus) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Top");
	EXPECT_TRUE(menu.scn.getUiInputState().focusVisible);
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Middle");
}

TEST_F(HudFocusTest, directionsPickTheNearestWidgetThatWay) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Middle");
	tap(menu.scn, input::key::Right);
	EXPECT_EQ(focusName(menu.scn), "Side");
	tap(menu.scn, input::key::Right);
	EXPECT_EQ(focusName(menu.scn), "Side");
	tap(menu.scn, input::key::Left);
	EXPECT_EQ(focusName(menu.scn), "Middle");
}

TEST_F(HudFocusTest, anInitialWidgetTakesTheFocusFirst) {
	Menu menu;
	menu.bottom.addComponent<UiFocus>().initial = true;
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	EXPECT_TRUE(menu.scn.getUiInputState().focusVisible);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Middle");
}

TEST_F(HudFocusTest, anInitialWidgetOnlyTakesTheFocusOnce) {
	Menu menu;
	menu.bottom.addComponent<UiFocus>().initial = true;
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	scene::UiInputSystem::clearFocus(menu.scn);
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
}

TEST_F(HudFocusTest, aHiddenInitialWidgetDoesNotTakeTheFocus) {
	Menu menu;
	menu.bottom.addComponent<UiFocus>().initial = true;
	menu.bottom.getComponent<scene::component::Visibility>().gameVisible = false;
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
}

TEST_F(HudFocusTest, disabledAndNotFocusableWidgetsAreSkipped) {
	Menu menu;
	menu.middle.getComponent<UiButton>().state = UiButton::State::Disabled;
	menu.side.addComponent<UiFocus>().focusable = false;
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Right);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
}

TEST_F(HudFocusTest, hiddenWidgetsAreSkipped) {
	Menu menu;
	menu.middle.getComponent<scene::component::Visibility>().gameVisible = false;
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
}

TEST_F(HudFocusTest, explicitNeighboursOverrideTheLayout) {
	Menu menu;
	auto& focus = menu.top.addComponent<UiFocus>();
	focus.down = menu.bottom.getUUID();
	focus.right = core::UUID{0x1234};
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Up);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Top");
	// An unknown neighbour falls back to the layout, which finds the button on the right.
	tap(menu.scn, input::key::Right);
	EXPECT_EQ(focusName(menu.scn), "Side");
}

TEST_F(HudFocusTest, aHeldDirectionRepeats) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	scene::UiInputSystem::navigate(menu.scn, g_viewport, keyFrame(input::key::Down), 0.f);
	EXPECT_EQ(focusName(menu.scn), "Middle");
	scene::UiInputSystem::navigate(menu.scn, g_viewport, keyFrame(input::key::Down, true), 0.3f);
	EXPECT_EQ(focusName(menu.scn), "Middle");
	scene::UiInputSystem::navigate(menu.scn, g_viewport, keyFrame(input::key::Down, true), 0.15f);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{}, 0.5f);
	EXPECT_EQ(menu.scn.getUiInputState().repeatDirection, 0);
}

TEST_F(HudFocusTest, leftAndRightAdjustAFocusedSlider) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto slider = addSlider(scn, canvas, "Volume", {100.f, 100.f});
	tap(scn, input::key::Right);
	EXPECT_EQ(focusName(scn), "Volume");
	EXPECT_NEAR(slider.getComponent<UiSlider>().value, 0.f, 1e-6f);
	tap(scn, input::key::Right);
	EXPECT_NEAR(slider.getComponent<UiSlider>().value, 0.05f, 1e-5f);
	tap(scn, input::key::Left);
	tap(scn, input::key::Left);
	EXPECT_NEAR(slider.getComponent<UiSlider>().value, 0.f, 1e-6f);
	slider.getComponent<UiSlider>().step = 0.4f;
	tap(scn, input::key::Right);
	tap(scn, input::key::Right);
	tap(scn, input::key::Right);
	EXPECT_NEAR(slider.getComponent<UiSlider>().value, 1.f, 1e-6f);
}

TEST_F(HudFocusTest, aSliderGivesWayVerticallyAndToANeighbour) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto slider = addSlider(scn, canvas, "Volume", {100.f, 100.f});
	addButton(scn, canvas, "Back", {100.f, 200.f});
	const auto other = addButton(scn, canvas, "Other", {400.f, 100.f});
	tap(scn, input::key::Down);
	tap(scn, input::key::Down);
	EXPECT_EQ(focusName(scn), "Back");
	tap(scn, input::key::Up);
	EXPECT_EQ(focusName(scn), "Volume");
	slider.addComponent<UiFocus>().right = other.getUUID();
	tap(scn, input::key::Right);
	EXPECT_EQ(focusName(scn), "Other");
	EXPECT_NEAR(slider.getComponent<UiSlider>().value, 0.f, 1e-6f);
}

TEST_F(HudFocusTest, theFocusStaysInTheTopWindow) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	addButton(scn, canvas, "Behind", {500.f, 400.f});
	auto window = makeWidget(scn, canvas, "Window", {50.f, 50.f}, {300.f, 200.f});
	window.addComponent<UiWindow>().titleBarHeight = 20.f;
	addButton(scn, window, "InWindow", {10.f, 10.f});
	addButton(scn, window, "InWindowToo", {10.f, 60.f});
	tap(scn, input::key::Down);
	EXPECT_EQ(focusName(scn), "InWindow");
	tap(scn, input::key::Down);
	tap(scn, input::key::Down);
	EXPECT_EQ(focusName(scn), "InWindowToo");
	tap(scn, input::key::Right);
	EXPECT_EQ(focusName(scn), "InWindowToo");
}

TEST_F(HudFocusTest, cancelClosesAClosableTopWindowAndTheFocusMovesOn) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	addButton(scn, canvas, "Behind", {500.f, 400.f});
	auto window = makeWidget(scn, canvas, "Window", {50.f, 50.f}, {300.f, 200.f});
	window.addComponent<UiWindow>().titleBarHeight = 20.f;
	addButton(scn, window, "InWindow", {10.f, 10.f});
	tap(scn, input::key::Down);
	EXPECT_EQ(focusName(scn), "InWindow");
	window.getComponent<UiWindow>().closable = false;
	tap(scn, input::key::Escape);
	EXPECT_TRUE(window.getComponent<scene::component::Visibility>().gameVisible);
	window.getComponent<UiWindow>().closable = true;
	tap(scn, input::key::Escape);
	EXPECT_FALSE(window.getComponent<scene::component::Visibility>().gameVisible);
	// The focus was in the window: it goes to the widget behind.
	scene::UiInputSystem::navigate(scn, g_viewport, input::ActionFrame{});
	EXPECT_EQ(focusName(scn), "Behind");
}

TEST_F(HudFocusTest, cancelDoesNothingBeforeTheNavigationStarts) {
	scene::Scene scn;
	scn.status = scene::Scene::Status::Playing;
	const auto canvas = makeCanvas(scn);
	auto window = makeWidget(scn, canvas, "Window", {50.f, 50.f}, {300.f, 200.f});
	window.addComponent<UiWindow>();
	addButton(scn, window, "InWindow", {10.f, 10.f});
	tap(scn, input::key::Escape);
	EXPECT_TRUE(window.getComponent<scene::component::Visibility>().gameVisible);
	EXPECT_FALSE(scene::UiInputSystem::getFocus(scn));
}

TEST_F(HudFocusTest, theMouseTakesTheFocusAndHidesItsHighlight) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	EXPECT_TRUE(menu.scn.getUiInputState().focusVisible);
	scene::UiInputSystem::update(&menu.scn, g_viewport, {10.f, 10.f}, false);
	EXPECT_EQ(focusName(menu.scn), "Top");
	// Moving over a widget focuses it, without showing the highlight.
	scene::UiInputSystem::update(&menu.scn, g_viewport, {150.f, 235.f}, false);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	EXPECT_FALSE(menu.scn.getUiInputState().focusVisible);
	// A press brings the highlight back, then the keys go on from the hovered widget.
	tap(menu.scn, input::key::Up);
	EXPECT_TRUE(menu.scn.getUiInputState().focusVisible);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Middle");
	// A motionless mouse does not take it back.
	scene::UiInputSystem::update(&menu.scn, g_viewport, {150.f, 235.f}, false);
	EXPECT_EQ(focusName(menu.scn), "Middle");
}

TEST_F(HudFocusTest, aFocusedButtonLooksHovered) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	scene::UiInputSystem::update(&menu.scn, g_viewport, {700.f, 500.f}, false);
	EXPECT_EQ(menu.top.getComponent<UiButton>().state, UiButton::State::Hovered);
	EXPECT_EQ(menu.middle.getComponent<UiButton>().state, UiButton::State::Normal);
}

TEST_F(HudFocusTest, setFocusFocusesAndShowsAWidget) {
	Menu menu;
	EXPECT_TRUE(scene::UiInputSystem::setFocus(menu.scn, menu.bottom));
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	EXPECT_TRUE(menu.scn.getUiInputState().focusVisible);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Middle");
	scene::UiInputSystem::clearFocus(menu.scn);
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
	EXPECT_FALSE(menu.scn.getUiInputState().focusVisible);
}

TEST_F(HudFocusTest, setFocusRefusesWhatCannotTakeIt) {
	Menu menu;
	auto label = makeWidget(menu.scn, menu.canvas, "Label", {10.f, 10.f}, {10.f, 10.f});
	EXPECT_FALSE(scene::UiInputSystem::setFocus(menu.scn, label));
	menu.top.getComponent<UiButton>().state = UiButton::State::Disabled;
	EXPECT_FALSE(scene::UiInputSystem::setFocus(menu.scn, menu.top));
	menu.middle.addComponent<UiFocus>().focusable = false;
	EXPECT_FALSE(scene::UiInputSystem::setFocus(menu.scn, menu.middle));
	EXPECT_FALSE(scene::UiInputSystem::setFocus(menu.scn, scene::Entity{}));
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
}

TEST_F(HudFocusTest, aDestroyedFocusIsDropped) {
	Menu menu;
	scene::UiInputSystem::setFocus(menu.scn, menu.middle);
	menu.scn.destroyEntity(menu.middle);
	scene::UiInputSystem::navigate(menu.scn, g_viewport, input::ActionFrame{});
	// The highlight was shown: the first widget takes over.
	EXPECT_EQ(focusName(menu.scn), "Top");
}

TEST_F(HudFocusTest, resetForgetsTheFocus) {
	Menu menu;
	scene::UiInputSystem::setFocus(menu.scn, menu.middle);
	scene::UiInputSystem::reset(menu.scn);
	EXPECT_FALSE(scene::UiInputSystem::getFocus(menu.scn));
}

TEST_F(HudFocusTest, uiFocusSurvivesASaveAndLoad) {
	scene::Scene scn;
	auto widget = scn.createEntity("Widget");
	auto& focus = widget.addComponent<UiFocus>();
	focus.focusable = false;
	focus.initial = true;
	focus.up = core::UUID{11};
	focus.down = core::UUID{22};
	focus.left = core::UUID{33};
	focus.right = core::UUID{44};
	focus.onFocusCallback = "on_focus";
	focus.highlightColor = {0.1f, 0.2f, 0.3f, 0.4f};
	focus.highlightThickness = 3.5f;
	widget.addComponent<UiSlider>().step = 0.25f;
	const auto yaml = scene::SceneSerializer::serializeEntityToString(widget);
	const auto other = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(other, yaml));
	const auto copy = other->findEntityByUUID(widget.getUUID());
	ASSERT_TRUE(copy);
	ASSERT_TRUE(copy.hasComponent<UiFocus>());
	const auto loaded = copy.getComponent<UiFocus>();
	EXPECT_FALSE(loaded.focusable);
	EXPECT_TRUE(loaded.initial);
	EXPECT_EQ(loaded.up, core::UUID{11});
	EXPECT_EQ(loaded.down, core::UUID{22});
	EXPECT_EQ(loaded.left, core::UUID{33});
	EXPECT_EQ(loaded.right, core::UUID{44});
	EXPECT_EQ(loaded.onFocusCallback, "on_focus");
	EXPECT_NEAR(loaded.highlightColor.w(), 0.4f, 1e-6f);
	EXPECT_NEAR(loaded.highlightThickness, 3.5f, 1e-6f);
	EXPECT_NEAR(copy.getComponent<UiSlider>().step, 0.25f, 1e-6f);
}

TEST_F(HudFocusTest, theFocusStopsAtTheEndsByDefault) {
	Menu menu;
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Top");
}

TEST_F(HudFocusTest, aWrappingCanvasJumpsToTheOppositeEnd) {
	Menu menu;
	menu.canvas.getComponent<Canvas>().wrapFocus = true;
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
	tap(menu.scn, input::key::Down);
	EXPECT_EQ(focusName(menu.scn), "Top");
}

TEST_F(HudFocusTest, wrappingStaysInTheMenuOfTheFocus) {
	Menu menu;
	menu.canvas.getComponent<Canvas>().wrapFocus = true;
	auto other = makeCanvas(menu.scn);
	addButton(menu.scn, other, "Elsewhere", {100.f, 400.f});
	tap(menu.scn, input::key::Down);
	tap(menu.scn, input::key::Up);
	EXPECT_EQ(focusName(menu.scn), "Bottom");
}

TEST_F(HudFocusTest, duplicatingASubtreePointsTheNeighboursAtTheCopies) {
	Menu menu;
	menu.top.addComponent<UiFocus>().down = menu.bottom.getUUID();
	menu.bottom.addComponent<UiFocus>().up = menu.top.getUUID();
	const core::UUID outside{0x99};
	menu.middle.addComponent<UiFocus>().up = outside;
	const auto copy = menu.scn.duplicateSubtree(menu.canvas);
	const auto& children = copy.getComponent<scene::component::Hierarchy>().childrenIds;
	ASSERT_EQ(children.size(), 4u);
	std::unordered_map<std::string, scene::Entity> byName;
	for (const auto id: children) {
		const auto child = menu.scn.findEntityByUUID(id);
		byName.emplace(child.getName(), child);
	}
	EXPECT_EQ(byName.at("Top").getComponent<UiFocus>().down, byName.at("Bottom").getUUID());
	EXPECT_EQ(byName.at("Bottom").getComponent<UiFocus>().up, byName.at("Top").getUUID());
	EXPECT_EQ(byName.at("Middle").getComponent<UiFocus>().up, outside);
	EXPECT_EQ(menu.top.getComponent<UiFocus>().down, menu.bottom.getUUID());
}

TEST_F(HudFocusTest, wrapFocusSurvivesASaveAndLoad) {
	scene::Scene scn;
	auto canvas = makeCanvas(scn);
	canvas.getComponent<Canvas>().wrapFocus = true;
	const auto yaml = scene::SceneSerializer::serializeEntityToString(canvas);
	const auto other = mkShared<scene::Scene>();
	ASSERT_TRUE(scene::SceneSerializer::deserializeEntityFromString(other, yaml));
	EXPECT_TRUE(other->findEntityByUUID(canvas.getUUID()).getComponent<Canvas>().wrapFocus);
}
