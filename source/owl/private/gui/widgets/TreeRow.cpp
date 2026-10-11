/**
 * @file TreeRow.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/TreeRow.h"

#include "gui/IconBank.h"
#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <cmath>

namespace owl::gui::widgets {

namespace {

auto orDefault(const math::vec4& iColor, const math::vec4& iFallback) -> math::vec4 {
	return iColor.w() > 0.f ? iColor : iFallback;
}

}// namespace

auto treeRow(const char* iId, const TreeRowSpec& iSpec) -> TreeRowResult {
	const auto& theme = UiLayer::getTheme();
	UiLayer::pushTextStyle(TextStyle::Label);
	const float fontSize = ImGui::GetFontSize();
	const float height = std::round(fontSize * 1.7f);
	const float x0 = ImGui::GetCursorScreenPos().x;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding |
							   ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
							   ImGuiTreeNodeFlags_AllowOverlap;
	if (!iSpec.hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (iSpec.defaultOpen)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	// The node draws nothing visible: the chevron, icon and label are drawn below.
	ImGui::PushStyleColor(ImGuiCol_Text, {0.f, 0.f, 0.f, 0.f});
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, vec(theme.surface3));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, vec(theme.surface4));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.f, std::floor((height - fontSize) * 0.5f)});
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 0.f});
	const bool open = ImGui::TreeNodeEx(iId, flags, "%s", "");
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	TreeRowResult result;
	result.open = open && iSpec.hasChildren;
	result.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	result.min = {min.x, min.y};
	result.max = {max.x, max.y};
	result.hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
					 ImGui::IsMouseHoveringRect(min, max, false);

	auto* draw = ImGui::GetWindowDrawList();
	if (iSpec.selected) {
		draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(theme.getSelection())));
		draw->AddRectFilled(min, {min.x + 2.f, max.y}, ImGui::GetColorU32(vec(theme.accent)));
	}
	const float textY = std::floor(min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f);
	float x = x0 + 4.f;
	if (iSpec.hasChildren) {
		const char* chevron = open ? icon::chevronDown : icon::chevronRight;
		ImGui::PushFont(nullptr, fontSize * 0.85f);
		const ImVec2 size = ImGui::CalcTextSize(chevron);
		draw->AddText({std::floor(x + (fontSize - size.x) * 0.5f), std::floor(min.y + (max.y - min.y - size.y) * 0.5f)},
					  ImGui::GetColorU32(vec(result.hovered ? theme.textMuted : theme.textDisabled)), chevron);
		ImGui::PopFont();
	}
	x += fontSize + 4.f;
	if (iSpec.glyph != nullptr) {
		draw->AddText({x, textY}, ImGui::GetColorU32(vec(orDefault(iSpec.glyphColor, theme.textMuted))), iSpec.glyph);
		x += fontSize + 6.f;
	} else if (iSpec.iconName != nullptr) {
		if (const auto info = IconBank::instance().getIcon(iSpec.iconName, fontSize); info.has_value()) {
			const float y = std::floor(min.y + (max.y - min.y - fontSize) * 0.5f);
			draw->AddImage(static_cast<ImTextureID>(info->textureId), {x, y}, {x + fontSize, y + fontSize},
						   vec(info->uv0), vec(info->uv1));
		}
		x += fontSize + 6.f;
	}
	draw->AddText({x, textY}, ImGui::GetColorU32(vec(orDefault(iSpec.labelColor, theme.text))), iSpec.label);
	if (iSpec.trailing != nullptr) {
		x += ImGui::CalcTextSize(iSpec.label).x + 6.f;
		UiLayer::pushTextStyle(TextStyle::Caption);
		const float captionY = std::floor(min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f);
		draw->AddText({x, captionY}, ImGui::GetColorU32(vec(theme.textDisabled)), iSpec.trailing);
		UiLayer::popTextStyle();
	}
	UiLayer::popTextStyle();
	return result;
}

void endTreeRow() { ImGui::TreePop(); }

}// namespace owl::gui::widgets
