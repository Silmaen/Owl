/**
 * @file SearchField.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/SearchField.h"

#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

namespace owl::gui::widgets {

namespace {

auto containsIgnoringCase(const std::string_view iText, const std::string_view iWord) -> bool {
	const auto lower = [](const char iChar) -> char {
		return static_cast<char>(std::tolower(static_cast<unsigned char>(iChar)));
	};
	return !std::ranges::search(iText, iWord, {}, lower, lower).empty();
}

}// namespace

auto matchesFilter(const std::string_view iText, const std::string_view iFilter) -> bool {
	size_t pos = 0;
	while (pos < iFilter.size()) {
		const size_t start = iFilter.find_first_not_of(' ', pos);
		if (start == std::string_view::npos)
			break;
		const size_t end = std::min(iFilter.find(' ', start), iFilter.size());
		if (!containsIgnoringCase(iText, iFilter.substr(start, end - start)))
			return false;
		pos = end;
	}
	return true;
}

auto searchField(const char* iId, std::string& ioText, const char* iHint) -> bool {
	const auto& theme = UiLayer::getTheme();
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	const float height = ImGui::GetFrameHeight();
	const float glyphWidth = ImGui::CalcTextSize(icon::search).x;
	ImGui::PushID(iId);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {style.FramePadding.x * 2.f + glyphWidth, style.FramePadding.y});
	ImGui::SetNextItemWidth(width);
	ImGui::SetNextItemAllowOverlap();
	bool changed = ImGui::InputTextWithHint("##search", iHint, &ioText, ImGuiInputTextFlags_EscapeClearsAll);
	ImGui::PopStyleVar();
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddText({origin.x + style.FramePadding.x, origin.y + style.FramePadding.y},
				  ImGui::GetColorU32(vec(theme.textDisabled)), icon::search);
	if (!ioText.empty()) {
		ImGui::SetCursorScreenPos({origin.x + width - height, origin.y});
		if (ImGui::InvisibleButton("##clear", {height, height})) {
			ioText.clear();
			changed = true;
		}
		const ImVec2 glyphSize = ImGui::CalcTextSize(icon::x);
		const auto color = ImGui::IsItemHovered() ? theme.text : theme.textMuted;
		draw->AddText({std::floor(origin.x + width - height + (height - glyphSize.x) * 0.5f),
					   std::floor(origin.y + (height - glyphSize.y) * 0.5f)},
					  ImGui::GetColorU32(vec(color)), icon::x);
		ImGui::SetCursorScreenPos({origin.x, origin.y + height + style.ItemSpacing.y});
		ImGui::Dummy({0.f, 0.f});
	}
	ImGui::PopID();
	return changed;
}

}// namespace owl::gui::widgets
