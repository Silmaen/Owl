/**
 * @file ImageTile.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/ImageTile.h"

#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <tuple>

namespace owl::gui::widgets {

auto imageTile(const char* iId, const ImageTileSpec& iSpec) -> ImageTileResult {
	const auto& theme = UiLayer::getTheme();
	std::ignore = ImGui::InvisibleButton(iId, {iSpec.size, iSpec.size},
										 ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	ImageTileResult result;
	result.hovered = ImGui::IsItemHovered();
	result.left = result.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	result.right = result.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(min, max, ImGui::GetColorU32(vec(theme.well)), theme.frameRounding);
	if (iSpec.textureId != 0)
		draw->AddImageRounded(static_cast<ImTextureID>(iSpec.textureId), {min.x + 2.f, min.y + 2.f},
							  {max.x - 2.f, max.y - 2.f}, vec(iSpec.uv0), vec(iSpec.uv1), IM_COL32_WHITE,
							  theme.frameRounding);
	if (iSpec.outline.w() > 0.f)
		draw->AddRect(min, max, ImGui::GetColorU32(vec(iSpec.outline)), theme.frameRounding, ImDrawFlags_None, 2.f);
	else if (result.hovered)
		draw->AddRect(min, max, ImGui::GetColorU32(vec(theme.textMuted)), theme.frameRounding);
	return result;
}

}// namespace owl::gui::widgets
