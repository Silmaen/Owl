/**
 * @file UiInputSystem.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/UiInputSystem.h"

#include "scene/Entity.h"
#include "scene/component/Canvas.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/ID.h"
#include "scene/component/LuaScript.h"
#include "scene/component/Tag.h"
#include "scene/component/UiButton.h"
#include "scene/component/UiFocus.h"
#include "scene/component/UiList.h"
#include "scene/component/UiScrollView.h"
#include "scene/component/UiSlider.h"
#include "scene/component/UiTooltip.h"
#include "scene/component/UiWindow.h"
#include "scene/component/Visibility.h"
#include "scene/ui/HudLayout.h"
#include "scene/ui/ScrollMath.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::scene {

namespace {

auto findScriptOwner(const Scene* iScene, Entity iEntity) -> Entity {
	while (iEntity) {
		if (iEntity.hasComponent<component::LuaScript>())
			break;
		const auto pid = iEntity.getComponent<component::Hierarchy>().parentId;
		iEntity = pid != core::UUID{0} ? iScene->findEntityByUUID(pid) : Entity{};
	}
	return iEntity;
}

// A HUD widget falls back to the active scene scripts: the HUD scene runs none.
template<typename Prepare>
void invokeCallback(const Scene* iSurface, const Scene* iActive, const Entity& iEntity,
					const std::string& iCallbackName, const Prepare& iPrepare) {
	if (iCallbackName.empty())
		return;
	if (const Entity owner = findScriptOwner(iSurface, iEntity); owner) {
		if (const auto& luaScript = owner.getComponent<component::LuaScript>();
			luaScript.instance && luaScript.instance->isValid()) {
			iPrepare(*luaScript.instance);
			std::ignore = luaScript.instance->callFunction(iCallbackName);
			return;
		}
	}
	if (iActive == nullptr || iActive == iSurface)
		return;
	for (const auto view = iActive->registry.view<component::LuaScript>(); const auto entity: view) {
		const auto& luaScript = view.get<component::LuaScript>(entity);
		if (!luaScript.instance || !luaScript.instance->isValid())
			continue;
		iPrepare(*luaScript.instance);
		if (luaScript.instance->callFunction(iCallbackName))
			return;
	}
}

void invokeLuaCallback(const Scene* iSurface, const Scene* iActive, const Entity& iEntity,
					   const std::string& iCallbackName, const uint64_t iEntityId) {
	invokeCallback(iSurface, iActive, iEntity, iCallbackName,
				   [iEntityId](const script::ScriptInstance& iInstance) -> void {
					   iInstance.setProperty("_clicked_entity", static_cast<int64_t>(iEntityId));
				   });
}

void invokeSliderCallback(const Scene* iSurface, const Scene* iActive, const Entity& iEntity,
						  const std::string& iCallbackName, const float iValue) {
	invokeCallback(iSurface, iActive, iEntity, iCallbackName,
				   [iValue](const script::ScriptInstance& iInstance) -> void {
					   iInstance.setProperty("_slider_value", iValue);
				   });
}

void invokeListCallback(const Scene* iSurface, const Scene* iActive, const Entity& iEntity,
						const std::string& iCallbackName, const std::string_view iIndexProperty, const int64_t iIndex) {
	const auto id = static_cast<int64_t>(static_cast<uint64_t>(iEntity.getUUID()));
	invokeCallback(iSurface, iActive, iEntity, iCallbackName,
				   [id, iIndexProperty, iIndex](const script::ScriptInstance& iInstance) -> void {
					   iInstance.setProperty("_list_entity", id);
					   iInstance.setProperty(std::string{iIndexProperty}, iIndex);
				   });
}

auto isInteractive(const Scene& iScene, const entt::entity iEntity) -> bool {
	if (iScene.registry.any_of<component::UiSlider, component::UiList>(iEntity))
		return true;
	const auto* button = iScene.registry.try_get<component::UiButton>(iEntity);
	return button != nullptr && button->state != component::UiButton::State::Disabled;
}

// The scroll container data of a node, nullptr when the widget is not one.
auto scrollOf(const ui::HudLayout& iLayout, const ui::HudNode& iNode) -> const ui::HudScroll* {
	return iNode.scroll == ui::HudNode::noParent ? nullptr : &iLayout.getScrolls()[iNode.scroll];
}

// The bar a point is on: the vertical one, the horizontal one, or none.
enum struct BarHit : uint8_t { None, Vertical, Horizontal };

auto barUnder(const ui::HudScroll& iScroll, const math::vec2& iPoint) -> BarHit {
	if (iScroll.geometry.verticalBar && iScroll.geometry.verticalTrack.contains(iPoint))
		return BarHit::Vertical;
	if (iScroll.geometry.horizontalBar && iScroll.geometry.horizontalTrack.contains(iPoint))
		return BarHit::Horizontal;
	return BarHit::None;
}

// The row of a list under a point, nothing over a bar or between two rows.
auto rowUnder(const ui::HudScroll& iScroll, const math::vec2& iPoint) -> std::optional<size_t> {
	if (!iScroll.isList || !iScroll.geometry.viewport.contains(iPoint))
		return std::nullopt;
	return ui::rowAt(iPoint.y() - iScroll.geometry.viewport.position.y() + iScroll.offset.y(), iScroll.rowHeight,
					 iScroll.rowSpacing, iScroll.rowCount);
}

// Move a scroll container to an offset in screen pixels (kept inside its content); true when it moved.
auto setScrollPixels(Scene& ioSurface, const ui::HudScroll& iScroll, const math::vec2& iTarget) -> bool {
	const math::vec2 target{std::clamp(iTarget.x(), 0.f, iScroll.geometry.maxOffset.x()),
							std::clamp(iTarget.y(), 0.f, iScroll.geometry.maxOffset.y())};
	if (iScroll.isList) {
		auto& list = ioSurface.registry.get<component::UiList>(iScroll.entity);
		const float wanted = target.y() / iScroll.scale;
		const bool moved = std::abs(list.scrollOffset - wanted) > 1e-4f;
		list.scrollOffset = wanted;
		return moved;
	}
	auto& view = ioSurface.registry.get<component::UiScrollView>(iScroll.entity);
	const math::vec2 wanted = target / iScroll.scale;
	const bool moved = std::abs(view.scrollOffset.x() - wanted.x()) > 1e-4f ||
					   std::abs(view.scrollOffset.y() - wanted.y()) > 1e-4f;
	view.scrollOffset = wanted;
	return moved;
}

auto wheelStepOf(const Scene& iSurface, const ui::HudScroll& iScroll) -> float {
	return iScroll.isList ? iSurface.registry.get<component::UiList>(iScroll.entity).wheelStep
						  : iSurface.registry.get<component::UiScrollView>(iScroll.entity).wheelStep;
}

// What the mouse reaches in one surface; a window under it blocks everything drawn before it.
struct Probe {
	const ui::HudNode* interactive = nullptr;
	const ui::HudNode* top = nullptr;
	const ui::HudNode* window = nullptr;

	[[nodiscard]] auto consumes() const -> bool { return interactive != nullptr || window != nullptr; }
};

auto probeSurface(const Scene& iScene, const ui::HudLayout& iLayout, const math::vec2& iPoint) -> Probe {
	Probe result;
	const auto nodes = iLayout.getNodes();
	for (const auto& node: nodes | std::views::reverse) {
		if (!node.isHit(iPoint))
			continue;
		if (result.top == nullptr)
			result.top = &node;
		const auto* scroll = scrollOf(iLayout, node);
		if (isInteractive(iScene, node.entity) || (scroll != nullptr && barUnder(*scroll, iPoint) != BarHit::None)) {
			result.interactive = &node;
			if (node.window != ui::HudNode::noParent)
				result.window = &nodes[node.window];
			return result;
		}
		if (iScene.registry.all_of<component::UiWindow>(node.entity)) {
			result.window = &node;
			return result;
		}
	}
	return result;
}

// A focused button looks hovered; only the mouse presses and clicks it here (`ui_accept` clicks through `navigate`).
void updateButton(const Scene* iActive, const Entity& iWidget, const bool iHovered, const bool iFocused,
				  const bool iPressed, const bool iClicked) {
	auto& button = iWidget.getComponent<component::UiButton>();
	if (button.state == component::UiButton::State::Disabled)
		return;
	if (!iHovered && !iFocused) {
		button.state = component::UiButton::State::Normal;
		return;
	}
	button.state = iHovered && iPressed ? component::UiButton::State::Pressed : component::UiButton::State::Hovered;
	if (iHovered && iClicked)
		invokeLuaCallback(iWidget.getScene(), iActive, iWidget, button.onClickCallback,
						  static_cast<uint64_t>(iWidget.getUUID()));
}

void updateSlider(const Scene* iActive, const Entity& iWidget, const ui::HudNode& iNode, const bool iHovered,
				  const bool iPressed, const math::vec2& iMousePos) {
	if (!iHovered || !iPressed || iNode.bounds.size.x() <= 0.f)
		return;
	auto& slider = iWidget.getComponent<component::UiSlider>();
	const float normalized = std::clamp((iMousePos.x() - iNode.bounds.position.x()) / iNode.bounds.size.x(), 0.f, 1.f);
	if (const float newValue = slider.minValue + normalized * (slider.maxValue - slider.minValue);
		std::abs(newValue - slider.value) > 1e-6f) {
		slider.value = newValue;
		invokeSliderCallback(iWidget.getScene(), iActive, iWidget, slider.onValueChangedCallback, slider.value);
	}
}

// The layout of a surface (the scene or its HUD), reused from the frame drawn or computed again.
auto layoutOf(Scene& ioSurface, const math::vec2ui& iViewportSize) -> const ui::HudLayout& {
	auto& layout = ioSurface.getHudLayout();
	if (!layout.isComputedFor(iViewportSize))
		layout.compute(ioSurface, iViewportSize, false);
	return layout;
}

// Update the widgets of a surface; only the one the mouse reaches (`iTopmost`) is hovered.
void updateWidgets(Scene& ioSurface, const Scene* iActive, const ui::HudLayout& iLayout, const ui::HudNode* iTopmost,
				   const entt::entity iFocused, const math::vec2& iMousePos, const bool iMousePressed,
				   const bool iClicked) {
	for (const auto& node: iLayout.getNodes()) {
		// A callback may destroy entities: the layout can name one that is gone.
		if (!ioSurface.registry.valid(node.entity))
			continue;
		const Entity widget{node.entity, &ioSurface};
		const bool hovered = &node == iTopmost;
		if (widget.hasComponent<component::UiButton>())
			updateButton(iActive, widget, hovered, node.entity == iFocused, iMousePressed, iClicked);
		if (widget.hasComponent<component::UiSlider>())
			updateSlider(iActive, widget, node, hovered, iMousePressed, iMousePos);
		if (const auto* scroll = scrollOf(iLayout, node); scroll != nullptr && scroll->isList) {
			const auto row = hovered ? rowUnder(*scroll, iMousePos) : std::nullopt;
			widget.getComponent<component::UiList>().hoverIndex = row ? static_cast<int32_t>(*row) : -1;
		}
	}
}

}// namespace

auto UiInputSystem::raiseWindow(Scene& ioScene, const entt::entity iWindow) -> bool {
	const auto* hierarchy = ioScene.registry.try_get<component::Hierarchy>(iWindow);
	if (hierarchy == nullptr || hierarchy->parentId == core::UUID{0})
		return false;
	const Entity parent = ioScene.findEntityByUUID(hierarchy->parentId);
	if (!parent)
		return false;
	auto& siblings = parent.getComponent<component::Hierarchy>().childrenIds;
	const auto self = std::ranges::find(siblings, ioScene.registry.get<component::ID>(iWindow).id);
	if (self == siblings.end() || std::next(self) == siblings.end())
		return false;
	std::ranges::rotate(self, std::next(self), siblings.end());
	return true;
}

namespace {

// The title bar of a window: where a click drags it, closes it, or does neither.
auto titleBarOf(const ui::HudNode& iNode, const component::UiWindow& iWindow) -> component::UiRect::Bounds {
	return {.position = iNode.bounds.position,
			.size = {iNode.bounds.size.x(), std::min(iWindow.titleBarHeight * iNode.scale, iNode.bounds.size.y())}};
}

void closeWindow(Scene& ioSurface, const Scene* iActive, const Entity& iWindow) {
	invokeLuaCallback(&ioSurface, iActive, iWindow, iWindow.getComponent<component::UiWindow>().onCloseCallback,
					  static_cast<uint64_t>(iWindow.getUUID()));
	if (ioSurface.registry.valid(static_cast<entt::entity>(iWindow)))
		iWindow.getComponent<component::Visibility>().gameVisible = false;
}

// A click on a window raises it, then its title bar closes or drags it.
void clickWindow(Scene& ioSurface, Scene& ioActive, const bool iInHud, const Probe& iProbe,
				 const math::vec2& iMousePos) {
	const Entity window{iProbe.window->entity, &ioSurface};
	UiInputSystem::raiseWindow(ioSurface, iProbe.window->entity);
	if (iProbe.interactive != nullptr)
		return;
	const auto& data = window.getComponent<component::UiWindow>();
	const auto titleBar = titleBarOf(*iProbe.window, data);
	if (!titleBar.contains(iMousePos))
		return;
	const float buttonSize = titleBar.size.y();
	const component::UiRect::Bounds closeButton{
			.position = {titleBar.position.x() + titleBar.size.x() - buttonSize, titleBar.position.y()},
			.size = {buttonSize, buttonSize}};
	if (data.closable && closeButton.contains(iMousePos)) {
		closeWindow(ioSurface, &ioActive, window);
		return;
	}
	if (!data.draggable)
		return;
	auto& state = ioActive.getUiInputState();
	state.dragWindow = iProbe.window->entity;
	state.dragInHud = iInHud;
	state.dragGrab = iMousePos - iProbe.window->bounds.position;
}

// Move the dragged window under the mouse, inside the screen; false once the drag is over.
auto dragWindow(Scene& ioActive, Scene* iHud, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
				const bool iMousePressed) -> bool {
	auto& state = ioActive.getUiInputState();
	if (state.dragWindow == entt::null)
		return false;
	Scene& surface = state.dragInHud && iHud != nullptr ? *iHud : ioActive;
	const auto* node = surface.registry.valid(state.dragWindow) && iMousePressed
							   ? layoutOf(surface, iViewportSize).findNode(state.dragWindow)
							   : nullptr;
	if (node == nullptr) {
		state.dragWindow = entt::null;
		return false;
	}
	const math::vec2 screen{static_cast<float>(iViewportSize.x()), static_cast<float>(iViewportSize.y())};
	const math::vec2 wanted = iMousePos - state.dragGrab;
	const math::vec2 target{std::clamp(wanted.x(), 0.f, std::max(0.f, screen.x() - node->bounds.size.x())),
							std::clamp(wanted.y(), 0.f, std::max(0.f, screen.y() - node->bounds.size.y()))};
	surface.registry.get<component::UiRect>(state.dragWindow).anchorOffset +=
			(target - node->bounds.position) / node->scale;
	return true;
}

// Move the thumb of a bar so it sits `iGrab` pixels before the mouse along the track.
void dragThumb(Scene& ioSurface, const ui::HudScroll& iScroll, const bool iVertical, const math::vec2& iMousePos,
			   const float iGrab) {
	const auto& geometry = iScroll.geometry;
	math::vec2 target = iScroll.offset;
	if (iVertical) {
		const float start = iMousePos.y() - geometry.verticalTrack.position.y() - iGrab;
		target.y() = ui::offsetFromThumb(geometry.verticalTrack.size.y(), geometry.viewport.size.y(),
										 geometry.content.y(), start, iScroll.minThumb);
	} else {
		const float start = iMousePos.x() - geometry.horizontalTrack.position.x() - iGrab;
		target.x() = ui::offsetFromThumb(geometry.horizontalTrack.size.x(), geometry.viewport.size.x(),
										 geometry.content.x(), start, iScroll.minThumb);
	}
	setScrollPixels(ioSurface, iScroll, target);
}

// A click on a scroll bar grabs its thumb (the track jumps to the click first); a click on a list row selects it.
void clickScroll(Scene& ioSurface, Scene& ioActive, const bool iInHud, const ui::HudScroll& iScroll,
				 const math::vec2& iMousePos) {
	if (const auto bar = barUnder(iScroll, iMousePos); bar != BarHit::None) {
		const bool vertical = bar == BarHit::Vertical;
		const auto thumb = vertical ? iScroll.verticalThumb() : iScroll.horizontalThumb();
		const float along = vertical ? iMousePos.y() - thumb.position.y() : iMousePos.x() - thumb.position.x();
		const float half = (vertical ? thumb.size.y() : thumb.size.x()) * 0.5f;
		const float grab = thumb.contains(iMousePos) ? along : half;
		auto& state = ioActive.getUiInputState();
		state.dragScroll = iScroll.entity;
		state.dragScrollInHud = iInHud;
		state.dragScrollVertical = vertical;
		state.dragScrollGrab = grab;
		dragThumb(ioSurface, iScroll, vertical, iMousePos, grab);
		return;
	}
	const auto row = rowUnder(iScroll, iMousePos);
	if (!row)
		return;
	auto& list = ioSurface.registry.get<component::UiList>(iScroll.entity);
	if (!list.select(static_cast<int64_t>(*row)))
		return;
	const std::string callback = list.onSelectCallback;
	invokeListCallback(&ioSurface, &ioActive, Entity{iScroll.entity, &ioSurface}, callback, "_selected_index",
					   static_cast<int64_t>(*row) + 1);
}

// Keep a thumb under the mouse while the button is down; false once the drag is over.
auto dragScrollBar(Scene& ioActive, Scene* iHud, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
				   const bool iMousePressed) -> bool {
	auto& state = ioActive.getUiInputState();
	if (state.dragScroll == entt::null)
		return false;
	Scene& surface = state.dragScrollInHud && iHud != nullptr ? *iHud : ioActive;
	const auto& layout = layoutOf(surface, iViewportSize);
	const auto* node =
			surface.registry.valid(state.dragScroll) && iMousePressed ? layout.findNode(state.dragScroll) : nullptr;
	const auto* scroll = node != nullptr ? scrollOf(layout, *node) : nullptr;
	if (scroll == nullptr) {
		state.dragScroll = entt::null;
		return false;
	}
	dragThumb(surface, *scroll, state.dragScrollVertical, iMousePos, state.dragScrollGrab);
	return true;
}

// Scroll the container under the mouse (or the first one above it that can move) by wheel notches.
auto scrollWithWheel(Scene& ioSurface, const ui::HudLayout& iLayout, const ui::HudNode* iTop, const math::vec2& iWheel)
		-> bool {
	const auto nodes = iLayout.getNodes();
	for (const auto* node = iTop; node != nullptr;
		 node = node->parent == ui::HudNode::noParent ? nullptr : &nodes[node->parent]) {
		const auto* scroll = scrollOf(iLayout, *node);
		if (scroll == nullptr)
			continue;
		const float step = wheelStepOf(ioSurface, *scroll) * scroll->scale;
		math::vec2 delta{-iWheel.x() * step, -iWheel.y() * step};
		if (scroll->geometry.maxOffset.y() <= 0.f && delta.x() == 0.f)
			std::swap(delta.x(), delta.y());
		if ((delta.x() != 0.f && scroll->geometry.maxOffset.x() > 0.f) ||
			(delta.y() != 0.f && scroll->geometry.maxOffset.y() > 0.f))
			return setScrollPixels(ioSurface, *scroll, scroll->offset + delta);
	}
	return false;
}

// The nearest widget at or above the one the mouse is on that carries a tooltip.
auto findTooltipWidget(const Scene& iScene, const ui::HudLayout& iLayout, const ui::HudNode* iTop) -> entt::entity {
	const auto nodes = iLayout.getNodes();
	for (const auto* node = iTop; node != nullptr;
		 node = node->parent == ui::HudNode::noParent ? nullptr : &nodes[node->parent]) {
		if (iScene.registry.all_of<component::UiTooltip>(node->entity))
			return node->entity;
	}
	return entt::null;
}

void trackTooltip(Scene::UiInputState& ioState, const entt::entity iWidget, const bool iInHud, const bool iPressed,
				  const float iDeltaTime) {
	if (iWidget != ioState.tooltipWidget || iInHud != ioState.tooltipInHud) {
		ioState.tooltipWidget = iWidget;
		ioState.tooltipInHud = iInHud;
		ioState.hoverTime = 0.f;
	} else
		ioState.hoverTime = iPressed ? 0.f : ioState.hoverTime + iDeltaTime;
}

enum struct Direction : uint8_t { None, Up, Down, Left, Right };

constexpr std::array g_directions{Direction::Up, Direction::Down, Direction::Left, Direction::Right};
// Seconds a direction is held before it repeats, then between two repeats.
constexpr float g_repeatDelay = 0.4f;
constexpr float g_repeatInterval = 0.1f;

auto actionOf(const Direction iDirection) -> std::string_view {
	switch (iDirection) {
		case Direction::Up:
			return "ui_up";
		case Direction::Down:
			return "ui_down";
		case Direction::Left:
			return "ui_left";
		case Direction::Right:
			return "ui_right";
		case Direction::None:
			break;
	}
	return {};
}

auto neighbourOf(const component::UiFocus& iFocus, const Direction iDirection) -> core::UUID {
	switch (iDirection) {
		case Direction::Up:
			return iFocus.up;
		case Direction::Down:
			return iFocus.down;
		case Direction::Left:
			return iFocus.left;
		case Direction::Right:
			return iFocus.right;
		case Direction::None:
			break;
	}
	return core::UUID{0};
}

// The edges of the frame of the actions, and the direction to move this frame (a press or a repeat).
struct NavInput {
	Direction direction = Direction::None;
	bool accept = false;
	bool cancel = false;

	[[nodiscard]] auto startsNavigation() const -> bool { return direction != Direction::None || accept; }
};

auto readNavInput(Scene::UiInputState& ioState, const input::ActionFrame& iActions, const float iDeltaTime)
		-> NavInput {
	const auto pressed = [&iActions](const std::string_view iName) -> bool {
		return iActions.has(iName) && iActions.isJustPressed(iName);
	};
	NavInput result{.accept = pressed("ui_accept"), .cancel = pressed("ui_cancel")};
	for (const auto direction: g_directions) {
		if (!pressed(actionOf(direction)))
			continue;
		result.direction = direction;
		ioState.repeatDirection = static_cast<uint8_t>(direction);
		ioState.repeatTime = 0.f;
		return result;
	}
	if (ioState.repeatDirection == static_cast<uint8_t>(Direction::None))
		return result;
	const auto held = static_cast<Direction>(ioState.repeatDirection);
	if (const auto name = actionOf(held); !iActions.has(name) || !iActions.isHeld(name)) {
		ioState.repeatDirection = static_cast<uint8_t>(Direction::None);
		return result;
	}
	ioState.repeatTime += iDeltaTime;
	if (ioState.repeatTime >= g_repeatDelay) {
		result.direction = held;
		ioState.repeatTime -= g_repeatInterval;
	}
	return result;
}

// One widget that can take the focus.
struct Candidate {
	Scene* surface = nullptr;
	const ui::HudNode* node = nullptr;
	bool inHud = false;
};

// The scene and its HUD asset, with their layouts of the frame.
struct FocusSurfaces {
	Scene* active = nullptr;
	Scene* hud = nullptr;
	const ui::HudLayout* activeLayout = nullptr;
	const ui::HudLayout* hudLayout = nullptr;

	[[nodiscard]] auto surfaceOf(const bool iInHud) const -> Scene* { return iInHud ? hud : active; }

	[[nodiscard]] auto layoutFor(const bool iInHud) const -> const ui::HudLayout* {
		return iInHud ? hudLayout : activeLayout;
	}
};

// The window drawn last (the HUD's over the scene's): the focus stays in it.
struct TopWindow {
	Scene* surface = nullptr;
	const ui::HudNode* node = nullptr;
};

auto findTopWindow(const FocusSurfaces& iSurfaces) -> TopWindow {
	for (const bool inHud: {true, false}) {
		Scene* const surface = iSurfaces.surfaceOf(inHud);
		if (surface == nullptr)
			continue;
		for (const auto& node: iSurfaces.layoutFor(inHud)->getNodes() | std::views::reverse) {
			if (surface->registry.all_of<component::UiWindow>(node.entity))
				return {.surface = surface, .node = &node};
		}
	}
	return {};
}

auto isFocusable(const Scene& iSurface, const entt::entity iEntity) -> bool {
	if (!isInteractive(iSurface, iEntity))
		return false;
	const auto* focus = iSurface.registry.try_get<component::UiFocus>(iEntity);
	return focus == nullptr || focus->focusable;
}

auto isCutAway(const ui::HudNode& iNode) -> bool {
	if (!iNode.clipped)
		return false;
	const auto visible = iNode.bounds.intersect(iNode.clip);
	return visible.size.x() <= 0.f || visible.size.y() <= 0.f;
}

// A widget cut away by its clipping panel cannot be reached; one scrolled out of a scroll view can, the focus scrolls it in.
auto isReachable(const ui::HudNode& iNode, const ui::HudLayout& iLayout) -> bool {
	const auto nodes = iLayout.getNodes();
	for (const auto* node = &iNode; node != nullptr;
		 node = node->scrollAncestor == ui::HudNode::noParent ? nullptr : &nodes[node->scrollAncestor]) {
		if (!isCutAway(*node))
			return true;
	}
	return false;
}

auto isInTopWindow(const ui::HudNode& iNode, const ui::HudLayout& iLayout, const TopWindow& iTop) -> bool {
	if (iTop.node == nullptr)
		return true;
	return iNode.window != ui::HudNode::noParent && &iLayout.getNodes()[iNode.window] == iTop.node;
}

auto canHoldFocus(const Scene& iSurface, const ui::HudNode& iNode, const ui::HudLayout& iLayout, const TopWindow& iTop)
		-> bool {
	return isFocusable(iSurface, iNode.entity) && isReachable(iNode, iLayout) && isInTopWindow(iNode, iLayout, iTop);
}

auto locateFocus(const FocusSurfaces& iSurfaces, const Scene::UiInputState& iState, const TopWindow& iTop)
		-> std::optional<Candidate> {
	Scene* const surface = iSurfaces.surfaceOf(iState.focusInHud);
	if (iState.focus == entt::null || surface == nullptr || !surface->registry.valid(iState.focus))
		return std::nullopt;
	const auto& layout = *iSurfaces.layoutFor(iState.focusInHud);
	const auto* node = layout.findNode(iState.focus);
	if (node == nullptr || !canHoldFocus(*surface, *node, layout, iTop))
		return std::nullopt;
	return Candidate{.surface = surface, .node = node, .inHud = iState.focusInHud};
}

// The focusable widgets of the scene then of its HUD, in layout order.
auto collectCandidates(const FocusSurfaces& iSurfaces, const TopWindow& iTop) -> std::vector<Candidate> {
	std::vector<Candidate> result;
	for (const bool inHud: {false, true}) {
		Scene* const surface = iSurfaces.surfaceOf(inHud);
		if (surface == nullptr)
			continue;
		const auto& layout = *iSurfaces.layoutFor(inHud);
		for (const auto& node: layout.getNodes()) {
			if (canHoldFocus(*surface, node, layout, iTop))
				result.push_back({.surface = surface, .node = &node, .inHud = inHud});
		}
	}
	return result;
}

auto findInitial(const std::span<const Candidate> iCandidates) -> const Candidate* {
	for (const auto& candidate: iCandidates) {
		const auto* focus = candidate.surface->registry.try_get<component::UiFocus>(candidate.node->entity);
		if (focus != nullptr && focus->initial)
			return &candidate;
	}
	return nullptr;
}

auto pickInitial(const std::span<const Candidate> iCandidates) -> const Candidate* {
	if (const auto* initial = findInitial(iCandidates); initial != nullptr)
		return initial;
	return iCandidates.empty() ? nullptr : &iCandidates.front();
}

auto findByUuid(const std::span<const Candidate> iCandidates, const core::UUID iUuid) -> const Candidate* {
	for (const auto& candidate: iCandidates) {
		if (Entity{candidate.node->entity, candidate.surface}.getUUID() == iUuid)
			return &candidate;
	}
	return nullptr;
}

// The canvas above a widget: its menu.
auto canvasOf(const Candidate& iCandidate) -> Entity {
	Entity entity{iCandidate.node->entity, iCandidate.surface};
	while (entity && !entity.hasComponent<component::Canvas>())
		entity = iCandidate.surface->findEntityByUUID(entity.getComponent<component::Hierarchy>().parentId);
	return entity;
}

// Distance travelled in the direction, and drift sideways, from one centre to another.
auto axesOf(const math::vec2& iDelta, const Direction iDirection) -> std::pair<float, float> {
	switch (iDirection) {
		case Direction::Up:
			return {-iDelta.y(), iDelta.x()};
		case Direction::Down:
			return {iDelta.y(), iDelta.x()};
		case Direction::Left:
			return {-iDelta.x(), iDelta.y()};
		case Direction::Right:
			return {iDelta.x(), iDelta.y()};
		case Direction::None:
			break;
	}
	return {0.f, 0.f};
}

// The nearest widget past the current one in a direction; a sideways drift weighs double.
auto pickInDirection(const std::span<const Candidate> iCandidates, const Candidate& iCurrent,
					 const Direction iDirection) -> const Candidate* {
	const math::vec2 from = iCurrent.node->bounds.getCenter();
	const Candidate* best = nullptr;
	float bestScore = std::numeric_limits<float>::max();
	for (const auto& candidate: iCandidates) {
		if (candidate.node == iCurrent.node)
			continue;
		const auto [along, across] = axesOf(candidate.node->bounds.getCenter() - from, iDirection);
		if (along <= 0.5f)
			continue;
		if (const float score = along + 2.f * std::abs(across); score < bestScore) {
			bestScore = score;
			best = &candidate;
		}
	}
	return best;
}

// With nothing ahead, the widget at the far end on the opposite side (same menu as the current one).
auto pickWrapped(const std::span<const Candidate> iCandidates, const Candidate& iCurrent, const Direction iDirection,
				 const Entity& iCanvas) -> const Candidate* {
	const math::vec2 from = iCurrent.node->bounds.getCenter();
	const Candidate* best = nullptr;
	float bestScore = std::numeric_limits<float>::max();
	for (const auto& candidate: iCandidates) {
		if (candidate.node == iCurrent.node || canvasOf(candidate) != iCanvas)
			continue;
		const auto [along, across] = axesOf(candidate.node->bounds.getCenter() - from, iDirection);
		if (const float score = along + 2.f * std::abs(across); score < bestScore) {
			bestScore = score;
			best = &candidate;
		}
	}
	return best;
}

void invokeFocusCallback(const Scene& iSurface, const Scene& iActive, const Entity& iWidget,
						 const std::string& iCallbackName) {
	const auto id = static_cast<int64_t>(static_cast<uint64_t>(iWidget.getUUID()));
	invokeCallback(&iSurface, &iActive, iWidget, iCallbackName, [id](const script::ScriptInstance& iInstance) -> void {
		iInstance.setProperty("_focused_entity", id);
	});
}

// Put the focus on a widget; a new one runs its `onFocusCallback`.
void setFocusState(Scene& ioActive, Scene& ioSurface, const bool iInHud, const entt::entity iWidget,
				   const bool iVisible) {
	auto& state = ioActive.getUiInputState();
	const bool changed = state.focus != iWidget || state.focusInHud != iInHud;
	state.focus = iWidget;
	state.focusInHud = iInHud;
	state.focusVisible = iVisible;
	if (!changed)
		return;
	const auto* focus = ioSurface.registry.try_get<component::UiFocus>(iWidget);
	if (focus == nullptr || focus->onFocusCallback.empty())
		return;
	const std::string callback = focus->onFocusCallback;
	invokeFocusCallback(ioSurface, ioActive, Entity{iWidget, &ioSurface}, callback);
}

void focusCandidate(Scene& ioActive, const Candidate& iCandidate, const bool iVisible) {
	setFocusState(ioActive, *iCandidate.surface, iCandidate.inHud, iCandidate.node->entity, iVisible);
}

void activateFocus(Scene& ioActive, const Candidate& iCurrent) {
	const auto* button = iCurrent.surface->registry.try_get<component::UiButton>(iCurrent.node->entity);
	if (button == nullptr)
		return;
	const Entity widget{iCurrent.node->entity, iCurrent.surface};
	const std::string callback = button->onClickCallback;
	invokeLuaCallback(iCurrent.surface, &ioActive, widget, callback, static_cast<uint64_t>(widget.getUUID()));
}

void adjustFocusedSlider(Scene& ioActive, const Candidate& iCurrent, const float iSign) {
	const Entity widget{iCurrent.node->entity, iCurrent.surface};
	auto& slider = widget.getComponent<component::UiSlider>();
	if (slider.maxValue <= slider.minValue)
		return;
	const float step = slider.step > 0.f ? slider.step : (slider.maxValue - slider.minValue) / 20.f;
	const float newValue = std::clamp(slider.value + iSign * step, slider.minValue, slider.maxValue);
	if (std::abs(newValue - slider.value) <= 1e-6f)
		return;
	slider.value = newValue;
	const std::string callback = slider.onValueChangedCallback;
	invokeSliderCallback(iCurrent.surface, &ioActive, widget, callback, newValue);
}

// Move the selection of the focused list by a row; false when it is not a list or the selection is at its end.
auto stepListSelection(Scene& ioActive, const Candidate& iCurrent, const int64_t iStep) -> bool {
	auto* list = iCurrent.surface->registry.try_get<component::UiList>(iCurrent.node->entity);
	if (list == nullptr)
		return false;
	int64_t wanted = list->selectedIndex + iStep;
	if (list->selectedIndex < 0)
		wanted = iStep > 0 ? 0 : static_cast<int64_t>(list->itemCount) - 1;
	if (wanted < 0 || wanted >= static_cast<int64_t>(list->itemCount) || !list->select(wanted))
		return false;
	const std::string callback = list->onSelectCallback;
	invokeListCallback(iCurrent.surface, &ioActive, Entity{iCurrent.node->entity, iCurrent.surface}, callback,
					   "_selected_index", wanted + 1);
	return true;
}

void closeTopWindow(Scene& ioActive, const TopWindow& iTop) {
	if (iTop.surface == nullptr)
		return;
	const Entity window{iTop.node->entity, iTop.surface};
	if (window.getComponent<component::UiWindow>().closable)
		closeWindow(*iTop.surface, &ioActive, window);
}

// The focus is lost (hidden, disabled, outside the top window...) or never was: pick the first widget, or none.
void acquireFocus(Scene& ioActive, const FocusSurfaces& iSurfaces, const TopWindow& iTop, const bool iVisible) {
	auto& state = ioActive.getUiInputState();
	state.focus = entt::null;
	state.focusVisible = false;
	if (!iVisible)
		return;
	const auto candidates = collectCandidates(iSurfaces, iTop);
	if (const auto* first = pickInitial(candidates); first != nullptr)
		focusCandidate(ioActive, *first, true);
}

void moveFocus(Scene& ioActive, const FocusSurfaces& iSurfaces, const TopWindow& iTop, const Candidate& iCurrent,
			   const Direction iDirection) {
	const auto* focus = iCurrent.surface->registry.try_get<component::UiFocus>(iCurrent.node->entity);
	const core::UUID neighbour = focus != nullptr ? neighbourOf(*focus, iDirection) : core::UUID{0};
	const bool hasNeighbour = neighbour != core::UUID{0};
	const bool sideways = iDirection == Direction::Left || iDirection == Direction::Right;
	if (sideways && !hasNeighbour && iCurrent.surface->registry.all_of<component::UiSlider>(iCurrent.node->entity)) {
		adjustFocusedSlider(ioActive, iCurrent, iDirection == Direction::Right ? 1.f : -1.f);
		return;
	}
	if (!hasNeighbour && (iDirection == Direction::Up || iDirection == Direction::Down) &&
		stepListSelection(ioActive, iCurrent, iDirection == Direction::Down ? 1 : -1))
		return;
	const auto candidates = collectCandidates(iSurfaces, iTop);
	const Candidate* target = hasNeighbour ? findByUuid(candidates, neighbour) : nullptr;
	if (target == nullptr)
		target = pickInDirection(candidates, iCurrent, iDirection);
	if (const Entity canvas = target == nullptr ? canvasOf(iCurrent) : Entity{};
		canvas && canvas.getComponent<component::Canvas>().wrapFocus)
		target = pickWrapped(candidates, iCurrent, iDirection, canvas);
	if (target != nullptr)
		focusCandidate(ioActive, *target, true);
}

// Scroll the views above the focused widget so it is in view (once per focus change, when the focus is shown).
void revealFocus(Scene& ioActive, const Candidate& iCurrent) {
	auto& state = ioActive.getUiInputState();
	if (!state.focusVisible || state.revealedFocus == iCurrent.node->entity)
		return;
	state.revealedFocus = iCurrent.node->entity;
	const auto& layout = iCurrent.surface->getHudLayout();
	const auto nodes = layout.getNodes();
	const auto& item = iCurrent.node->bounds;
	for (size_t index = iCurrent.node->scrollAncestor; index != ui::HudNode::noParent;
		 index = nodes[index].scrollAncestor) {
		const auto& container = nodes[index];
		const auto* scroll = scrollOf(layout, container);
		if (scroll == nullptr || scroll->isList)
			continue;
		const auto& viewport = scroll->geometry.viewport;
		const math::vec2 start = item.position - viewport.position + scroll->offset;
		const math::vec2 target{
				ui::revealRange(scroll->offset.x(), viewport.size.x(), start.x(), start.x() + item.size.x()),
				ui::revealRange(scroll->offset.y(), viewport.size.y(), start.y(), start.y() + item.size.y())};
		setScrollPixels(*iCurrent.surface, *scroll, target);
	}
}

void navigateSurfaces(Scene& ioActive, const FocusSurfaces& iSurfaces, const NavInput& iInput) {
	auto& state = ioActive.getUiInputState();
	const TopWindow top = findTopWindow(iSurfaces);
	const auto current = locateFocus(iSurfaces, state, top);
	if (!current) {
		const bool wasShown = state.focusVisible;
		acquireFocus(ioActive, iSurfaces, top, wasShown || iInput.startsNavigation());
		return;
	}
	revealFocus(ioActive, *current);
	if (!iInput.startsNavigation() && !iInput.cancel)
		return;
	if (!state.focusVisible) {
		state.focusVisible = iInput.startsNavigation();
		return;
	}
	if (iInput.accept)
		activateFocus(ioActive, *current);
	else if (iInput.direction != Direction::None)
		moveFocus(ioActive, iSurfaces, top, *current, iInput.direction);
	else if (iInput.cancel)
		closeTopWindow(ioActive, top);
}

// The surfaces of a scene, laid out for the frame; the HUD handle keeps its asset alive.
auto surfacesOf(Scene& ioScene, const shared<Scene>& iHud, const math::vec2ui& iViewportSize) -> FocusSurfaces {
	Scene* const hud = iHud && iHud.get() != &ioScene ? iHud.get() : nullptr;
	return {.active = &ioScene,
			.hud = hud,
			.activeLayout = &layoutOf(ioScene, iViewportSize),
			.hudLayout = hud != nullptr ? &layoutOf(*hud, iViewportSize) : nullptr};
}

// Scroll the view holding the focus with the `ui_scroll` action (right stick, Page Up / Page Down).
void scrollWithAction(Scene& ioScene, const math::vec2ui& iViewportSize, const input::ActionFrame& iActions,
					  const float iDeltaTime) {
	static constexpr float actionScrollSpeed = 480.f;
	if (!iActions.has("ui_scroll"))
		return;
	const math::vec2 value = iActions.getVector("ui_scroll");
	const auto& state = ioScene.getUiInputState();
	if ((value.x() == 0.f && value.y() == 0.f) || state.focus == entt::null)
		return;
	const auto hudHandle = ioScene.getHud();
	Scene* const surface = state.focusInHud && hudHandle ? hudHandle.get() : &ioScene;
	if (!surface->registry.valid(state.focus))
		return;
	const auto& layout = layoutOf(*surface, iViewportSize);
	const auto* node = layout.findNode(state.focus);
	for (const auto nodes = layout.getNodes(); node != nullptr;
		 node = node->scrollAncestor == ui::HudNode::noParent ? nullptr : &nodes[node->scrollAncestor]) {
		const auto* scroll = scrollOf(layout, *node);
		if (scroll == nullptr)
			continue;
		const math::vec2 delta = value * (actionScrollSpeed * scroll->scale * iDeltaTime);
		if (setScrollPixels(*surface, *scroll, scroll->offset + delta))
			return;
	}
}

void navigateFrame(Scene& ioScene, const math::vec2ui& iViewportSize, const input::ActionFrame& iActions,
				   const float iDeltaTime) {
	scrollWithAction(ioScene, iViewportSize, iActions, iDeltaTime);
	auto& state = ioScene.getUiInputState();
	const NavInput input = readNavInput(state, iActions, iDeltaTime);
	const bool starting = !state.focusStarted;
	if (!starting && state.focus == entt::null && !input.startsNavigation())
		return;
	const auto hudHandle = ioScene.getHud();
	const auto surfaces = surfacesOf(ioScene, hudHandle, iViewportSize);
	if (starting) {
		state.focusStarted = true;
		if (const auto candidates = collectCandidates(surfaces, findTopWindow(surfaces)); !candidates.empty()) {
			if (const auto* initial = findInitial(candidates); initial != nullptr)
				focusCandidate(ioScene, *initial, true);
		}
	}
	navigateSurfaces(ioScene, surfaces, input);
}

// Ask for the rows of the lists that are in view and were never filled.
void bindVisibleRows(Scene& ioSurface, const Scene* iActive, const ui::HudLayout& iLayout) {
	std::vector<size_t> pending;
	for (const auto& scroll: iLayout.getScrolls()) {
		if (!scroll.isList || !ioSurface.registry.valid(scroll.entity))
			continue;
		auto& list = ioSurface.registry.get<component::UiList>(scroll.entity);
		if (list.onBindCallback.empty() || list.itemCount == 0)
			continue;
		list.rows.resize(list.itemCount);
		const float offset = std::clamp(list.scrollOffset * scroll.scale, 0.f, scroll.geometry.maxOffset.y());
		const auto [first, end] = ui::visibleRows(offset, scroll.geometry.viewport.size.y(), scroll.rowHeight,
												  scroll.rowSpacing, scroll.rowCount);
		pending.clear();
		for (size_t row = first; row < end; ++row) {
			if (!list.rows[row].bound) {
				list.rows[row].bound = true;
				pending.push_back(row);
			}
		}
		const std::string callback = list.onBindCallback;
		for (const auto row: pending) {
			if (!ioSurface.registry.valid(scroll.entity))
				break;
			invokeListCallback(&ioSurface, iActive, Entity{scroll.entity, &ioSurface}, callback, "_row_index",
							   static_cast<int64_t>(row) + 1);
		}
	}
}

// The mouse takes the focus where it moves or clicks, and hides its highlight.
void focusUnderMouse(Scene& ioActive, const Candidate& iHovered) {
	if (isFocusable(*iHovered.surface, iHovered.node->entity))
		focusCandidate(ioActive, iHovered, false);
}

}// namespace

void UiInputSystem::update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
						   const bool iMousePressed, const float iDeltaTime, const math::vec2& iWheel) {
	OWL_PROFILE_FUNCTION()

	if (iScene == nullptr)
		return;
	auto& state = iScene->getUiInputState();
	state.consuming = false;
	if (iScene->status != Scene::Status::Playing)
		return;

	const bool clicked = !state.wasPressed && iMousePressed;
	const bool mouseMoved = state.hasMouse && (std::abs(iMousePos.x() - state.mousePosition.x()) > 0.5f ||
											   std::abs(iMousePos.y() - state.mousePosition.y()) > 0.5f);
	state.wasPressed = iMousePressed;
	state.hasMouse = true;
	state.mousePosition = iMousePos;

	// The HUD asset is drawn over the scene: a widget of it takes the mouse before the scene's own.
	const auto hudHandle = iScene->getHud();
	Scene* const hud = hudHandle && hudHandle.get() != iScene ? hudHandle.get() : nullptr;
	const bool dragging = dragWindow(*iScene, hud, iViewportSize, iMousePos, iMousePressed) ||
						  dragScrollBar(*iScene, hud, iViewportSize, iMousePos, iMousePressed);
	const ui::HudLayout* hudLayout = hud != nullptr ? &layoutOf(*hud, iViewportSize) : nullptr;
	const auto& layout = layoutOf(*iScene, iViewportSize);
	const Probe hudProbe = hud != nullptr && !dragging ? probeSurface(*hud, *hudLayout, iMousePos) : Probe{};
	const Probe sceneProbe = dragging || hudProbe.consumes() ? Probe{} : probeSurface(*iScene, layout, iMousePos);
	if (clicked && !dragging) {
		if (hudProbe.window != nullptr)
			clickWindow(*hud, *iScene, true, hudProbe, iMousePos);
		else if (sceneProbe.window != nullptr)
			clickWindow(*iScene, *iScene, false, sceneProbe, iMousePos);
	}
	if (clicked && !dragging) {
		const bool inHud = hudProbe.interactive != nullptr;
		const Probe& probe = inHud ? hudProbe : sceneProbe;
		Scene* const surface = inHud ? hud : iScene;
		const auto& surfaceLayout = inHud ? *hudLayout : layout;
		if (const auto* scroll = probe.interactive != nullptr ? scrollOf(surfaceLayout, *probe.interactive) : nullptr;
			scroll != nullptr)
			clickScroll(*surface, *iScene, inHud, *scroll, iMousePos);
	}
	if (iWheel.x() != 0.f || iWheel.y() != 0.f) {
		const bool scrolled = hud != nullptr && scrollWithWheel(*hud, *hudLayout, hudProbe.top, iWheel);
		if (!scrolled && !hudProbe.consumes())
			scrollWithWheel(*iScene, layout, sceneProbe.top, iWheel);
	}
	if ((mouseMoved || clicked) && !dragging) {
		if (hudProbe.interactive != nullptr)
			focusUnderMouse(*iScene, {.surface = hud, .node = hudProbe.interactive, .inHud = true});
		else if (sceneProbe.interactive != nullptr)
			focusUnderMouse(*iScene, {.surface = iScene, .node = sceneProbe.interactive, .inHud = false});
	}
	navigateFrame(*iScene, iViewportSize, iScene->getActions(), iDeltaTime);
	const auto shownFocus = [&state](const bool iInHud) -> entt::entity {
		return state.focusVisible && state.focusInHud == iInHud ? state.focus : entt::null;
	};
	updateWidgets(*iScene, iScene, layout, sceneProbe.interactive, shownFocus(false), iMousePos, iMousePressed,
				  clicked);
	if (hud != nullptr)
		updateWidgets(*hud, iScene, *hudLayout, hudProbe.interactive, shownFocus(true), iMousePos, iMousePressed,
					  clicked);
	state.consuming = dragging || hudProbe.consumes() || sceneProbe.consumes();
	bindVisibleRows(*iScene, iScene, layout);
	if (hud != nullptr)
		bindVisibleRows(*hud, iScene, *hudLayout);

	entt::entity tooltip = entt::null;
	bool tooltipInHud = false;
	if (!dragging) {
		if (hud != nullptr)
			tooltip = findTooltipWidget(*hud, *hudLayout, hudProbe.top);
		tooltipInHud = tooltip != entt::null;
		if (!tooltipInHud)
			tooltip = findTooltipWidget(*iScene, layout, sceneProbe.top);
	}
	trackTooltip(state, tooltip, tooltipInHud, iMousePressed, iDeltaTime);

	// Drawing lays the HUD out again: the next update reuses that layout, or recomputes one when no frame was drawn.
	iScene->getHudLayout().invalidate();
	if (hud != nullptr)
		hud->getHudLayout().invalidate();
}

void UiInputSystem::navigate(Scene& ioScene, const math::vec2ui& iViewportSize, const input::ActionFrame& iActions,
							 const float iDeltaTime) {
	OWL_PROFILE_FUNCTION()

	navigateFrame(ioScene, iViewportSize, iActions, iDeltaTime);
	ioScene.getHudLayout().invalidate();
	if (const auto hud = ioScene.getHud(); hud && hud.get() != &ioScene)
		hud->getHudLayout().invalidate();
}

auto UiInputSystem::setFocus(Scene& ioScene, const Entity& iWidget) -> bool {
	Scene* const surface = iWidget.getScene();
	if (!iWidget || surface == nullptr || !isFocusable(*surface, static_cast<entt::entity>(iWidget)))
		return false;
	setFocusState(ioScene, *surface, surface != &ioScene, static_cast<entt::entity>(iWidget), true);
	return true;
}

auto UiInputSystem::getFocus(Scene& ioScene) -> Entity {
	const auto& state = ioScene.getUiInputState();
	if (state.focus == entt::null)
		return {};
	const auto hud = ioScene.getHud();
	Scene* const surface = state.focusInHud ? hud.get() : &ioScene;
	if (surface == nullptr || !surface->registry.valid(state.focus))
		return {};
	return Entity{state.focus, surface};
}

void UiInputSystem::clearFocus(Scene& ioScene) {
	auto& state = ioScene.getUiInputState();
	state.focus = entt::null;
	state.focusVisible = false;
}

auto UiInputSystem::isUIConsuming(const Scene& iScene) -> bool { return iScene.getUiInputState().consuming; }

void UiInputSystem::reset(Scene& ioScene) { ioScene.getUiInputState() = {}; }

}// namespace owl::scene
