/**
 * @file Menu.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Menu.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <string>

namespace owl::gui::widgets {

auto menuItem(const MenuItemSpec& iSpec) -> bool {
	const auto& theme = UiLayer::getTheme();
	// Spaces before the label reserve the icon column; the glyph is drawn over them.
	const float column = ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x * 2.f;
	const float space = std::max(1.f, ImGui::CalcTextSize(" ").x);
	const std::string label =
			std::format("{}{}", std::string(static_cast<size_t>(std::ceil(column / space)), ' '), iSpec.label);
	if (iSpec.danger)
		ImGui::PushStyleColor(ImGuiCol_Text, vec(theme.error));
	const bool clicked = ImGui::MenuItem(label.c_str(), iSpec.shortcut, false, iSpec.enabled);
	if (iSpec.danger)
		ImGui::PopStyleColor();
	if (iSpec.glyph != nullptr) {
		auto color = iSpec.danger ? theme.error : theme.textMuted;
		if (!iSpec.enabled)
			color = theme.textDisabled;
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const ImVec2 size = ImGui::CalcTextSize(iSpec.glyph);
		ImGui::GetWindowDrawList()->AddText({std::floor(min.x + ImGui::GetStyle().ItemInnerSpacing.x * 0.5f),
											 std::floor(min.y + (max.y - min.y - size.y) * 0.5f)},
											ImGui::GetColorU32(vec(color)), iSpec.glyph);
	}
	return clicked;
}

void menuHeader(const char* iTitle, const char* iKind) {
	const auto& theme = UiLayer::getTheme();
	UiLayer::pushTextStyle(TextStyle::Strong);
	ImGui::TextUnformatted(iTitle);
	UiLayer::popTextStyle();
	if (iKind != nullptr) {
		ImGui::SameLine();
		UiLayer::pushTextStyle(TextStyle::Caption);
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(vec(theme.textDisabled), "%s", iKind);
		UiLayer::popTextStyle();
	}
	ImGui::Separator();
}

}// namespace owl::gui::widgets
