/**
 * @file InspectorEditTracker.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "InspectorEditTracker.h"

#include <gui/utils.h>
#include <imgui_internal.h>
#include <scene/SceneSerializer.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <tuple>

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

auto componentFields(const std::string& iComponentYaml, const std::string& iComponentKey) -> std::string {
	try {
		const auto fields = YAML::Load(iComponentYaml)[iComponentKey];
		if (!fields.IsMap())
			return {};
		YAML::Emitter out;
		out << YAML::Flow << fields;
		return out.c_str();
	} catch (const YAML::Exception& iEx) {
		OWL_WARN("Inspector: Component {} does not read back ({}).", iComponentKey, iEx.what())
		return {};
	}
}

auto popupDepth() -> int { return GImGui->OpenPopupStack.Size; }

auto toVec(const ImVec2& iVec) -> math::vec2 { return {iVec.x, iVec.y}; }

}// namespace

void InspectorEditTracker::beginFrame(const scene::Entity& iEntity, const commands::CommandTarget& iCommands) {
	const core::UUID uuid = iEntity ? iEntity.getUUID() : core::UUID{0};
	if (uuid != m_entityUuid) {
		flush(iCommands);
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

void InspectorEditTracker::endComponent(const std::string& iComponentKey, const std::string& iComponentName,
										const commands::CommandTarget& iCommands) {
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
	commit(iComponentKey, state, iCommands);
	state.before.reset();
}

void InspectorEditTracker::endFrame(const commands::CommandTarget& iCommands) {
	for (auto& [key, state]: m_states) {
		if (state.drawn)
			continue;
		state.hasRect = false;
		state.containsActive = false;
		state.hasNavFocus = false;
		state.ownedPopupDepth = -1;
		if (!state.before.has_value())
			continue;
		commit(key, state, iCommands);
		state.before.reset();
	}
}

void InspectorEditTracker::flush(const commands::CommandTarget& iCommands) {
	for (auto& [key, state]: m_states) {
		if (state.before.has_value())
			commit(key, state, iCommands);
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

void InspectorEditTracker::commit(const std::string& iComponentKey, ComponentState& ioState,
								  const commands::CommandTarget& iCommands) const {
	if (!iCommands.scene)
		return;
	const auto entity = iCommands.scene->findEntityByUUID(m_entityUuid);
	if (!entity || !ioState.before.has_value())
		return;
	const auto after = countedComponentYaml(entity, iComponentKey);
	if (after.empty() || after == *ioState.before || !iCommands.isValid())
		return;
	const auto values = componentFields(after, iComponentKey);
	++serializationCounter();
	const auto afterYaml = scene::SceneSerializer::serializeEntityToString(entity);
	const auto beforeYaml = scene::SceneSerializer::replaceComponentInString(afterYaml, iComponentKey, *ioState.before);
	if (beforeYaml.empty() || values.empty())
		return;
	// The command records the change from the state before the session.
	std::ignore = scene::SceneSerializer::applyEntityFromString(entity, beforeYaml);
	const auto result =
			iCommands.execute("component.set", {{"entity", static_cast<int64_t>(static_cast<uint64_t>(m_entityUuid))},
												{"component", iComponentKey},
												{"values", values},
												{"replace", true}});
	if (!result) {
		OWL_WARN("Inspector: Edit of {} not recorded ({}).", ioState.name, result.error().message)
		std::ignore = scene::SceneSerializer::applyEntityFromString(entity, afterYaml);
	}
}

}// namespace owl::nest::panel
