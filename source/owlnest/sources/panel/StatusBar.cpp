/**
 * @file StatusBar.cpp
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "StatusBar.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/Toast.h>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>

namespace owl::nest::panel {

namespace {

// Middle dot separator, UTF-8.
constexpr auto g_separator = " \xC2\xB7 ";
auto toU32(const math::vec4& iColor) -> ImU32 {
	return ImGui::GetColorU32(ImVec4{iColor.x(), iColor.y(), iColor.z(), iColor.w()});
}

auto plural(const uint64_t iCount, const char* iWord) -> std::string {
	return std::format("{} {}{}", iCount, iWord, iCount == 1 ? "" : "s");
}

auto getPillColor(const StatusInfo::Mode iMode) -> ImU32 {
	const auto& theme = gui::UiLayer::getTheme();
	switch (iMode) {
		case StatusInfo::Mode::Edit:
			return toU32(theme.surface4);
		case StatusInfo::Mode::Play:
			return toU32(theme.play);
		case StatusInfo::Mode::Pause:
			return toU32(theme.warning);
	}
	return toU32(theme.surface4);
}

}// namespace

auto StatusBar::formatElapsed(const float iSeconds) -> std::string {
	const auto seconds = static_cast<int64_t>(std::floor(std::max(iSeconds, 0.f)));
	if (seconds < 5)
		return "just now";
	if (seconds < 60)
		return std::format("{} s ago", seconds);
	if (seconds < 3600)
		return std::format("{} min ago", seconds / 60);
	return std::format("{} h ago", seconds / 3600);
}

auto StatusBar::getModeLabel(const StatusInfo::Mode iMode) -> std::string {
	switch (iMode) {
		case StatusInfo::Mode::Edit:
			return "EDIT";
		case StatusInfo::Mode::Play:
			return "PLAYING";
		case StatusInfo::Mode::Pause:
			return "PAUSED";
	}
	return "EDIT";
}

void StatusBar::onImGuiRender(const StatusInfo& iInfo) {
	const ImGuiStyle& style = ImGui::GetStyle();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	const float height = ImGui::GetFrameHeightWithSpacing();
	const float spacing = style.ItemSpacing.x;
	const float textHeight = ImGui::GetTextLineHeight();
	const float textY = origin.y + (height - textHeight) * 0.5f;
	auto* draw = ImGui::GetWindowDrawList();
	const auto& theme = gui::UiLayer::getTheme();

	const bool simulating = iInfo.mode != StatusInfo::Mode::Edit;
	draw->AddRectFilled(origin, {origin.x + width, origin.y + height},
						simulating ? toU32(gui::Theme::mix(theme.surface0, theme.play, 0.22f)) : toU32(theme.surface0));
	draw->AddLine(origin, {origin.x + width, origin.y}, toU32(theme.lineSoft));

	float x = origin.x + spacing;
	const auto mode = getModeLabel(iInfo.mode);
	const ImVec2 modeSize = ImGui::CalcTextSize(mode.c_str());
	draw->AddRectFilled({x, textY - 1.f}, {x + modeSize.x + 2.f * spacing, textY + textHeight + 1.f},
						getPillColor(iInfo.mode), 3.f);
	draw->AddText({x + spacing, textY}, ImGui::GetColorU32(ImGuiCol_Text), mode.c_str());
	x += modeSize.x + 4.f * spacing;

	std::string document = iInfo.document.empty() ? std::string{"No document"} : iInfo.document;
	if (iInfo.entityCount.has_value())
		document += std::format("{}{} {}", g_separator, *iInfo.entityCount,
								*iInfo.entityCount == 1 ? "entity" : "entities");
	draw->AddText({x, textY}, ImGui::GetColorU32(ImGuiCol_TextDisabled), document.c_str());

	float right = origin.x + width - spacing;
	const auto counter = [&](const char* iId, const std::string& iText, const ImU32 iColor) -> void {
		const ImVec2 size = ImGui::CalcTextSize(iText.c_str());
		right -= size.x + 2.f * spacing;
		ImGui::SetCursorScreenPos({right, origin.y});
		if (ImGui::InvisibleButton(iId, {size.x + 2.f * spacing, height})) {
			acknowledge(iInfo.warningCount, iInfo.errorCount);
			m_openLogRequested = true;
		}
		if (ImGui::IsItemHovered()) {
			draw->AddRectFilled({right, origin.y + 2.f}, {right + size.x + 2.f * spacing, origin.y + height - 2.f},
								ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.f);
			ImGui::SetTooltip("Open the Log");
		}
		draw->AddText({right + spacing, textY}, iColor, iText.c_str());
	};
	const auto label = [&](const std::string& iText) -> void {
		const ImVec2 size = ImGui::CalcTextSize(iText.c_str());
		right -= size.x + 2.f * spacing;
		draw->AddText({right + spacing, textY}, ImGui::GetColorU32(ImGuiCol_TextDisabled), iText.c_str());
	};

	const auto errors = getUnseenErrors(iInfo.errorCount);
	const auto warnings = getUnseenWarnings(iInfo.warningCount);
	counter("##statusErrors", std::format("{} {}", gui::icon::circleX, plural(errors, "error")),
			errors > 0 ? toU32(theme.error) : ImGui::GetColorU32(ImGuiCol_TextDisabled));
	counter("##statusWarnings", std::format("{} {}", gui::icon::triangleAlert, plural(warnings, "warning")),
			warnings > 0 ? toU32(theme.warning) : ImGui::GetColorU32(ImGuiCol_TextDisabled));
	if (iInfo.notifications != nullptr) {
		const size_t unread = iInfo.notifications->getUnreadCount();
		const auto text = unread > 0 ? std::format("{} {}", gui::icon::bell, unread) : std::string{gui::icon::bell};
		const ImVec2 size = ImGui::CalcTextSize(text.c_str());
		right -= size.x + 2.f * spacing;
		ImGui::SetCursorScreenPos({right, origin.y});
		if (ImGui::InvisibleButton("##statusBell", {size.x + 2.f * spacing, height})) {
			ImGui::OpenPopup("##notifications");
			iInfo.notifications->markRead();
		}
		const bool hovered = ImGui::IsItemHovered();
		if (hovered)
			draw->AddRectFilled({right, origin.y + 2.f}, {right + size.x + 2.f * spacing, origin.y + height - 2.f},
								ImGui::GetColorU32(ImGuiCol_ButtonHovered), 3.f);
		if (hovered && !ImGui::IsPopupOpen("##notifications"))
			ImGui::SetTooltip("Notifications");
		draw->AddText({right + spacing, textY},
					  unread > 0 ? toU32(theme.accent) : ImGui::GetColorU32(ImGuiCol_TextDisabled), text.c_str());
		ImGui::SetNextWindowPos({right + size.x + 2.f * spacing, origin.y}, ImGuiCond_Always, {1.f, 1.f});
		if (ImGui::BeginPopup("##notifications")) {
			gui::widgets::menuHeader("Notifications");
			iInfo.notifications->renderHistory();
			ImGui::EndPopup();
		}
	}
	if (!iInfo.backend.empty())
		label(iInfo.device.empty() ? iInfo.backend : std::format("{}{}{}", iInfo.backend, g_separator, iInfo.device));
	if (iInfo.previews.has_value() && iInfo.previews->second > 0) {
		// A bar filled with the ready share, after its caption.
		const float barWidth = ImGui::GetFontSize() * 6.f;
		const float barHeight = std::max(4.f, std::round(textHeight * 0.35f));
		right -= barWidth + spacing;
		const float barY = std::floor(origin.y + (height - barHeight) * 0.5f);
		const float ratio = static_cast<float>(iInfo.previews->first) / static_cast<float>(iInfo.previews->second);
		draw->AddRectFilled({right, barY}, {right + barWidth, barY + barHeight}, toU32(theme.surface3), barHeight);
		draw->AddRectFilled({right, barY}, {right + std::max(barHeight, barWidth * ratio), barY + barHeight},
							toU32(theme.accent), barHeight);
		label(std::format("{} Previews {}/{}", gui::icon::image, iInfo.previews->first, iInfo.previews->second));
	}
	if (iInfo.secondsSinceAutosave.has_value())
		label(std::format("{} Autosaved {}", gui::icon::save, formatElapsed(*iInfo.secondsSinceAutosave)));

	ImGui::SetCursorScreenPos(origin);
	ImGui::Dummy({width, height});
}

}// namespace owl::nest::panel
