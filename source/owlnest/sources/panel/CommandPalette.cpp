/**
 * @file CommandPalette.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "CommandPalette.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

auto lower(const char iChar) -> char { return static_cast<char>(std::tolower(static_cast<unsigned char>(iChar))); }

auto isWordStart(const std::string_view iText, const size_t iPos) -> bool {
	if (iPos == 0)
		return true;
	const char previous = iText[iPos - 1];
	return previous == ' ' || previous == '.' || previous == '_' || previous == '/' || previous == ':' ||
		   (std::isupper(static_cast<unsigned char>(iText[iPos])) != 0 &&
			std::islower(static_cast<unsigned char>(previous)) != 0);
}

}// namespace

auto CommandPalette::score(const std::string_view iLabel, const std::string_view iQuery) -> int {
	int total = 0;
	size_t from = 0;
	size_t previous = std::string_view::npos;
	for (const char wanted: iQuery) {
		if (wanted == ' ')
			continue;
		size_t found = std::string_view::npos;
		for (size_t i = from; i < iLabel.size(); ++i) {
			if (lower(iLabel[i]) == lower(wanted)) {
				found = i;
				break;
			}
		}
		if (found == std::string_view::npos)
			return -1;
		total += 1;
		if (previous != std::string_view::npos && found == previous + 1)
			total += 6;
		if (isWordStart(iLabel, found))
			total += 8;
		if (found == 0)
			total += 4;
		previous = found;
		from = found + 1;
	}
	if (total == 0)
		return 0;
	// Shorter labels win between equal matches, and a match never drops below 0: the penalty is at most 5.
	return total * 8 - static_cast<int>(std::min<size_t>(iLabel.size(), 40) / 8);
}

auto CommandPalette::filter(const std::vector<PaletteItem>& iItems, const std::string_view iQuery)
		-> std::vector<size_t> {
	std::vector<std::pair<int, size_t>> ranked;
	for (size_t i = 0; i < iItems.size(); ++i) {
		const auto text = std::format("{} {}", iItems[i].category, iItems[i].label);
		const int labelScore = score(iItems[i].label, iQuery);
		const int fullScore = score(text, iQuery);
		const int best = std::max(labelScore, fullScore);
		if (best >= 0 || iQuery.empty())
			ranked.emplace_back(iQuery.empty() ? 0 : best, i);
	}
	std::ranges::stable_sort(
			ranked, [](const auto& iFirst, const auto& iSecond) -> bool { return iFirst.first > iSecond.first; });
	std::vector<size_t> indices;
	for (const auto& [rank, index]: ranked) {
		if (indices.size() == maxResults)
			break;
		indices.push_back(index);
	}
	return indices;
}

auto CommandPalette::categoryOf(const std::string_view iActionId) -> std::string {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 7> categories{{{"scene", "Scene"},
																							  {"entity", "Entity"},
																							  {"guizmo", "Gizmo"},
																							  {"edit", "Edit"},
																							  {"doc", "Document"},
																							  {"help", "Help"},
																							  {"view", "View"}}};
	const auto prefix = iActionId.substr(0, iActionId.find('.'));
	for (const auto& [key, name]: categories) {
		if (key == prefix)
			return std::string{name};
	}
	return "Command";
}

void CommandPalette::open() {
	m_open = true;
	m_focusInput = true;
	m_query.clear();
	m_highlighted = 0;
}

void CommandPalette::onImGuiRender(const std::vector<PaletteItem>& iItems) {
	if (!m_open)
		return;
	const auto& theme = gui::UiLayer::getTheme();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float width = std::min(viewport->WorkSize.x * 0.8f, ImGui::GetFontSize() * 36.f);
	ImGui::SetNextWindowPos(
			{viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + ImGui::GetFontSize() * 4.f},
			ImGuiCond_Always, {0.5f, 0.f});
	ImGui::SetNextWindowSize({width, 0.f});
	if (m_focusInput)
		ImGui::SetNextWindowFocus();
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
									   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
									   ImGuiWindowFlags_AlwaysAutoResize;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.f, 10.f});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme.windowRounding * 1.6f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, gui::vec(theme.surface2));
	const bool visible = ImGui::Begin("##commandPalette", nullptr, flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(3);
	if (!visible) {
		ImGui::End();
		return;
	}

	const auto results = filter(iItems, m_query);
	if (!results.empty())
		m_highlighted = std::min(m_highlighted, results.size() - 1);
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !results.empty())
		m_highlighted = (m_highlighted + 1) % results.size();
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !results.empty())
		m_highlighted = (m_highlighted + results.size() - 1) % results.size();

	ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.1f);
	ImGui::TextColored(gui::vec(theme.textDisabled), "%s", gui::icon::search);
	ImGui::SameLine();
	if (m_focusInput) {
		ImGui::SetKeyboardFocusHere();
		m_focusInput = false;
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, gui::vec(gui::Theme::withAlpha(theme.well, 0.f)));
	if (ImGui::InputTextWithHint("##paletteQuery", "Type a command or an entity name", &m_query))
		m_highlighted = 0;
	ImGui::PopStyleColor();
	ImGui::PopFont();
	const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		m_open = false;
	ImGui::Separator();

	const PaletteItem* chosen = nullptr;
	if (results.empty())
		ImGui::TextColored(gui::vec(theme.textMuted), "No command matches.");
	const float categoryWidth = ImGui::GetFontSize() * 5.f;
	for (size_t row = 0; row < results.size(); ++row) {
		const auto& item = iItems[results[row]];
		ImGui::PushID(static_cast<int>(row));
		const bool highlighted = row == m_highlighted;
		const auto origin = ImGui::GetCursorScreenPos();
		if (ImGui::Selectable("##paletteItem", highlighted, ImGuiSelectableFlags_None, {0.f, ImGui::GetFrameHeight()}))
			chosen = &item;
		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDelta.x * ImGui::GetIO().MouseDelta.x +
													  ImGui::GetIO().MouseDelta.y * ImGui::GetIO().MouseDelta.y >
											  0.f)
			m_highlighted = row;
		auto* draw = ImGui::GetWindowDrawList();
		const float textY = std::floor(origin.y + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f);
		draw->AddText({origin.x + 4.f, textY},
					  ImGui::GetColorU32(gui::vec(highlighted ? theme.accent : theme.textDisabled)),
					  item.category.c_str());
		draw->AddText({origin.x + categoryWidth, textY}, ImGui::GetColorU32(gui::vec(theme.text)), item.label.c_str());
		if (!item.shortcut.empty()) {
			const auto size = ImGui::CalcTextSize(item.shortcut.c_str());
			draw->AddText({origin.x + ImGui::GetContentRegionAvail().x - size.x - 6.f, textY},
						  ImGui::GetColorU32(gui::vec(theme.textMuted)), item.shortcut.c_str());
		}
		ImGui::PopID();
	}
	if (enter && !results.empty())
		chosen = &iItems[results[m_highlighted]];
	ImGui::Separator();
	ImGui::TextColored(gui::vec(theme.textDisabled), "Up / Down to move, Enter to run, Escape to close");
	if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !m_focusInput)
		m_open = false;
	ImGui::End();
	if (chosen != nullptr) {
		m_open = false;
		if (chosen->run)
			chosen->run();
	}
}

}// namespace owl::nest::panel
