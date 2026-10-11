/**
 * @file Tooltip.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Tooltip.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

namespace owl::gui::widgets {

void richTooltip(const char* iTitle, const char* iShortcut, const char* iDescription) {
	if (iTitle == nullptr || !ImGui::BeginTooltip())
		return;
	const auto& theme = UiLayer::getTheme();
	UiLayer::pushTextStyle(TextStyle::Strong);
	ImGui::TextUnformatted(iTitle);
	UiLayer::popTextStyle();
	if (iShortcut != nullptr && *iShortcut != '\0') {
		ImGui::SameLine(0.f, ImGui::GetFontSize());
		UiLayer::pushTextStyle(TextStyle::Label);
		const ImVec2 textSize = ImGui::CalcTextSize(iShortcut);
		const ImVec2 padding{5.f, 1.f};
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		auto* draw = ImGui::GetWindowDrawList();
		draw->AddRectFilled(pos, {pos.x + textSize.x + 2.f * padding.x, pos.y + textSize.y + 2.f * padding.y},
							ImGui::GetColorU32(vec(theme.surface3)), 3.f);
		draw->AddText({pos.x + padding.x, pos.y + padding.y}, ImGui::GetColorU32(vec(theme.textMuted)), iShortcut);
		ImGui::Dummy({textSize.x + 2.f * padding.x, textSize.y + 2.f * padding.y});
		UiLayer::popTextStyle();
	}
	if (iDescription != nullptr && *iDescription != '\0') {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.f);
		ImGui::PushStyleColor(ImGuiCol_Text, vec(theme.textMuted));
		ImGui::TextWrapped("%s", iDescription);
		ImGui::PopStyleColor();
		ImGui::PopTextWrapPos();
	}
	ImGui::EndTooltip();
}

void itemTooltip(const char* iTitle, const char* iShortcut, const char* iDescription) {
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled))
		richTooltip(iTitle, iShortcut, iDescription);
}

void dragPreview(const char* iGlyph, const char* iLabel, const char* iKind) {
	const auto& theme = UiLayer::getTheme();
	ImGui::TextColored(vec(theme.accent), "%s", iGlyph);
	ImGui::SameLine();
	ImGui::TextUnformatted(iLabel);
	if (iKind != nullptr) {
		ImGui::SameLine();
		ImGui::TextColored(vec(theme.textDisabled), "%s", iKind);
	}
}

}// namespace owl::gui::widgets
