/**
 * @file Badge.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Badge.h"

#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace owl::gui::widgets {

namespace {

auto kindColor(const BadgeKind iKind) -> math::vec4 {
	const auto& theme = UiLayer::getTheme();
	switch (iKind) {
		case BadgeKind::Neutral:
			return theme.textMuted;
		case BadgeKind::Accent:
			return theme.accent;
		case BadgeKind::Success:
			return theme.success;
		case BadgeKind::Warning:
			return theme.warning;
		case BadgeKind::Error:
			return theme.error;
		case BadgeKind::Info:
			return theme.info;
	}
	return theme.textMuted;
}

auto kindGlyph(const BadgeKind iKind) -> const char* {
	switch (iKind) {
		case BadgeKind::Error:
			return icon::circleX;
		case BadgeKind::Warning:
			return icon::triangleAlert;
		case BadgeKind::Success:
			return icon::check;
		case BadgeKind::Neutral:
		case BadgeKind::Accent:
		case BadgeKind::Info:
			return icon::info;
	}
	return icon::info;
}

}// namespace

void badge(const char* iText, const BadgeKind iKind) {
	const auto& theme = UiLayer::getTheme();
	UiLayer::pushTextStyle(TextStyle::Caption);
	const ImVec2 textSize = ImGui::CalcTextSize(iText);
	const ImVec2 padding{6.f, 1.f};
	const ImVec2 size{textSize.x + 2.f * padding.x, textSize.y + 2.f * padding.y};
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float offsetY = std::floor(std::max(0.f, ImGui::GetFrameHeight() - size.y) * 0.5f);
	const auto color = kindColor(iKind);
	const auto background = iKind == BadgeKind::Neutral ? theme.surface3 : Theme::withAlpha(color, 0.18f);
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled({pos.x, pos.y + offsetY}, {pos.x + size.x, pos.y + offsetY + size.y},
						ImGui::GetColorU32(vec(background)), size.y * 0.5f);
	draw->AddText({pos.x + padding.x, pos.y + offsetY + padding.y}, ImGui::GetColorU32(vec(color)), iText);
	ImGui::Dummy({size.x, ImGui::GetFrameHeight()});
	UiLayer::popTextStyle();
}

void notice(const char* iText, const BadgeKind iKind) {
	const auto color = kindColor(iKind);
	const ImVec2 padding{8.f, 6.f};
	const float width = ImGui::GetContentRegionAvail().x;
	const char* glyph = kindGlyph(iKind);
	const float glyphWidth = ImGui::CalcTextSize(glyph).x + ImGui::GetStyle().ItemInnerSpacing.x;
	const float wrap = std::max(1.f, width - 2.f * padding.x - glyphWidth);
	const ImVec2 textSize = ImGui::CalcTextSize(iText, nullptr, false, wrap);
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 max{pos.x + width, pos.y + textSize.y + 2.f * padding.y};
	auto* draw = ImGui::GetWindowDrawList();
	const float rounding = ImGui::GetStyle().FrameRounding;
	draw->AddRectFilled(pos, max, ImGui::GetColorU32(vec(Theme::withAlpha(color, 0.12f))), rounding);
	draw->AddRect(pos, max, ImGui::GetColorU32(vec(Theme::withAlpha(color, 0.35f))), rounding);
	draw->AddText({pos.x + padding.x, pos.y + padding.y}, ImGui::GetColorU32(vec(color)), glyph);
	draw->AddText(nullptr, 0.f, {pos.x + padding.x + glyphWidth, pos.y + padding.y},
				  ImGui::GetColorU32(vec(UiLayer::getTheme().text)), iText, nullptr, wrap);
	ImGui::Dummy({width, max.y - pos.y});
}

auto chip(const char* iLabel, const bool iSelected) -> bool {
	const auto& theme = UiLayer::getTheme();
	const ImVec2 textSize = ImGui::CalcTextSize(iLabel, nullptr, true);
	const ImVec2 padding{10.f, ImGui::GetStyle().FramePadding.y};
	const ImVec2 size{textSize.x + 2.f * padding.x, ImGui::GetFrameHeight()};
	const bool clicked = ImGui::InvisibleButton(iLabel, size);
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 min = ImGui::GetItemRectMin();
	auto* draw = ImGui::GetWindowDrawList();
	auto background = hovered ? theme.surface3 : theme.surface2;
	if (iSelected)
		background = theme.getSelection();
	draw->AddRectFilled(min, {min.x + size.x, min.y + size.y}, ImGui::GetColorU32(vec(background)), size.y * 0.5f);
	if (iSelected)
		draw->AddRect(min, {min.x + size.x, min.y + size.y},
					  ImGui::GetColorU32(vec(Theme::withAlpha(theme.accent, 0.55f))), size.y * 0.5f);
	auto color = hovered ? theme.text : theme.textMuted;
	if (iSelected)
		color = theme.accent;
	const char* end = std::strstr(iLabel, "##");
	draw->AddText({min.x + padding.x, min.y + (size.y - textSize.y) * 0.5f}, ImGui::GetColorU32(vec(color)), iLabel,
				  end);
	return clicked;
}

}// namespace owl::gui::widgets
