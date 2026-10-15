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
#include "scene/component/LuaScript.h"
#include "scene/component/UiButton.h"
#include "scene/component/UiSlider.h"
#include "scene/ui/HudLayout.h"
#include "script/ScriptInstance.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

// The topmost button or slider under the mouse takes it: labels and panels drawn over it do not.
auto findTopmostInteractive(const Scene& iScene, const ui::HudLayout& iLayout, const math::vec2& iPoint)
		-> const ui::HudNode* {
	for (const auto& node: iLayout.getNodes() | std::views::reverse) {
		if (node.bounds.contains(iPoint) && isInteractive(iScene, node.entity))
			return &node;
	}
	return nullptr;
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

void UiInputSystem::update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
						   const bool iMousePressed) {
	OWL_PROFILE_FUNCTION()

	if (iScene == nullptr)
		return;
	auto& state = iScene->getUiInputState();
	state.consuming = false;
	if (iScene->status != Scene::Status::Playing)
		return;

	const bool clicked = !state.wasPressed && iMousePressed;
	state.wasPressed = iMousePressed;

	// The HUD asset is drawn over the scene: a widget of it takes the mouse before the scene's own.
	const auto hud = iScene->getHud();
	const bool hasHud = hud && hud.get() != iScene;
	const ui::HudLayout* hudLayout = hasHud ? &layoutOf(*hud, iViewportSize) : nullptr;
	const auto* hudTopmost = hasHud ? findTopmostInteractive(*hud, *hudLayout, iMousePos) : nullptr;
	const auto& layout = layoutOf(*iScene, iViewportSize);
	const auto* topmost = hudTopmost != nullptr ? nullptr : findTopmostInteractive(*iScene, layout, iMousePos);
	updateWidgets(*iScene, iScene, layout, topmost, iMousePos, iMousePressed, clicked);
	if (hasHud)
		updateWidgets(*hud, iScene, *hudLayout, hudTopmost, iMousePos, iMousePressed, clicked);
	state.consuming = topmost != nullptr || hudTopmost != nullptr;
	// Drawing lays the HUD out again: the next update reuses that layout, or recomputes one when no frame was drawn.
	iScene->getHudLayout().invalidate();
	if (hasHud)
		hud->getHudLayout().invalidate();
}

auto UiInputSystem::isUIConsuming(const Scene& iScene) -> bool { return iScene.getUiInputState().consuming; }

void UiInputSystem::reset(Scene& ioScene) { ioScene.getUiInputState() = {}; }

}// namespace owl::scene
