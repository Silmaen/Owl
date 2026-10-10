/**
 * @file EmptyState.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/EmptyState.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace owl::gui::widgets {

auto emptyState(const char* iGlyph, const char* iTitle, const char* iDescription, const char* iAction) -> bool {
	const auto& theme = UiLayer::getTheme();
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 start = ImGui::GetCursorPos();
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float base = style.FontSizeBase;
	const float glyphScale = 2.4f;
	const float wrap = std::min(avail.x * 0.85f, ImGui::GetFontSize() * 22.f);
	const float gap = style.ItemSpacing.y * 2.f;

	float height = 0.f;
	if (iGlyph != nullptr)
		height += ImGui::GetFontSize() * glyphScale + gap;
	height += ImGui::GetTextLineHeight();
	const float descriptionHeight =
			iDescription != nullptr ? ImGui::CalcTextSize(iDescription, nullptr, false, wrap).y : 0.f;
	if (iDescription != nullptr)
		height += gap + descriptionHeight;
	if (iAction != nullptr)
		height += gap + ImGui::GetFrameHeight();

	float y = start.y + std::max(0.f, std::floor((avail.y - height) * 0.5f));
	const auto centred = [&](const float iWidth) -> void {
		ImGui::SetCursorPos({start.x + std::max(0.f, std::floor((avail.x - iWidth) * 0.5f)), y});
	};
	if (iGlyph != nullptr) {
		ImGui::PushFont(nullptr, base * glyphScale);
		centred(ImGui::CalcTextSize(iGlyph).x);
		ImGui::TextColored(vec(theme.textDisabled), "%s", iGlyph);
		y += ImGui::GetFontSize() + gap;
		ImGui::PopFont();
	}
	UiLayer::pushTextStyle(TextStyle::Strong);
	centred(ImGui::CalcTextSize(iTitle).x);
	ImGui::TextUnformatted(iTitle);
	UiLayer::popTextStyle();
	y += ImGui::GetTextLineHeight();
	if (iDescription != nullptr) {
		y += gap;
		const float width = std::min(wrap, ImGui::CalcTextSize(iDescription).x);
		centred(width);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::TextColored(vec(theme.textMuted), "%s", iDescription);
		ImGui::PopTextWrapPos();
		y += descriptionHeight;
	}
	bool clicked = false;
	if (iAction != nullptr) {
		y += gap;
		const float width = ImGui::CalcTextSize(iAction).x + style.FramePadding.x * 4.f;
		centred(width);
		ImGui::PushStyleColor(ImGuiCol_Button, vec(theme.accent));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(Theme::mix(theme.accent, theme.text, 0.15f)));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(Theme::mix(theme.accent, theme.surface0, 0.15f)));
		ImGui::PushStyleColor(ImGuiCol_Text, vec(theme.onAccent));
		clicked = ImGui::Button(iAction, {width, 0.f});
		ImGui::PopStyleColor(4);
	}
	return clicked;
}

}// namespace owl::gui::widgets
