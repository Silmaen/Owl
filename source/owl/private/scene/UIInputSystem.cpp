/**
 * @file UIInputSystem.cpp
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/UIInputSystem.h"

#include "scene/Entity.h"
#include "scene/component/Canvas.h"
#include "scene/component/Hierarchy.h"
#include "scene/component/LuaScript.h"
#include "scene/component/UIButton.h"
#include "scene/component/UIRect.h"
#include "scene/component/UISlider.h"
#include "script/ScriptInstance.h"

#include <algorithm>

namespace owl::scene {

bool UIInputSystem::s_consuming = false;
bool UIInputSystem::s_wasPressed = false;

namespace {

/// Check if a screen-space point is inside a UI rect.
auto hitTestRect(const component::UIRect& iRect, const math::vec2& iParentSize, const math::vec2& iPoint) -> bool {
	const math::vec2 center = iRect.computePosition(iParentSize);
	const float halfW = iRect.size.x() * 0.5f;
	const float halfH = iRect.size.y() * 0.5f;
	return iPoint.x() >= center.x() - halfW && iPoint.x() <= center.x() + halfW && iPoint.y() >= center.y() - halfH &&
		   iPoint.y() <= center.y() + halfH;
}

/**
 * @brief Walk up hierarchy from an entity to find the nearest ancestor with a LuaScript component.
 * @param[in] iScene The scene containing the entity.
 * @param[in] iEntity The starting entity.
 * @return The nearest ancestor (or self) with a LuaScript, or invalid Entity if none found.
 */
auto findScriptOwner(const Scene* iScene, Entity iEntity) -> Entity {
	while (iEntity) {
		if (iEntity.hasComponent<component::LuaScript>())
			break;
		const auto pid = iEntity.getComponent<component::Hierarchy>().parentId;
		iEntity = pid != core::UUID{0} ? iScene->findEntityByUUID(pid) : Entity{};
	}
	return iEntity;
}

/**
 * @brief Find the nearest ancestor with a LuaScript and call a named function.
 * @param[in] iScene The scene containing the entity.
 * @param[in] iEntity The entity whose ancestor to search.
 * @param[in] iCallbackName The Lua function name to invoke.
 * @param[in] iEntityId The clicked entity UUID, passed as _clicked_entity property.
 */
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

/**
 * @brief Invoke a slider value-changed callback on the nearest ancestor LuaScript.
 * @param[in] iScene The scene containing the entity.
 * @param[in] iEntity The slider entity whose ancestor to search.
 * @param[in] iCallbackName The Lua function name to invoke.
 * @param[in] iValue The new slider value, passed as _slider_value property.
 */
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

void UIInputSystem::update(Scene* iScene, const math::vec2ui& iViewportSize, const math::vec2& iMousePos,
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
			if (!child || !child.hasComponent<component::UIRect>())
				continue;
			if (!iScene->isEffectivelyVisible(child, false))
				continue;

			const auto& rect = child.getComponent<component::UIRect>();
			const bool hovered = hitTestRect(rect, vpSize, iMousePos);

			// UIButton interaction.
			if (child.hasComponent<component::UIButton>()) {
				auto& button = child.getComponent<component::UIButton>();
				if (button.state == component::UIButton::State::Disabled)
					continue;
				if (hovered) {
					s_consuming = true;
					if (iMousePressed)
						button.state = component::UIButton::State::Pressed;
					else
						button.state = component::UIButton::State::Hovered;
					if (clicked) {
						const auto uuid = static_cast<uint64_t>(child.getUUID());
						invokeLuaCallback(iScene, child, button.onClickCallback, uuid);
					}
				} else {
					button.state = component::UIButton::State::Normal;
				}
			}

			// UISlider interaction.
			if (child.hasComponent<component::UISlider>()) {
				auto& slider = child.getComponent<component::UISlider>();
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

auto UIInputSystem::isUIConsuming() -> bool { return s_consuming; }

void UIInputSystem::reset() {
	s_consuming = false;
	s_wasPressed = false;
}

}// namespace owl::scene
