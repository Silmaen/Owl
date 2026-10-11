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

#include <algorithm>
#include <cctype>
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

auto subHeader(const char* iTitle, const bool iDefaultOpen) -> bool {
	const auto& theme = UiLayer::getTheme();
	auto* storage = ImGui::GetStateStorage();
	const ImGuiID id = ImGui::GetID(iTitle);
	bool open = storage->GetBool(id, iDefaultOpen);
	UiLayer::pushTextStyle(TextStyle::Caption);
	std::string caption = iTitle;
	std::ranges::transform(caption, caption.begin(), [](const char iChar) -> char {
		return static_cast<char>(std::toupper(static_cast<unsigned char>(iChar)));
	});
	const float height = ImGui::GetTextLineHeight() + 8.f;
	ImGui::PushID(iTitle);
	if (ImGui::InvisibleButton("##sub", {ImGui::GetContentRegionAvail().x, height})) {
		open = !open;
		storage->SetBool(id, open);
	}
	const bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();
	const ImVec2 min = ImGui::GetItemRectMin();
	const auto color = ImGui::GetColorU32(vec(hovered ? theme.text : theme.textDisabled));
	auto* draw = ImGui::GetWindowDrawList();
	const char* chevron = open ? icon::chevronDown : icon::chevronRight;
	const float textY = std::floor(min.y + 5.f);
	draw->AddText({min.x, textY}, color, chevron);
	draw->AddText({min.x + ImGui::CalcTextSize(chevron).x + 4.f, textY}, color, caption.c_str());
	UiLayer::popTextStyle();
	return open;
}

}// namespace owl::gui::widgets
