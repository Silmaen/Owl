/**
 * @file SectionHeader.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/SectionHeader.h"

#include "gui/IconBank.h"
#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/Tooltip.h"

#include <imgui.h>

#include <cmath>
#include <format>
#include <string>

namespace owl::gui::widgets {

auto sectionHeader(const char* iTitle, const char* iIconName, const bool iWithMenu, const bool iDefaultOpen)
		-> SectionHeaderResult {
	const auto& theme = UiLayer::getTheme();
	ImGui::PushStyleColor(ImGuiCol_Header, vec(theme.surface2));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, vec(theme.surface3));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, vec(theme.surface4));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
						{ImGui::GetStyle().FramePadding.x, ImGui::GetStyle().FramePadding.y + 1.f});
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
							   ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
	if (iDefaultOpen)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	SectionHeaderResult result;
	result.open = ImGui::TreeNodeEx(std::format("##section_{}", iTitle).c_str(), flags);
	result.hovered = ImGui::IsItemHovered();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	result.min = {min.x, min.y};
	result.max = {max.x, max.y};
	const float height = max.y - min.y;
	auto* draw = ImGui::GetWindowDrawList();
	float x = std::floor(min.x + ImGui::GetTreeNodeToLabelSpacing());
	const float iconSize = ImGui::GetFontSize();
	if (iIconName != nullptr) {
		if (const auto icon = IconBank::instance().getIcon(iIconName, iconSize); icon.has_value()) {
			const float y = std::floor(min.y + (height - iconSize) * 0.5f);
			draw->AddImage(static_cast<ImTextureID>(icon->textureId), {x, y}, {x + iconSize, y + iconSize},
						   vec(icon->uv0), vec(icon->uv1));
		}
		x += iconSize + ImGui::GetStyle().ItemInnerSpacing.x;
	}
	UiLayer::pushTextStyle(TextStyle::Section);
	const ImVec2 titleSize = ImGui::CalcTextSize(iTitle);
	draw->AddText({x, std::floor(min.y + (height - titleSize.y) * 0.5f)}, ImGui::GetColorU32(vec(theme.text)), iTitle);
	UiLayer::popTextStyle();

	if (iWithMenu) {
		ImGui::SetCursorScreenPos({max.x - height, min.y});
		ImGui::PushID(iTitle);
		result.menuClicked = ImGui::InvisibleButton("##sectionMenu", {height, height});
		if (ImGui::IsItemHovered())
			draw->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
								ImGui::GetColorU32(vec(theme.surface4)), theme.frameRounding);
		const ImVec2 glyphSize = ImGui::CalcTextSize(icon::ellipsisVertical);
		draw->AddText({std::floor(max.x - height + (height - glyphSize.x) * 0.5f),
					   std::floor(min.y + (height - glyphSize.y) * 0.5f)},
					  ImGui::GetColorU32(vec(theme.textMuted)), icon::ellipsisVertical);
		itemTooltip("Options", nullptr, "Reset, copy, paste or remove this section.");
		ImGui::PopID();
	}
	return result;
}

void endSection() { ImGui::TreePop(); }

}// namespace owl::gui::widgets
