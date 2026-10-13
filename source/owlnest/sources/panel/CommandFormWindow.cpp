/**
 * @file CommandFormWindow.cpp
 * @author Silmaen
 * @date 13/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandFormWindow.h"

#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/PropertyGrid.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <span>
#include <string>
#include <tuple>
#include <variant>

namespace owl::nest::panel {

namespace {

void entityField(int64_t& ioUuid, const std::vector<EntityChoice>& iEntities) {
	const auto current = std::ranges::find(iEntities, core::UUID{static_cast<uint64_t>(ioUuid)}, &EntityChoice::uuid);
	const char* preview = current != iEntities.end() ? current->tag.c_str() : "Pick an entity";
	if (!ImGui::BeginCombo("##value", preview))
		return;
	for (const auto& entity: iEntities) {
		ImGui::PushID(static_cast<int>(static_cast<uint64_t>(entity.uuid) & 0x7fffffff));
		const bool selected = current != iEntities.end() && current->uuid == entity.uuid;
		if (ImGui::Selectable(entity.tag.c_str(), selected))
			ioUuid = static_cast<int64_t>(static_cast<uint64_t>(entity.uuid));
		ImGui::PopID();
	}
	ImGui::EndCombo();
}

void enumField(std::string& ioValue, const std::vector<std::string>& iChoices) {
	if (!ImGui::BeginCombo("##value", ioValue.c_str()))
		return;
	for (const auto& choice: iChoices) {
		if (ImGui::Selectable(choice.c_str(), choice == ioValue))
			ioValue = choice;
	}
	ImGui::EndCombo();
}

void valueField(commands::FormField& ioField, const std::vector<EntityChoice>& iEntities) {
	auto& value = ioField.value;
	switch (ioField.spec.type) {
		case commands::ArgType::Bool:
			ImGui::Checkbox("##value", &std::get<bool>(value));
			return;
		case commands::ArgType::Integer:
			ImGui::InputScalar("##value", ImGuiDataType_S64, &std::get<int64_t>(value));
			return;
		case commands::ArgType::Number:
			ImGui::InputDouble("##value", &std::get<double>(value));
			return;
		case commands::ArgType::Text:
			ImGui::InputText("##value", &std::get<std::string>(value));
			return;
		case commands::ArgType::Entity:
			entityField(std::get<int64_t>(value), iEntities);
			return;
		case commands::ArgType::Enum:
			enumField(std::get<std::string>(value), ioField.spec.choices);
			return;
		case commands::ArgType::Vec2:
			std::ignore = gui::widgets::vectorField("##value", std::span{std::get<math::vec2>(value).data(), 2});
			return;
		case commands::ArgType::Vec3:
			std::ignore = gui::widgets::vectorField("##value", std::span{std::get<math::vec3>(value).data(), 3});
			return;
		case commands::ArgType::Vec4:
			std::ignore = gui::widgets::vectorField("##value", std::span{std::get<math::vec4>(value).data(), 4});
			return;
		case commands::ArgType::Color:
			ImGui::ColorEdit4("##value", std::get<math::vec4>(value).data());
			return;
	}
}

void fieldRow(commands::FormField& ioField, const std::vector<EntityChoice>& iEntities) {
	gui::widgets::propertyLabel(ioField.spec.name.c_str(), ioField.spec.description.c_str());
	if (!ioField.spec.required) {
		ImGui::Checkbox("##given", &ioField.set);
		ImGui::SetItemTooltip("Give this argument");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
	}
	ImGui::BeginDisabled(!ioField.set);
	valueField(ioField, iEntities);
	ImGui::EndDisabled();
	std::ignore = gui::widgets::propertyReset(false);
}

}// namespace

void CommandFormWindow::open(const commands::CommandSpec& iSpec, const core::UUID iSelection) {
	m_form = commands::CommandForm{iSpec};
	m_form.setDefaultEntity(iSelection);
	m_description = iSpec.description;
	m_open = true;
	m_focus = true;
}

auto CommandFormWindow::onImGuiRender(const std::vector<EntityChoice>& iEntities) -> std::optional<CommandSubmission> {
	if (!m_open)
		return std::nullopt;
	const auto& theme = gui::UiLayer::getTheme();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float width = std::min(viewport->WorkSize.x * 0.8f, ImGui::GetFontSize() * 36.f);
	ImGui::SetNextWindowPos(
			{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + ImGui::GetFontSize() * 4.f},
			ImGuiCond_Always, {0.5f, 0.f});
	ImGui::SetNextWindowSize({width, 0.f});
	if (m_focus) {
		ImGui::SetNextWindowFocus();
		m_focus = false;
	}
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
									   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
									   ImGuiWindowFlags_AlwaysAutoResize;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.f, 10.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme.windowRounding * 1.6f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::vec(theme.surface2));
	const bool visible = ImGui::Begin("##commandForm", nullptr, flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
	if (!visible) {
		ImGui::End();
		return std::nullopt;
	}
	ImGui::TextColored(gui::vec(theme.accent), "%s", m_form.getCommandName().c_str());
	ImGui::TextColored(gui::vec(theme.textMuted), "%s", m_description.c_str());
	ImGui::Separator();
	if (gui::widgets::beginProperties("##commandFormGrid")) {
		for (auto& field: m_form.getFields()) {
			ImGui::PushID(field.spec.name.c_str());
			fieldRow(field, iEntities);
			ImGui::PopID();
		}
		gui::widgets::endProperties();
	}
	const auto args = m_form.buildArgs();
	if (!args)
		ImGui::TextColored(gui::vec(theme.warning), "%s", args.error().message.c_str());
	ImGui::Separator();
	ImGui::BeginDisabled(!args.has_value());
	bool run = ImGui::Button("Run");
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
		m_open = false;
	ImGui::SameLine();
	ImGui::TextColored(gui::vec(theme.textDisabled), "Ctrl+Enter to run, Escape to close");
	run = run || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter));
	ImGui::End();
	if (!run || !args || !m_open)
		return std::nullopt;
	m_open = false;
	return CommandSubmission{.command = m_form.getCommandName(), .args = *args};
}

}// namespace owl::nest::panel
