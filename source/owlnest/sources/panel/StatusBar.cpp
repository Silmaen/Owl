/**
 * @file StatusBar.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "StatusBar.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>

namespace owl::nest::panel {

namespace {

// Middle dot separator, UTF-8.
constexpr auto g_separator = " \xC2\xB7 ";
// Background while the scene plays: a dark shade of the play blue, so the mode reads at a glance.
constexpr ImU32 g_playBackground = IM_COL32(20, 38, 69, 255);
constexpr ImU32 g_playPill = IM_COL32(61, 139, 255, 255);
constexpr ImU32 g_pausePill = IM_COL32(239, 138, 60, 255);
constexpr ImU32 g_warningText = IM_COL32(239, 138, 60, 255);
constexpr ImU32 g_errorText = IM_COL32(229, 83, 75, 255);

auto plural(const uint64_t iCount, const char* iWord) -> std::string {
	return std::format("{} {}{}", iCount, iWord, iCount == 1 ? "" : "s");
}

auto getPillColor(const StatusInfo::Mode iMode) -> ImU32 {
	switch (iMode) {
		case StatusInfo::Mode::Edit:
			return ImGui::GetColorU32(ImGuiCol_Button);
		case StatusInfo::Mode::Play:
			return g_playPill;
		case StatusInfo::Mode::Pause:
			return g_pausePill;
	}
	return ImGui::GetColorU32(ImGuiCol_Button);
}

}// namespace

auto StatusBar::formatElapsed(const float iSeconds) -> std::string {
	const auto seconds = static_cast<int64_t>(std::floor(std::max(iSeconds, 0.f)));
	if (seconds < 5)
		return "just now";
	if (seconds < 60)
		return std::format("{} s ago", seconds);
	if (seconds < 3600)
		return std::format("{} min ago", seconds / 60);
	return std::format("{} h ago", seconds / 3600);
}

auto StatusBar::getModeLabel(const StatusInfo::Mode iMode) -> std::string {
	switch (iMode) {
		case StatusInfo::Mode::Edit:
			return "EDIT";
		case StatusInfo::Mode::Play:
			return "PLAYING";
		case StatusInfo::Mode::Pause:
			return "PAUSED";
	}
	return "EDIT";
}

void StatusBar::onImGuiRender(const StatusInfo& iInfo) {
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	const float height = ImGui::GetFrameHeightWithSpacing();
	const float spacing = style.ItemSpacing.x;
	const float textHeight = ImGui::GetTextLineHeight();
	const float textY = origin.y + (height - textHeight) * 0.5f;
	auto* draw = ImGui::GetWindowDrawList();

	const bool simulating = iInfo.mode != StatusInfo::Mode::Edit;
	draw->AddRectFilled(origin, {origin.x + width, origin.y + height},
						simulating ? g_playBackground : ImGui::GetColorU32(ImGuiCol_TitleBg));
	draw->AddLine(origin, {origin.x + width, origin.y}, ImGui::GetColorU32(ImGuiCol_Border));

	float x = origin.x + spacing;
	const auto mode = getModeLabel(iInfo.mode);
	const ImVec2 modeSize = ImGui::CalcTextSize(mode.c_str());
	draw->AddRectFilled({x, textY - 1.f}, {x + modeSize.x + 2.f * spacing, textY + textHeight + 1.f},
						getPillColor(iInfo.mode), 3.f);
	draw->AddText({x + spacing, textY}, ImGui::GetColorU32(ImGuiCol_Text), mode.c_str());
	x += modeSize.x + 4.f * spacing;

	std::string document = iInfo.document.empty() ? std::string{"No document"} : iInfo.document;
	if (iInfo.entityCount.has_value())
		document += std::format("{}{} {}", g_separator, *iInfo.entityCount,
								*iInfo.entityCount == 1 ? "entity" : "entities");
	draw->AddText({x, textY}, ImGui::GetColorU32(ImGuiCol_TextDisabled), document.c_str());

	float right = origin.x + width - spacing;
	const auto counter = [&](const char* iId, const std::string& iText, const ImU32 iColor) -> void {
		const ImVec2 size = ImGui::CalcTextSize(iText.c_str());
		right -= size.x + 2.f * spacing;
		ImGui::SetCursorScreenPos({right, origin.y});
		if (ImGui::InvisibleButton(iId, {size.x + 2.f * spacing, height})) {
			acknowledge(iInfo.warningCount, iInfo.errorCount);
			m_openLogRequested = true;
		}
		if (ImGui::IsItemHovered()) {
			draw->AddRectFilled({right, origin.y + 2.f}, {right + size.x + 2.f * spacing, origin.y + height - 2.f},
								ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.f);
			ImGui::SetTooltip("Open the Log");
		}
		draw->AddText({right + spacing, textY}, iColor, iText.c_str());
	};
	const auto label = [&](const std::string& iText) -> void {
		const ImVec2 size = ImGui::CalcTextSize(iText.c_str());
		right -= size.x + 2.f * spacing;
		draw->AddText({right + spacing, textY}, ImGui::GetColorU32(ImGuiCol_TextDisabled), iText.c_str());
	};

	const auto errors = getUnseenErrors(iInfo.errorCount);
	const auto warnings = getUnseenWarnings(iInfo.warningCount);
	counter("##statusErrors", plural(errors, "error"),
			errors > 0 ? g_errorText : ImGui::GetColorU32(ImGuiCol_TextDisabled));
	counter("##statusWarnings", plural(warnings, "warning"),
			warnings > 0 ? g_warningText : ImGui::GetColorU32(ImGuiCol_TextDisabled));
	if (!iInfo.backend.empty())
		label(iInfo.backend);
	if (iInfo.secondsSinceAutosave.has_value())
		label("Autosaved " + formatElapsed(*iInfo.secondsSinceAutosave));

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy({width, height});
}

}// namespace owl::nest::panel
