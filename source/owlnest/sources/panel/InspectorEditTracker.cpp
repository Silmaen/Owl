/**
 * @file InspectorEditTracker.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "InspectorEditTracker.h"

#include "../commands/ComponentCommands.h"

#include <gui/utils.h>
#include <imgui_internal.h>
#include <scene/SceneSerializer.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <ranges>
#include <string>

namespace owl::nest::panel {

namespace {

auto serializationCounter() -> uint64_t& {
	static uint64_t s_count = 0;
	return s_count;
}

auto countedComponentYaml(const scene::Entity& iEntity, const std::string& iComponentKey) -> std::string {
	++serializationCounter();
	return scene::SceneSerializer::serializeComponentToString(iEntity, iComponentKey);
}

auto anyMouseClickedOrReleased() -> bool {
	for (int button = 0; button < ImGuiMouseButton_COUNT; ++button) {
		if (ImGui::IsMouseClicked(button) || ImGui::IsMouseReleased(button))
			return true;
	}
	return false;
}

auto anyKeyTyped() -> bool {
	if (!ImGui::GetIO().InputQueueCharacters.empty())
		return true;
	for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
		if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key)))
			return true;
	}
	return false;
}

auto popupDepth() -> int { return GImGui->OpenPopupStack.Size; }

auto toVec(const ImVec2& iVec) -> math::vec2 { return {iVec.x, iVec.y}; }

}// namespace

void InspectorEditTracker::beginFrame(const scene::Entity& iEntity, scene::Scene* ioScene,
									  SceneUndoManager* ioUndoManager) {
	const core::UUID uuid = iEntity ? iEntity.getUUID() : core::UUID{0};
	if (uuid != m_entityUuid) {
		flush(ioScene, ioUndoManager);
		m_entityUuid = uuid;
	}
	for (auto& state: m_states | std::views::values) state.drawn = false;
}

void InspectorEditTracker::beginComponent(const scene::Entity& iEntity, const std::string& iComponentKey) {
	auto& state = m_states[iComponentKey];
	state.drawn = true;
	if (!state.before.has_value() && shouldOpen(state))
		state.before = countedComponentYaml(iEntity, iComponentKey);
	state.popupDepthAtBegin = popupDepth();
	state.navSeenAtBegin = GImGui->NavIdIsAlive;
	ImGui::BeginGroup();
}

void InspectorEditTracker::endComponent(const scene::Entity& iEntity, const std::string& iComponentKey,
										const std::string& iComponentName, SceneUndoManager* ioUndoManager) {
	ImGui::EndGroup();
	auto& state = m_states[iComponentKey];
	state.name = iComponentName;
	state.rectMin = toVec(ImGui::GetItemRectMin());
	state.rectMax = toVec(ImGui::GetItemRectMax());
	state.hasRect = true;
	state.containsActive = ImGui::IsItemActive();
	state.hasNavFocus =
			!state.navSeenAtBegin && GImGui->NavIdIsAlive && GImGui->NavWindow == ImGui::GetCurrentWindowRead();
	if (popupDepth() > state.popupDepthAtBegin && state.before.has_value())
		state.ownedPopupDepth = state.popupDepthAtBegin;
	else if (state.ownedPopupDepth >= 0 && popupDepth() <= state.ownedPopupDepth)
		state.ownedPopupDepth = -1;
	if (!state.before.has_value())
		return;
	const bool mouseHeldOver =
			ImGui::IsAnyMouseDown() && ImGui::IsMouseHoveringRect(gui::vec(state.rectMin), gui::vec(state.rectMax));
	if (state.containsActive || state.ownedPopupDepth >= 0 || mouseHeldOver)
		return;
	if (auto* scene = iEntity.getScene(); scene != nullptr)
		commit(*scene, iComponentKey, state, ioUndoManager);
	state.before.reset();
}

void InspectorEditTracker::endFrame(scene::Scene* ioScene, SceneUndoManager* ioUndoManager) {
	for (auto& [key, state]: m_states) {
		if (state.drawn)
			continue;
		state.hasRect = false;
		state.containsActive = false;
		state.hasNavFocus = false;
		state.ownedPopupDepth = -1;
		if (!state.before.has_value())
			continue;
		if (ioScene != nullptr)
			commit(*ioScene, key, state, ioUndoManager);
		state.before.reset();
	}
}

void InspectorEditTracker::flush(scene::Scene* ioScene, SceneUndoManager* ioUndoManager) {
	for (auto& [key, state]: m_states) {
		if (state.before.has_value() && ioScene != nullptr)
			commit(*ioScene, key, state, ioUndoManager);
	}
	m_states.clear();
	m_entityUuid = core::UUID{0};
}

auto InspectorEditTracker::isEditing() const -> bool {
	return std::ranges::any_of(m_states | std::views::values,
							   [](const ComponentState& iState) -> bool { return iState.before.has_value(); });
}

auto InspectorEditTracker::serializationCount() -> uint64_t { return serializationCounter(); }

auto InspectorEditTracker::shouldOpen(const ComponentState& iState) -> bool {
	if (iState.containsActive || iState.ownedPopupDepth >= 0)
		return true;
	if (iState.hasNavFocus && anyKeyTyped())
		return true;
	return iState.hasRect && ImGui::IsMouseHoveringRect(gui::vec(iState.rectMin), gui::vec(iState.rectMax)) &&
		   anyMouseClickedOrReleased();
}

void InspectorEditTracker::commit(scene::Scene& ioScene, const std::string& iComponentKey, ComponentState& ioState,
								  SceneUndoManager* ioUndoManager) const {
	const auto entity = ioScene.findEntityByUUID(m_entityUuid);
	if (!entity || !ioState.before.has_value())
		return;
	const auto after = countedComponentYaml(entity, iComponentKey);
	if (after.empty() || after == *ioState.before)
		return;
	++serializationCounter();
	auto afterSnapshot = EntitySnapshot::capture(entity);
	++serializationCounter();
	auto beforeYaml =
			scene::SceneSerializer::replaceComponentInString(afterSnapshot.yamlData, iComponentKey, *ioState.before);
	if (beforeYaml.empty())
		return;
	auto overrides = commands::PrefabOverrideChange::record(entity, ioScene, beforeYaml);
	if (!overrides.isEmpty()) {
		++serializationCounter();
		afterSnapshot = EntitySnapshot::capture(entity);
	}
	if (ioUndoManager == nullptr)
		return;
	auto command = mkUniq<commands::ModifyEntityCommand>(entity.getUUID(), EntitySnapshot{entity.getUUID(), beforeYaml},
														 std::format("Modify {}", ioState.name));
	command->setAfter(std::move(afterSnapshot));
	command->setPrefabOverrides(std::move(overrides));
	ioUndoManager->push(std::move(command));
}

}// namespace owl::nest::panel
