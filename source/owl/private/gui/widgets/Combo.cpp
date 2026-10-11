/**
 * @file Combo.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Combo.h"

#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <cmath>
#include <cstddef>
#include <span>

namespace owl::gui::widgets {

auto beginCombo(const char* iLabel, const char* iPreview, const int iFlags) -> bool {
	auto* draw = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float width = ImGui::CalcItemWidth();
	const float height = ImGui::GetFrameHeight();
	const bool open = ImGui::BeginCombo(iLabel, iPreview, iFlags | ImGuiComboFlags_NoArrowButton);
	ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
	const ImVec2 size = ImGui::CalcTextSize(icon::chevronDown);
	draw->AddText({std::floor(pos.x + width - size.x - ImGui::GetStyle().FramePadding.x),
				   std::floor(pos.y + (height - size.y) * 0.5f)},
				  ImGui::GetColorU32(vec(UiLayer::getTheme().textMuted)), icon::chevronDown);
	ImGui::PopFont();
	return open;
}

auto combo(const char* iLabel, int* ioCurrent, const std::span<const char* const> iItems) -> bool {
	const int count = static_cast<int>(iItems.size());
	const char* preview = *ioCurrent >= 0 && *ioCurrent < count ? iItems[static_cast<size_t>(*ioCurrent)] : "";
	if (!beginCombo(iLabel, preview))
		return false;
	bool changed = false;
	for (int i = 0; i < count; ++i) {
		const bool selected = i == *ioCurrent;
		if (ImGui::Selectable(iItems[static_cast<size_t>(i)], selected) && !selected) {
			*ioCurrent = i;
			changed = true;
		}
		if (selected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
	return changed;
}

}// namespace owl::gui::widgets
