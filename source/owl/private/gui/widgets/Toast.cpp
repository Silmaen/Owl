/**
 * @file Toast.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Toast.h"

#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <utility>

namespace owl::gui::widgets {

namespace {

auto kindColor(const ToastKind iKind) -> math::vec4 {
	const auto& theme = UiLayer::getTheme();
	switch (iKind) {
		case ToastKind::Success:
			return theme.success;
		case ToastKind::Info:
			return theme.info;
		case ToastKind::Warning:
			return theme.warning;
		case ToastKind::Error:
			return theme.error;
	}
	return theme.info;
}

auto kindGlyph(const ToastKind iKind) -> const char* {
	switch (iKind) {
		case ToastKind::Success:
			return icon::circleCheck;
		case ToastKind::Info:
			return icon::info;
		case ToastKind::Warning:
			return icon::triangleAlert;
		case ToastKind::Error:
			return icon::circleX;
	}
	return icon::info;
}

auto fade(const math::vec4& iColor, const float iOpacity) -> ImU32 {
	return ImGui::GetColorU32(ImVec4{iColor.x(), iColor.y(), iColor.z(), iColor.w() * iOpacity});
}

}// namespace

void ToastQueue::push(const ToastKind iKind, std::string iTitle, std::string iDetail) {
	++m_unread;
	if (!m_history.empty() && m_history.back().kind == iKind && m_history.back().title == iTitle &&
		m_history.back().detail == iDetail) {
		++m_history.back().count;
	} else {
		m_history.push_back({.kind = iKind, .title = iTitle, .detail = iDetail, .age = 0.f, .count = 1});
		while (m_history.size() > maxHistory) m_history.pop_front();
	}
	for (auto& toast: m_toasts) {
		if (toast.kind == iKind && toast.title == iTitle && toast.detail == iDetail && toast.age < lifetime) {
			++toast.count;
			toast.age = 0.f;
			return;
		}
	}
	m_toasts.push_back(
			{.kind = iKind, .title = std::move(iTitle), .detail = std::move(iDetail), .age = 0.f, .count = 1});
	while (m_toasts.size() > maxToasts) m_toasts.pop_front();
}

void ToastQueue::update(const float iSeconds) {
	for (auto& toast: m_toasts) toast.age += iSeconds;
	std::erase_if(m_toasts, [](const Toast& iToast) -> bool { return iToast.age >= lifetime + fadeTime; });
}

auto ToastQueue::getOpacity(const float iAge) -> float {
	if (iAge <= lifetime)
		return 1.f;
	return std::clamp(1.f - (iAge - lifetime) / fadeTime, 0.f, 1.f);
}

void ToastQueue::onImGuiRender() const {
	if (m_toasts.empty())
		return;
	const auto& theme = UiLayer::getTheme();
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	auto* draw = ImGui::GetForegroundDrawList(viewport);
	const float fontSize = ImGui::GetFontSize();
	const float margin = fontSize * 1.2f;
	const ImVec2 padding{fontSize * 0.75f, fontSize * 0.55f};
	const float stripe = 3.f;
	float bottom = viewport->WorkPos.y + viewport->WorkSize.y - fontSize * 2.5f;
	for (const auto& toast: m_toasts | std::views::reverse) {
		const float opacity = getOpacity(toast.age);
		const std::string title = toast.count > 1 ? std::format("{} ({}x)", toast.title, toast.count) : toast.title;
		const ImVec2 glyphSize = ImGui::CalcTextSize(kindGlyph(toast.kind));
		const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
		const ImVec2 detailSize = toast.detail.empty() ? ImVec2{0.f, 0.f} : ImGui::CalcTextSize(toast.detail.c_str());
		const float textWidth = std::max(titleSize.x, detailSize.x);
		const float width = std::max(fontSize * 16.f, stripe + padding.x * 3.f + glyphSize.x + textWidth);
		const float height = padding.y * 2.f + titleSize.y + (toast.detail.empty() ? 0.f : detailSize.y + 2.f);
		const ImVec2 min{std::floor(viewport->WorkPos.x + viewport->WorkSize.x - margin - width),
						 std::floor(bottom - height)};
		const ImVec2 max{min.x + width, min.y + height};
		const float rounding = theme.windowRounding;
		draw->AddRectFilled({min.x + 2.f, min.y + 4.f}, {max.x + 2.f, max.y + 4.f},
							fade(Theme::withAlpha(theme.surface0, 0.45f), opacity), rounding);
		draw->AddRectFilled(min, max, fade(theme.surface2, opacity), rounding);
		draw->AddRect(min, max, fade(theme.line, opacity), rounding);
		draw->AddRectFilled(min, {min.x + stripe, max.y}, fade(kindColor(toast.kind), opacity), rounding,
							ImDrawFlags_RoundCornersLeft);
		const float x = min.x + stripe + padding.x;
		draw->AddText({x, min.y + padding.y}, fade(kindColor(toast.kind), opacity), kindGlyph(toast.kind));
		const float textX = x + glyphSize.x + padding.x;
		draw->AddText({textX, min.y + padding.y}, fade(theme.text, opacity), title.c_str());
		if (!toast.detail.empty())
			draw->AddText({textX, min.y + padding.y + titleSize.y + 2.f}, fade(theme.textMuted, opacity),
						  toast.detail.c_str());
		bottom = min.y - fontSize * 0.5f;
	}
}

void ToastQueue::renderHistory() const {
	const auto& theme = UiLayer::getTheme();
	if (m_history.empty()) {
		ImGui::TextColored(vec(theme.textDisabled), "No notification yet.");
		return;
	}
	for (const auto& toast: m_history | std::views::reverse) {
		ImGui::TextColored(vec(kindColor(toast.kind)), "%s", kindGlyph(toast.kind));
		ImGui::SameLine();
		ImGui::BeginGroup();
		if (toast.count > 1)
			ImGui::Text("%s (%ux)", toast.title.c_str(), toast.count);
		else
			ImGui::TextUnformatted(toast.title.c_str());
		if (!toast.detail.empty())
			ImGui::TextColored(vec(theme.textMuted), "%s", toast.detail.c_str());
		ImGui::EndGroup();
	}
}

}// namespace owl::gui::widgets
