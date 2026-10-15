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
#include "scene/component/Hierarchy.h"
#include "scene/component/ID.h"
#include "scene/component/LuaScript.h"
#include "scene/component/UiButton.h"
#include "scene/component/UiSlider.h"
#include "scene/component/UiTooltip.h"
#include "scene/component/UiWindow.h"
#include "scene/component/Visibility.h"
#include "scene/ui/HudLayout.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string>
#include <tuple>

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

auto isInteractive(const Scene& iScene, const entt::entity iEntity) -> bool {
	if (iScene.registry.all_of<component::UiSlider>(iEntity))
		return true;
	const auto* button = iScene.registry.try_get<component::UiButton>(iEntity);
	return button != nullptr && button->state != component::UiButton::State::Disabled;
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
		if (isInteractive(iScene, node.entity)) {
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

void updateButton(const Scene* iActive, const Entity& iWidget, const bool iHovered, const bool iPressed,
				  const bool iClicked) {
	auto& button = iWidget.getComponent<component::UiButton>();
	if (button.state == component::UiButton::State::Disabled)
		return;
	if (!iHovered) {
		button.state = component::UiButton::State::Normal;
		return;
	}
	button.state = iPressed ? component::UiButton::State::Pressed : component::UiButton::State::Hovered;
	if (iClicked)
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
				   const math::vec2& iMousePos, const bool iMousePressed, const bool iClicked) {
	for (const auto& node: iLayout.getNodes()) {
		// A callback may destroy entities: the layout can name one that is gone.
		if (!ioSurface.registry.valid(node.entity))
			continue;
		const Entity widget{node.entity, &ioSurface};
		const bool hovered = &node == iTopmost;
		if (widget.hasComponent<component::UiButton>())
			updateButton(iActive, widget, hovered, iMousePressed, iClicked);
		if (widget.hasComponent<component::UiSlider>())
			updateSlider(iActive, widget, node, hovered, iMousePressed, iMousePos);
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

}// namespace

void UiInputSystem::update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
						   const bool iMousePressed, const float iDeltaTime) {
	OWL_PROFILE_FUNCTION()

	if (iScene == nullptr)
		return;
	auto& state = iScene->getUiInputState();
	state.consuming = false;
	if (iScene->status != Scene::Status::Playing)
		return;

	const bool clicked = !state.wasPressed && iMousePressed;
	state.wasPressed = iMousePressed;
	state.hasMouse = true;
	state.mousePosition = iMousePos;

	// The HUD asset is drawn over the scene: a widget of it takes the mouse before the scene's own.
	const auto hudHandle = iScene->getHud();
	Scene* const hud = hudHandle && hudHandle.get() != iScene ? hudHandle.get() : nullptr;
	const bool dragging = dragWindow(*iScene, hud, iViewportSize, iMousePos, iMousePressed);
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
	updateWidgets(*iScene, iScene, layout, sceneProbe.interactive, iMousePos, iMousePressed, clicked);
	if (hud != nullptr)
		updateWidgets(*hud, iScene, *hudLayout, hudProbe.interactive, iMousePos, iMousePressed, clicked);
	state.consuming = dragging || hudProbe.consumes() || sceneProbe.consumes();

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

auto UiInputSystem::isUIConsuming(const Scene& iScene) -> bool { return iScene.getUiInputState().consuming; }

void UiInputSystem::reset(Scene& ioScene) { ioScene.getUiInputState() = {}; }

}// namespace owl::scene
