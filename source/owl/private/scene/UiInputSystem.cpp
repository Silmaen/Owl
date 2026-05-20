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
#include "scene/component/LuaScript.h"
#include "scene/component/UiButton.h"
#include "scene/component/UiRect.h"
#include "scene/component/UiSlider.h"
#include "script/ScriptInstance.h"

#include <algorithm>

namespace owl::scene {

bool UiInputSystem::s_consuming = false;
bool UiInputSystem::s_wasPressed = false;

namespace {
auto hitTestRect(const component::UiRect& iRect, const math::vec2& iParentSize, const math::vec2& iPoint) -> bool {
	const math::vec2 center = iRect.computePosition(iParentSize);
	const float halfW = iRect.size.x() * 0.5f;
	const float halfH = iRect.size.y() * 0.5f;
	return iPoint.x() >= center.x() - halfW && iPoint.x() <= center.x() + halfW && iPoint.y() >= center.y() - halfH &&
		   iPoint.y() <= center.y() + halfH;
}

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

}// namespace

void UiInputSystem::update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
						   const bool iMousePressed) {
	OWL_PROFILE_FUNCTION()

	s_consuming = false;

	if (iScene == nullptr || iScene->status != Scene::Status::Playing)
		return;

	const auto vpSize = math::vec2{static_cast<float>(iViewportSize.x()), static_cast<float>(iViewportSize.y())};
	const bool clicked = !s_wasPressed && iMousePressed;
	s_wasPressed = iMousePressed;

	// Iterate Canvas entities (sorted by sortOrder, highest first for input priority).
	struct CanvasEntry {
		entt::entity entity;
		int32_t sortOrder;
	};
	std::vector<CanvasEntry> canvases;
	for (const auto view = iScene->registry.view<component::Canvas>(); const auto entity: view)
		canvases.push_back({entity, view.get<component::Canvas>(entity).sortOrder});
	std::ranges::sort(canvases, [](const auto& iA, const auto& iB) -> auto { return iA.sortOrder > iB.sortOrder; });

	for (const auto& [canvasEnt, sortOrder]: canvases) {
		const Entity canvasEntity{canvasEnt, iScene};
		if (!iScene->isEffectivelyVisible(canvasEntity, false))
			continue;

		for (const auto& [parentId, childrenIds] = canvasEntity.getComponent<component::Hierarchy>();
			 const auto childId: childrenIds) {
			const Entity child = iScene->findEntityByUUID(childId);
			if (!child || !child.hasComponent<component::UiRect>())
				continue;
			if (!iScene->isEffectivelyVisible(child, false))
				continue;

			const auto& rect = child.getComponent<component::UiRect>();
			const bool hovered = hitTestRect(rect, vpSize, iMousePos);

			// UiButton interaction.
			if (child.hasComponent<component::UiButton>()) {
				auto& button = child.getComponent<component::UiButton>();
				if (button.state == component::UiButton::State::Disabled)
					continue;
				if (hovered) {
					s_consuming = true;
					if (iMousePressed)
						button.state = component::UiButton::State::Pressed;
					else
						button.state = component::UiButton::State::Hovered;
					if (clicked) {
						const auto uuid = static_cast<uint64_t>(child.getUUID());
						invokeLuaCallback(iScene, child, button.onClickCallback, uuid);
					}
				} else {
					button.state = component::UiButton::State::Normal;
				}
			}

			// UiSlider interaction.
			if (child.hasComponent<component::UiSlider>()) {
				auto& slider = child.getComponent<component::UiSlider>();
				if (hovered && iMousePressed) {
					s_consuming = true;
					const math::vec2 center = rect.computePosition(vpSize);
					const float leftEdge = center.x() - rect.size.x() * 0.5f;
					const float normalized = std::clamp((iMousePos.x() - leftEdge) / rect.size.x(), 0.f, 1.f);
					if (const float newValue = slider.minValue + normalized * (slider.maxValue - slider.minValue);
						std::abs(newValue - slider.value) > 1e-6f) {
						slider.value = newValue;
						invokeSliderCallback(iScene, child, slider.onValueChangedCallback, slider.value);
					}
				} else if (hovered) {
					s_consuming = true;
				}
			}
		}
	}
}

auto UiInputSystem::isUIConsuming() -> bool { return s_consuming; }

void UiInputSystem::reset() {
	s_consuming = false;
	s_wasPressed = false;
}

}// namespace owl::scene
