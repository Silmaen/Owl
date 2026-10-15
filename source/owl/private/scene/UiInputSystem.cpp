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

void invokeLuaCallback(const Scene* iScene, const Entity& iEntity, const std::string& iCallbackName,
					   const uint64_t iEntityId) {
	if (iCallbackName.empty())
		return;
	const Entity owner = findScriptOwner(iScene, iEntity);
	if (!owner)
		return;
	if (const auto& luaScript = owner.getComponent<component::LuaScript>();
		luaScript.instance && luaScript.instance->isValid()) {
		luaScript.instance->setProperty("_clicked_entity", static_cast<int64_t>(iEntityId));
		std::ignore = luaScript.instance->callFunction(iCallbackName);
	}
}

void invokeSliderCallback(const Scene* iScene, const Entity& iEntity, const std::string& iCallbackName,
						  const float iValue) {
	if (iCallbackName.empty())
		return;
	const Entity owner = findScriptOwner(iScene, iEntity);
	if (!owner)
		return;
	if (const auto& luaScript = owner.getComponent<component::LuaScript>();
		luaScript.instance && luaScript.instance->isValid()) {
		luaScript.instance->setProperty("_slider_value", iValue);
		std::ignore = luaScript.instance->callFunction(iCallbackName);
	}
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

void updateButton(const Scene* iScene, const Entity& iWidget, const bool iHovered, const bool iPressed,
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
		invokeLuaCallback(iScene, iWidget, button.onClickCallback, static_cast<uint64_t>(iWidget.getUUID()));
}

void updateSlider(const Scene* iScene, const Entity& iWidget, const ui::HudNode& iNode, const bool iHovered,
				  const bool iPressed, const math::vec2& iMousePos) {
	if (!iHovered || !iPressed || iNode.bounds.size.x() <= 0.f)
		return;
	auto& slider = iWidget.getComponent<component::UiSlider>();
	const float normalized = std::clamp((iMousePos.x() - iNode.bounds.position.x()) / iNode.bounds.size.x(), 0.f, 1.f);
	if (const float newValue = slider.minValue + normalized * (slider.maxValue - slider.minValue);
		std::abs(newValue - slider.value) > 1e-6f) {
		slider.value = newValue;
		invokeSliderCallback(iScene, iWidget, slider.onValueChangedCallback, slider.value);
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

	auto& layout = iScene->getHudLayout();
	if (!layout.isComputedFor(iViewportSize))
		layout.compute(*iScene, iViewportSize, false);
	const auto* topmost = findTopmostInteractive(*iScene, layout, iMousePos);
	for (const auto& node: layout.getNodes()) {
		// A callback may destroy entities: the layout can name one that is gone.
		if (!iScene->registry.valid(node.entity))
			continue;
		const Entity widget{node.entity, iScene};
		const bool hovered = &node == topmost;
		if (widget.hasComponent<component::UiButton>())
			updateButton(iScene, widget, hovered, iMousePressed, clicked);
		if (widget.hasComponent<component::UiSlider>())
			updateSlider(iScene, widget, node, hovered, iMousePressed, iMousePos);
	}
	state.consuming = topmost != nullptr;
	// Drawing lays the HUD out again: the next update reuses that layout, or recomputes one when no frame was drawn.
	layout.invalidate();
}

auto UiInputSystem::isUIConsuming(const Scene& iScene) -> bool { return iScene.getUiInputState().consuming; }

void UiInputSystem::reset(Scene& ioScene) { ioScene.getUiInputState() = {}; }

}// namespace owl::scene
