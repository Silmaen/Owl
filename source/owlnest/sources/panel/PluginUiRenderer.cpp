/**
 * @file PluginUiRenderer.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PluginUiRenderer.h"

#include "../DockLayout.h"

#include <gui/IconFont.h>
#include <gui/utils.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest::panel {

namespace {

auto tooltipOf(const session::PluginWidget& iWidget) -> const char* {
	return iWidget.tooltip.empty() ? nullptr : iWidget.tooltip.c_str();
}

template<typename T>
auto valueOr(const script::HostValue& iValue, const T& iDefault) -> T {
	const auto* value = std::get_if<T>(&iValue);
	return value != nullptr ? *value : iDefault;
}

auto toFloats(const script::HostValue& iValue, const size_t iSize) -> std::array<float, 4> {
	std::array<float, 4> floats{0.f, 0.f, 0.f, 1.f};
	const auto numbers = valueOr(iValue, std::vector<double>{});
	for (size_t index = 0; index < std::min(iSize, numbers.size()); ++index)
		floats[index] = static_cast<float>(numbers[index]);
	return floats;
}

auto toNumbers(const std::array<float, 4>& iFloats, const size_t iSize) -> script::HostValue {
	std::vector<double> numbers;
	numbers.reserve(iSize);
	for (size_t index = 0; index < iSize; ++index) numbers.push_back(static_cast<double>(iFloats[index]));
	return numbers;
}

}// namespace

PluginUiRenderer::~PluginUiRenderer() = default;

void PluginUiRenderer::begin(const std::string_view iId, const bool iNested) {
	ImGui::PushID(iId.data(), iId.data() + iId.size());
	m_count = 0;
	m_gridOpen = false;
	m_sectionOpen = false;
	m_nested = iNested;
}

void PluginUiRenderer::end() {
	closeGrid();
	if (m_sectionOpen)
		gui::widgets::endSection();
	m_sectionOpen = false;
	ImGui::PopID();
}

auto PluginUiRenderer::openGrid() -> bool {
	if (!m_gridOpen)
		m_gridOpen = gui::widgets::beginProperties("##pluginProperties");
	return m_gridOpen;
}

void PluginUiRenderer::closeGrid() {
	if (m_gridOpen)
		gui::widgets::endProperties();
	m_gridOpen = false;
}

auto PluginUiRenderer::shownValue(const uint32_t iField, const script::HostValue& iGiven) const -> script::HostValue {
	return iField == m_activeField ? m_activeValue : iGiven;
}

auto PluginUiRenderer::commit(const uint32_t iField, const bool iEdited, script::HostValue iValue)
		-> session::PluginWidgetResult {
	if (iEdited) {
		m_activeField = iField;
		m_activeValue = iValue;
	}
	const bool ended = iField == m_activeField && ImGui::IsItemDeactivated();
	const bool changed = ended && ImGui::IsItemDeactivatedAfterEdit();
	if (ended)
		m_activeField = 0;
	return {.value = std::move(iValue), .changed = changed};
}

auto PluginUiRenderer::drawRow(const session::PluginWidget& iWidget) -> session::PluginWidgetResult {
	using Kind = session::PluginWidgetKind;
	session::PluginWidgetResult result{.value = iWidget.value, .changed = false};
	gui::widgets::propertyLabel(iWidget.label.c_str(), tooltipOf(iWidget));
	const auto field = static_cast<uint32_t>(ImGui::GetID("##value"));
	const auto shown = shownValue(field, iWidget.value);
	const auto speed = static_cast<float>(iWidget.step.value_or(iWidget.kind == Kind::Integer ? 1.0 : 0.1));
	if (iWidget.kind == Kind::Value) {
		ImGui::TextUnformatted(valueOr(iWidget.value, std::string{}).c_str());
	} else if (iWidget.kind == Kind::Checkbox) {
		bool value = valueOr(iWidget.value, false);
		result.changed = gui::widgets::checkbox("##value", &value);
		result.value = value;
	} else if (iWidget.kind == Kind::Number) {
		auto value = static_cast<float>(valueOr(shown, 0.0));
		const bool edited = ImGui::DragFloat(
				"##value", &value, speed, static_cast<float>(iWidget.min.value_or(-std::numeric_limits<float>::max())),
				static_cast<float>(iWidget.max.value_or(std::numeric_limits<float>::max())), "%.3f",
				ImGuiSliderFlags_AlwaysClamp);
		result = commit(field, edited, static_cast<double>(value));
	} else if (iWidget.kind == Kind::Integer) {
		auto value = static_cast<int>(valueOr(shown, int64_t{0}));
		const bool edited = ImGui::DragInt("##value", &value, speed,
										   static_cast<int>(iWidget.min.value_or(std::numeric_limits<int>::min())),
										   static_cast<int>(iWidget.max.value_or(std::numeric_limits<int>::max())),
										   "%d", ImGuiSliderFlags_AlwaysClamp);
		result = commit(field, edited, static_cast<int64_t>(value));
	} else if (iWidget.kind == Kind::Input) {
		auto value = valueOr(shown, std::string{});
		const bool edited = ImGui::InputText("##value", &value);
		result = commit(field, edited, std::move(value));
	} else if (iWidget.kind == Kind::Vec3) {
		auto value = toFloats(shown, 3);
		const bool edited = gui::widgets::vectorField("##value", std::span<float>{value.data(), 3}, speed);
		// The three fields are one edit: it ends when none of them is active any more.
		if (edited) {
			m_activeField = field;
			m_activeValue = toNumbers(value, 3);
		}
		result.value = toNumbers(value, 3);
		if (field == m_activeField && !edited && !ImGui::IsAnyItemActive()) {
			result.changed = true;
			m_activeField = 0;
		}
	} else if (iWidget.kind == Kind::Color) {
		auto value = toFloats(shown, 4);
		const bool edited = ImGui::ColorEdit4("##value", value.data(),
											  ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip);
		result = commit(field, edited, toNumbers(value, 4));
	} else if (iWidget.kind == Kind::Choice) {
		auto value = valueOr(iWidget.value, std::string{});
		if (gui::widgets::beginCombo("##value", value.c_str())) {
			for (const auto& choice: iWidget.choices) {
				if (ImGui::Selectable(choice.c_str(), choice == value) && choice != value) {
					value = choice;
					result.changed = true;
				}
			}
			ImGui::EndCombo();
		}
		result.value = std::move(value);
	}
	std::ignore = gui::widgets::propertyReset(false);
	return result;
}

auto PluginUiRenderer::draw(const session::PluginWidget& iWidget) -> session::PluginWidgetResult {
	using Kind = session::PluginWidgetKind;
	ImGui::PushID(m_count++);
	session::PluginWidgetResult result{.value = iWidget.value, .changed = false};
	switch (iWidget.kind) {
		case Kind::Section:
			closeGrid();
			if (m_sectionOpen)
				gui::widgets::endSection();
			m_sectionOpen = false;
			if (m_nested) {
				result.changed = gui::widgets::subHeader(iWidget.label.c_str(), true);
			} else {
				m_sectionOpen = gui::widgets::sectionHeader(iWidget.label.c_str(), gui::icon::puzzle, false).open;
				result.changed = m_sectionOpen;
			}
			break;
		case Kind::Text:
			closeGrid();
			if (iWidget.emphasis)
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped("%s", iWidget.label.c_str());
			if (iWidget.emphasis)
				ImGui::PopStyleColor();
			break;
		case Kind::Button:
			closeGrid();
			result.changed = gui::widgets::button(
					"##button",
					{.label = iWidget.label.c_str(), .tooltip = tooltipOf(iWidget), .primary = iWidget.emphasis});
			break;
		case Kind::Value:
		case Kind::Checkbox:
		case Kind::Number:
		case Kind::Integer:
		case Kind::Input:
		case Kind::Vec3:
		case Kind::Color:
		case Kind::Choice:
			if (openGrid())
				result = drawRow(iWidget);
			break;
	}
	ImGui::PopID();
	return result;
}

void renderPluginPanels(session::PluginHost& ioHost, PluginUiRenderer& ioUi) {
	std::vector<std::pair<std::string, std::string>> panels;
	for (const auto* panel: ioHost.getContributions(session::PluginContributionKind::Panel))
		if (panel->visible)
			panels.emplace_back(panel->id, panel->title);
	for (const auto& [id, title]: panels) {
		bool open = true;
		if (ImGui::Begin(std::format("{} {}{}{}", gui::icon::puzzle, title, panelWindow::pluginPrefix, id).c_str(),
						 &open)) {
			ioUi.begin(id, false);
			std::ignore = ioHost.drawPanel(id, ioUi);
			ioUi.end();
		}
		ImGui::End();
		if (!open)
			std::ignore = ioHost.setPanelVisible(id, false);
	}
}

}// namespace owl::nest::panel
