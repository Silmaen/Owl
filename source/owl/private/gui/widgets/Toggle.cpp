/**
 * @file Toggle.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Toggle.h"

#include "core/Macros.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cmath>
#include <cstring>

namespace owl::gui::widgets {

namespace {

auto visibleEnd(const char* iLabel) -> const char* {
	const char* hidden = std::strstr(iLabel, "##");
	return hidden != nullptr ? hidden : iLabel + std::strlen(iLabel);
}

}// namespace

auto toggle(const char* iId, bool* ioValue) -> bool {
	const auto& theme = UiLayer::getTheme();
	const float frame = ImGui::GetFrameHeight();
	const float height = std::round(ImGui::GetFontSize() * 1.05f);
	const float width = std::round(height * 1.8f);
	const bool clicked = ImGui::InvisibleButton(iId, {width, frame});
	if (clicked) {
		*ioValue = !*ioValue;
		ImGui::MarkItemEdited(ImGui::GetItemID());
	}
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 itemMin = ImGui::GetItemRectMin();
	const ImVec2 min{itemMin.x, std::floor(itemMin.y + (frame - height) * 0.5f)};
	const ImVec2 max{min.x + width, min.y + height};
	auto track = *ioValue ? theme.accent : theme.surface4;
	if (hovered)
		track = Theme::mix(track, theme.text, 0.1f);
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(track)), height * 0.5f);
	const float radius = height * 0.5f - 2.f;
	const float knobX = *ioValue ? max.x - height * 0.5f : min.x + height * 0.5f;
	draw->AddCircleFilled({knobX, min.y + height * 0.5f}, radius,
						  ImGui::GetColorU32(vec(*ioValue ? theme.onAccent : theme.textMuted)));
	return clicked;
}

auto checkbox(const char* iLabel, bool* ioValue) -> bool {
	const auto& theme = UiLayer::getTheme();
	const char* end = visibleEnd(iLabel);
	const float frame = ImGui::GetFrameHeight();
	const float box = std::round(ImGui::GetFontSize() * 0.9f);
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float labelWidth = end != iLabel ? ImGui::CalcTextSize(iLabel, end).x + spacing : 0.f;
	const bool clicked = ImGui::InvisibleButton(iLabel, {box + labelWidth, frame});
	if (clicked) {
		*ioValue = !*ioValue;
		ImGui::MarkItemEdited(ImGui::GetItemID());
	}
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 itemMin = ImGui::GetItemRectMin();
	const ImVec2 min{itemMin.x, std::floor(itemMin.y + (frame - box) * 0.5f)};
	const ImVec2 max{min.x + box, min.y + box};
	auto* draw = ImGui::GetWindowDrawList();
	const float rounding = std::min(theme.frameRounding, box * 0.25f);
	if (*ioValue) {
		draw->AddRectFilled(
				min, max, ImGui::GetColorU32(vec(hovered ? Theme::mix(theme.accent, theme.text, 0.15f) : theme.accent)),
				rounding);
		const float s = box / 14.f;
		const ImVec2 points[3] = {{min.x + 3.5f * s, min.y + 7.2f * s},
								  {min.x + 6.f * s, min.y + 9.7f * s},
								  {min.x + 10.6f * s, min.y + 4.6f * s}};
		draw->AddPolyline(points, 3, ImGui::GetColorU32(vec(theme.onAccent)), ImDrawFlags_None,
						  std::max(1.5f, 1.7f * s));
	} else {
		draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(theme.well)), rounding);
		draw->AddRect(min, max, ImGui::GetColorU32(vec(hovered ? theme.textMuted : theme.textDisabled)), rounding);
	}
	if (end != iLabel)
		draw->AddText({max.x + spacing, std::floor(itemMin.y + (frame - ImGui::GetTextLineHeight()) * 0.5f)},
					  ImGui::GetColorU32(ImGuiCol_Text), iLabel, end);
	return clicked;
}

}// namespace owl::gui::widgets
