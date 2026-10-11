/**
 * @file IconButton.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/IconButton.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/Tooltip.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace owl::gui::widgets {

auto iconButton(const char* iId, const IconButtonSpec& iSpec) -> bool {
	const auto& theme = UiLayer::getTheme();
	const float side = ImGui::GetFrameHeight();
	ImGui::PushID(iId);
	ImGui::BeginDisabled(iSpec.disabled);
	const bool clicked = ImGui::InvisibleButton("##iconButton", {side, side});
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	auto* draw = ImGui::GetWindowDrawList();
	math::vec4 background = Theme::withAlpha(theme.surface0, 0.f);
	if (iSpec.selected)
		background = theme.getSelection();
	if (hovered && !iSpec.disabled)
		background = active ? theme.surface4 : theme.surface3;
	if (background.w() > 0.f)
		draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(background)), theme.frameRounding);
	if (iSpec.glyph != nullptr) {
		math::vec4 color = iSpec.selected ? theme.accent : theme.text;
		if (iSpec.disabled)
			color = theme.textDisabled;
		const ImVec2 glyphSize = ImGui::CalcTextSize(iSpec.glyph);
		const ImVec2 pos{std::floor(min.x + (side - glyphSize.x) * 0.5f),
						 std::floor(min.y + (side - glyphSize.y) * 0.5f)};
		draw->AddText(pos, ImGui::GetColorU32(vec(color)), iSpec.glyph);
	}
	ImGui::EndDisabled();
	if (iSpec.tooltip != nullptr)
		itemTooltip(iSpec.tooltip, iSpec.shortcut, iSpec.description);
	ImGui::PopID();
	return clicked && !iSpec.disabled;
}

auto button(const char* iId, const ButtonSpec& iSpec) -> bool {
	const auto& theme = UiLayer::getTheme();
	const float height = ImGui::GetFrameHeight();
	const float padX = std::round(ImGui::GetFontSize() * 0.6f);
	const float gap = std::round(ImGui::GetFontSize() * 0.35f);
	const float glyphWidth = iSpec.glyph != nullptr ? ImGui::CalcTextSize(iSpec.glyph).x + gap : 0.f;
	const ImVec2 labelSize = ImGui::CalcTextSize(iSpec.label);
	ImGui::PushID(iId);
	ImGui::BeginDisabled(iSpec.disabled);
	const float content = glyphWidth + labelSize.x;
	const float width = std::max(padX * 2.f + content, iSpec.width);
	const bool clicked = ImGui::InvisibleButton("##button", {width, height});
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	math::vec4 background = hovered ? theme.surface4 : theme.surface3;
	math::vec4 color = iSpec.disabled ? theme.textDisabled : theme.text;
	if (iSpec.primary) {
		background = hovered ? Theme::mix(theme.accent, theme.text, 0.15f) : theme.accent;
		if (active)
			background = Theme::mix(theme.accent, theme.surface0, 0.15f);
		color = theme.onAccent;
	}
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(background)), theme.frameRounding);
	const float textY = std::floor(min.y + (height - labelSize.y) * 0.5f);
	float x = std::floor(min.x + (width - content) * 0.5f);
	if (iSpec.glyph != nullptr) {
		draw->AddText({x, textY}, ImGui::GetColorU32(vec(color)), iSpec.glyph);
		x += glyphWidth;
	}
	draw->AddText({x, textY}, ImGui::GetColorU32(vec(color)), iSpec.label);
	ImGui::EndDisabled();
	if (iSpec.tooltip != nullptr)
		itemTooltip(iSpec.tooltip, iSpec.shortcut, iSpec.description);
	ImGui::PopID();
	return clicked && !iSpec.disabled;
}

}// namespace owl::gui::widgets
