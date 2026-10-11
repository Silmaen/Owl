/**
 * @file Ribbon.cpp
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/Ribbon.h"

#include "gui/IconBank.h"
#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/Tooltip.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace owl::gui::widgets {

namespace {

// Sizes at a 16 px UI font, scaled with the font (`scaled`).
constexpr float g_largeIconSize = 28.0f;
constexpr float g_largeWidthMin = 54.0f;
constexpr float g_largeInnerPadX = 6.0f;
constexpr float g_largeInnerPadY = 5.0f;
constexpr float g_smallIconSize = 16.0f;
constexpr float g_smallLabelPadX = 6.0f;
constexpr float g_smallWidthMin = 76.0f;
constexpr float g_caretSize = 7.0f;
constexpr float g_caretPadX = 4.0f;
constexpr float g_groupInnerPadX = 6.0f;
constexpr float g_groupContentHeight = 66.0f;
constexpr float g_groupLabelGap = 4.0f;
constexpr float g_groupFooterPadding = 5.0f;
// Line height of the group captions, drawn in the caption style.
constexpr float g_captionHeight = 13.0f;
constexpr float g_tabPadX = 14.0f;
// Exactly three small buttons stack in the height of one large button.
constexpr size_t g_maxSmallStack = 3;

// Scaled with the body font, not the current one: the ribbon draws its labels a step smaller.
auto scaled(const float iValue) -> float {
	const auto& style = ImGui::GetStyle();
	return std::round(iValue * style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi / 16.f);
}

auto toU32(const math::vec4& iColor) -> ImU32 { return ImGui::GetColorU32(vec(iColor)); }

void drawButtonBackground(const Ribbon::Button& iButton) {
	const auto& theme = UiLayer::getTheme();
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const bool checked = iButton.isChecked && iButton.isChecked();
	if (!hovered && !active && !checked)
		return;
	math::vec4 color = theme.surface4;
	if (hovered && !checked)
		color = active ? theme.surface4 : theme.surface3;
	ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), toU32(color),
											  theme.frameRounding);
}

// Lucide glyphs sit in the private-use area: their UTF-8 encoding starts with 0xEE or 0xEF.
auto isGlyph(const std::string& iIconName) -> bool {
	return !iIconName.empty() && static_cast<unsigned char>(iIconName.front()) >= 0xEE;
}

void drawButtonIcon(const std::string& iIconName, const ImVec2& iCenter, const float iSize,
					const math::vec4& iColor = UiLayer::getTheme().textMuted) {
	if (isGlyph(iIconName)) {
		ImFont* font = ImGui::GetFont();
		const ImVec2 size = font->CalcTextSizeA(iSize, FLT_MAX, 0.f, iIconName.c_str());
		ImGui::GetWindowDrawList()->AddText(
				font, iSize, {std::floor(iCenter.x - size.x * 0.5f), std::floor(iCenter.y - size.y * 0.5f)},
				toU32(iColor), iIconName.c_str());
		return;
	}
	const auto info = IconBank::instance().getIcon(iIconName, iSize);
	if (!info.has_value())
		return;
	const ImVec2 iconMin{std::floor(iCenter.x - iSize * 0.5f), std::floor(iCenter.y - iSize * 0.5f)};
	const ImVec2 iconMax{iconMin.x + iSize, iconMin.y + iSize};
	ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(info->textureId), iconMin, iconMax, vec(info->uv0),
										 vec(info->uv1));
}

void drawCaret(const ImVec2& iCenter, const float iSize) {
	const ImU32 col = toU32(UiLayer::getTheme().textMuted);
	const float h = iSize * 0.5f;
	ImGui::GetWindowDrawList()->AddTriangleFilled({iCenter.x - h, iCenter.y - h * 0.6f},
												  {iCenter.x + h, iCenter.y - h * 0.6f},
												  {iCenter.x, iCenter.y + h * 0.6f}, col);
}

void showTooltip(const Ribbon::Button& iButton) {
	if (iButton.tooltip.empty() && iButton.shortcut.empty())
		return;
	itemTooltip(iButton.label.empty() ? iButton.tooltip.c_str() : iButton.label.c_str(),
				iButton.shortcut.empty() ? nullptr : iButton.shortcut.c_str(),
				iButton.label.empty() ? nullptr : iButton.tooltip.c_str());
}

void handlePopup(const Ribbon::Button& iButton, const bool iClicked) {
	if (!iButton.popupContents)
		return;
	if (iClicked)
		ImGui::OpenPopup("##rb_popup");
	if (ImGui::BeginPopup("##rb_popup")) {
		iButton.popupContents();
		ImGui::EndPopup();
	}
}

auto renderLargeButton(const Ribbon::Button& iButton) -> bool {
	const auto labelSize = ImGui::CalcTextSize(iButton.label.c_str());
	const float width = std::max(scaled(g_largeWidthMin), labelSize.x + scaled(g_largeInnerPadX) * 2.f);
	const float height = scaled(g_groupContentHeight);
	const bool enabled = iButton.isEnabled ? iButton.isEnabled() : true;
	const bool hasPopup = static_cast<bool>(iButton.popupContents);
	ImGui::PushID(&iButton);
	ImGui::BeginDisabled(!enabled);
	const bool clicked = ImGui::InvisibleButton("##large", ImVec2{width, height}) && enabled;
	const auto rectMin = ImGui::GetItemRectMin();
	const auto rectMax = ImGui::GetItemRectMax();
	drawButtonBackground(iButton);
	const float iconSize = scaled(g_largeIconSize);
	drawButtonIcon(iButton.iconName, {rectMin.x + width * 0.5f, rectMin.y + scaled(g_largeInnerPadY) + iconSize * 0.5f},
				   iconSize, UiLayer::getTheme().text);
	const ImVec2 textPos{std::floor(rectMin.x + (width - labelSize.x) * 0.5f),
						 rectMax.y - labelSize.y - scaled(g_largeInnerPadY)};
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), iButton.label.c_str());
	if (hasPopup)
		drawCaret({rectMax.x - scaled(g_caretSize) - g_caretPadX, rectMax.y - scaled(g_caretSize) - g_largeInnerPadY},
				  scaled(g_caretSize));
	ImGui::EndDisabled();
	showTooltip(iButton);
	handlePopup(iButton, clicked);
	ImGui::PopID();
	return clicked && !hasPopup;
}

auto renderSmallButton(const Ribbon::Button& iButton, const float iColumnWidth, const float iHeight) -> bool {
	const bool enabled = iButton.isEnabled ? iButton.isEnabled() : true;
	const bool hasPopup = static_cast<bool>(iButton.popupContents);
	ImGui::PushID(&iButton);
	ImGui::BeginDisabled(!enabled);
	const bool clicked = ImGui::InvisibleButton("##small", ImVec2{iColumnWidth, iHeight}) && enabled;
	const auto rectMin = ImGui::GetItemRectMin();
	const auto rectMax = ImGui::GetItemRectMax();
	drawButtonBackground(iButton);
	const float cy = (rectMin.y + rectMax.y) * 0.5f;
	const float iconSize = scaled(g_smallIconSize);
	const float pad = scaled(g_smallLabelPadX);
	drawButtonIcon(iButton.iconName, {rectMin.x + pad + iconSize * 0.5f, cy}, iconSize);
	const auto labelSize = ImGui::CalcTextSize(iButton.label.c_str());
	const ImVec2 textPos{rectMin.x + pad + iconSize + pad, std::floor(cy - labelSize.y * 0.5f)};
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), iButton.label.c_str());
	if (hasPopup)
		drawCaret({rectMax.x - scaled(g_caretSize) * 0.5f - g_caretPadX, cy}, scaled(g_caretSize));
	ImGui::EndDisabled();
	showTooltip(iButton);
	handlePopup(iButton, clicked);
	ImGui::PopID();
	return clicked && !hasPopup;
}

auto measureSmallColumnWidth(const std::vector<const Ribbon::Button*>& iStack) -> float {
	float maxLabel = 0.f;
	bool anyHasPopup = false;
	for (const auto* btn: iStack) {
		maxLabel = std::max(maxLabel, ImGui::CalcTextSize(btn->label.c_str()).x);
		anyHasPopup = anyHasPopup || static_cast<bool>(btn->popupContents);
	}
	const float pad = scaled(g_smallLabelPadX);
	const float caretWidth = anyHasPopup ? (scaled(g_caretSize) + g_caretPadX * 2.f) : 0.f;
	return std::max(scaled(g_smallWidthMin), pad + scaled(g_smallIconSize) + pad + maxLabel + pad + caretWidth);
}

void renderGroup(const Ribbon::Group& iGroup) {
	const auto groupOrigin = ImGui::GetCursorScreenPos();
	const float contentHeight = scaled(g_groupContentHeight);
	ImGui::BeginGroup();
	std::vector<const Ribbon::Button*> pendingSmalls;
	bool emitted = false;
	const auto flushSmalls = [&pendingSmalls, &emitted, contentHeight]() -> void {
		if (pendingSmalls.empty())
			return;
		const float colWidth = measureSmallColumnWidth(pendingSmalls);
		const float rowHeight = contentHeight / static_cast<float>(g_maxSmallStack);
		if (emitted)
			ImGui::SameLine();
		emitted = true;
		ImGui::BeginGroup();
		for (const auto* btn: pendingSmalls) {
			if (renderSmallButton(*btn, colWidth, rowHeight))
				btn->onClick();
		}
		ImGui::EndGroup();
		pendingSmalls.clear();
	};
	for (const auto& btn: iGroup.buttons) {
		if (btn.size == Ribbon::ButtonSize::Large) {
			flushSmalls();
			if (emitted)
				ImGui::SameLine();
			emitted = true;
			if (renderLargeButton(btn))
				btn.onClick();
		} else {
			if (pendingSmalls.size() >= g_maxSmallStack)
				flushSmalls();
			pendingSmalls.push_back(&btn);
		}
	}
	flushSmalls();
	ImGui::EndGroup();

	const float groupWidth = ImGui::GetItemRectSize().x;
	UiLayer::pushTextStyle(TextStyle::Caption);
	const auto labelSize = ImGui::CalcTextSize(iGroup.label.c_str());
	const float offset = std::max(0.f, std::floor((groupWidth - labelSize.x) * 0.5f));
	ImGui::GetWindowDrawList()->AddText({groupOrigin.x + offset, groupOrigin.y + contentHeight + g_groupLabelGap},
										toU32(UiLayer::getTheme().textDisabled), iGroup.label.c_str());
	UiLayer::popTextStyle();
}

void renderTabContent(const Ribbon::Tab& iTab) {
	const float totalHeight =
			scaled(g_groupContentHeight) + g_groupLabelGap + scaled(g_captionHeight) + g_groupFooterPadding;
	UiLayer::pushTextStyle(TextStyle::Label);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{scaled(g_groupInnerPadX), 2.f});
	const auto startCursor = ImGui::GetCursorScreenPos();
	auto* drawList = ImGui::GetWindowDrawList();
	const ImU32 separatorColor = toU32(UiLayer::getTheme().line);
	for (size_t gi = 0; gi < iTab.groups.size(); ++gi) {
		if (gi > 0) {
			ImGui::SameLine(0.f, scaled(g_groupInnerPadX) * 2.f);
		}
		ImGui::BeginGroup();
		renderGroup(iTab.groups[gi]);
		ImGui::EndGroup();
		if (gi + 1 < iTab.groups.size()) {
			const float x = std::floor(ImGui::GetItemRectMax().x + scaled(g_groupInnerPadX));
			drawList->AddLine({x, startCursor.y + 4.f}, {x, startCursor.y + totalHeight - 4.f}, separatorColor);
		}
	}
	ImGui::Dummy(ImVec2{1, totalHeight});
	ImGui::PopStyleVar();
	UiLayer::popTextStyle();
}

// Icon-only button of the tab row; returns true on click.
auto quickButton(const Ribbon::Button& iButton, const float iSide) -> bool {
	const bool enabled = iButton.isEnabled ? iButton.isEnabled() : true;
	ImGui::PushID(&iButton);
	ImGui::BeginDisabled(!enabled);
	const bool clicked = ImGui::InvisibleButton("##quick", {iSide, iSide}) && enabled;
	drawButtonBackground(iButton);
	const auto min = ImGui::GetItemRectMin();
	drawButtonIcon(iButton.iconName, {min.x + iSide * 0.5f, min.y + iSide * 0.5f}, scaled(g_smallIconSize));
	ImGui::EndDisabled();
	itemTooltip(iButton.label.c_str(), iButton.shortcut.empty() ? nullptr : iButton.shortcut.c_str(),
				iButton.tooltip.empty() ? nullptr : iButton.tooltip.c_str());
	ImGui::PopID();
	return clicked;
}

}// namespace

auto Ribbon::addTab(std::string iLabel) -> size_t {
	m_tabs.push_back({std::move(iLabel), {}, false});
	return m_tabs.size() - 1;
}

void Ribbon::setTabHighlighted(const size_t iTabIndex, const bool iHighlighted) {
	if (iTabIndex < m_tabs.size())
		m_tabs[iTabIndex].highlighted = iHighlighted;
}

auto Ribbon::addGroup(const size_t iTabIndex, std::string iLabel) -> size_t {
	m_tabs[iTabIndex].groups.push_back({std::move(iLabel), {}});
	return m_tabs[iTabIndex].groups.size() - 1;
}

void Ribbon::addButton(const size_t iTabIndex, const size_t iGroupIndex, Button iButton) {
	m_tabs[iTabIndex].groups[iGroupIndex].buttons.push_back(std::move(iButton));
}

void Ribbon::clear() {
	m_tabs.clear();
	m_quickAccess.clear();
}

void Ribbon::setBackstage(std::string iLabel, std::function<void()> iOnClick) {
	m_backstageLabel = std::move(iLabel);
	m_onBackstage = std::move(iOnClick);
}

void Ribbon::addQuickAccess(Button iButton) { m_quickAccess.push_back(std::move(iButton)); }

void Ribbon::setSearchBox(std::string iHint, std::string iShortcut, std::function<void()> iOnClick) {
	m_searchHint = std::move(iHint);
	m_searchShortcut = std::move(iShortcut);
	m_onSearch = std::move(iOnClick);
}

void Ribbon::selectTab(const std::string& iLabel) {
	for (size_t i = 0; i < m_tabs.size(); ++i) {
		if (m_tabs[i].label == iLabel) {
			m_selected = i;
			m_selectedLabel = iLabel;
			return;
		}
	}
}

auto Ribbon::getSelectedTab() const -> std::string {
	return m_selected < m_tabs.size() ? m_tabs[m_selected].label : std::string{};
}

auto Ribbon::getHeight() const -> float {
	const float tabRow = ImGui::GetFrameHeight() + 4.f;
	if (m_collapsed)
		return tabRow;
	return tabRow + scaled(g_groupContentHeight) + g_groupLabelGap + scaled(g_captionHeight) + g_groupFooterPadding +
		   8.f;
}

void Ribbon::renderTabRow() {
	const auto& theme = UiLayer::getTheme();
	auto* draw = ImGui::GetWindowDrawList();
	const float height = ImGui::GetFrameHeight();
	const float rowTop = ImGui::GetCursorScreenPos().y;
	const float left = ImGui::GetCursorScreenPos().x;
	const float right = left + ImGui::GetContentRegionAvail().x;
	draw->AddRectFilled({left - 8.f, rowTop - 4.f}, {right + 8.f, rowTop + height + 4.f}, toU32(theme.surface0));
	ImGui::SetCursorScreenPos({left, rowTop + 2.f});

	if (!m_backstageLabel.empty()) {
		const float width = ImGui::CalcTextSize(m_backstageLabel.c_str()).x + scaled(g_tabPadX) * 2.f;
		if (ImGui::InvisibleButton("##backstage", {width, height}) && m_onBackstage)
			m_onBackstage();
		const auto min = ImGui::GetItemRectMin();
		const auto max = ImGui::GetItemRectMax();
		const auto background = ImGui::IsItemHovered() ? Theme::mix(theme.accent, theme.text, 0.15f) : theme.accent;
		draw->AddRectFilled(min, max, toU32(background), theme.frameRounding);
		UiLayer::pushTextStyle(TextStyle::Strong);
		const auto size = ImGui::CalcTextSize(m_backstageLabel.c_str());
		draw->AddText({std::floor(min.x + (width - size.x) * 0.5f), std::floor(min.y + (height - size.y) * 0.5f)},
					  toU32(theme.onAccent), m_backstageLabel.c_str());
		UiLayer::popTextStyle();
		itemTooltip(m_backstageLabel.c_str(), nullptr, "Projects, packaging, settings, help and exit.");
		ImGui::SameLine(0.f, scaled(8.f));
	}

	if (!m_selectedLabel.empty())
		selectTab(m_selectedLabel);
	m_selected = std::min(m_selected, m_tabs.empty() ? size_t{0} : m_tabs.size() - 1);
	for (size_t i = 0; i < m_tabs.size(); ++i) {
		auto& tab = m_tabs[i];
		const bool selected = i == m_selected && !m_collapsed;
		const float width = ImGui::CalcTextSize(tab.label.c_str()).x + scaled(g_tabPadX) * 2.f;
		ImGui::PushID(static_cast<int>(i));
		if (ImGui::InvisibleButton("##tab", {width, height})) {
			if (i == m_selected)
				m_collapsed = !m_collapsed;
			else
				m_collapsed = false;
			m_selected = i;
			m_selectedLabel = tab.label;
		}
		const auto min = ImGui::GetItemRectMin();
		const auto max = ImGui::GetItemRectMax();
		if (selected)
			draw->AddRectFilled(min, {max.x, max.y + 4.f}, toU32(theme.surface2), theme.tabRounding,
								ImDrawFlags_RoundCornersTop);
		else if (ImGui::IsItemHovered())
			draw->AddRectFilled(min, max, toU32(theme.surface1), theme.tabRounding);
		if (tab.highlighted)
			draw->AddRectFilled({min.x + 4.f, min.y}, {max.x - 4.f, min.y + 2.f}, toU32(theme.accent), 1.f);
		auto color = selected ? theme.text : theme.textMuted;
		if (tab.highlighted)
			color = theme.accent;
		const auto size = ImGui::CalcTextSize(tab.label.c_str());
		draw->AddText({std::floor(min.x + (width - size.x) * 0.5f), std::floor(min.y + (height - size.y) * 0.5f)},
					  toU32(color), tab.label.c_str());
		ImGui::PopID();
		ImGui::SameLine(0.f, 2.f);
	}

	// Quick access and the collapse toggle, right-aligned.
	const float side = height;
	const float spacing = 2.f;
	const float quickWidth = static_cast<float>(m_quickAccess.size() + 1) * (side + spacing);
	if (!m_searchHint.empty()) {
		const float tabsEnd = ImGui::GetCursorScreenPos().x + scaled(12.f);
		const float available = right - quickWidth - scaled(12.f) - tabsEnd;
		const float width = std::min(scaled(380.f), available);
		if (width > scaled(140.f)) {
			const float x = std::max(tabsEnd, std::floor((left + right - width) * 0.5f));
			ImGui::SetCursorScreenPos({x, rowTop + 2.f});
			if (ImGui::InvisibleButton("##ribbonSearch", {width, height}) && m_onSearch)
				m_onSearch();
			const bool hovered = ImGui::IsItemHovered();
			const auto min = ImGui::GetItemRectMin();
			const auto max = ImGui::GetItemRectMax();
			draw->AddRectFilled(min, max, toU32(theme.surface2), theme.frameRounding);
			draw->AddRect(min, max, toU32(hovered ? theme.surface4 : theme.line), theme.frameRounding);
			UiLayer::pushTextStyle(TextStyle::Label);
			const float textY = std::floor(min.y + (height - ImGui::GetTextLineHeight()) * 0.5f);
			const auto textColor = toU32(hovered ? theme.textMuted : theme.textDisabled);
			const float pad = scaled(8.f);
			draw->AddText({min.x + pad, textY}, textColor, icon::search);
			draw->AddText({min.x + pad + ImGui::CalcTextSize(icon::search).x + scaled(6.f), textY}, textColor,
						  m_searchHint.c_str());
			if (!m_searchShortcut.empty()) {
				const auto size = ImGui::CalcTextSize(m_searchShortcut.c_str());
				const ImVec2 kbdMin{max.x - pad - size.x - 8.f, min.y + 4.f};
				draw->AddRectFilled(kbdMin, {max.x - pad, max.y - 4.f}, toU32(theme.surface3), theme.frameRounding);
				draw->AddText({kbdMin.x + 4.f, textY}, toU32(theme.textMuted), m_searchShortcut.c_str());
			}
			UiLayer::popTextStyle();
		}
	}
	ImGui::SetCursorScreenPos({right - quickWidth, rowTop + 2.f});
	for (const auto& button: m_quickAccess) {
		if (quickButton(button, side))
			button.onClick();
		ImGui::SameLine(0.f, spacing);
	}
	if (ImGui::InvisibleButton("##collapse", {side, side}))
		m_collapsed = !m_collapsed;
	if (ImGui::IsItemHovered())
		draw->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), toU32(theme.surface3),
							theme.frameRounding);
	const char* glyph = m_collapsed ? icon::chevronDown : icon::chevronUp;
	const auto glyphSize = ImGui::CalcTextSize(glyph);
	const auto min = ImGui::GetItemRectMin();
	draw->AddText({std::floor(min.x + (side - glyphSize.x) * 0.5f), std::floor(min.y + (side - glyphSize.y) * 0.5f)},
				  toU32(theme.textMuted), glyph);
	itemTooltip(m_collapsed ? "Expand the ribbon" : "Collapse the ribbon", "Ctrl+F1",
				"Keep only the tab names; click a tab to show its buttons again.");
	ImGui::SetCursorScreenPos({left, rowTop + height + 4.f});
	ImGui::Dummy({0.f, 0.f});
}

void Ribbon::onRender() {
	if (m_tabs.empty())
		return;
	const auto& theme = UiLayer::getTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{8.f, 4.f});
	ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(theme.surface2));
	ImGui::BeginChild("##ribbon", ImVec2{0.f, getHeight()}, ImGuiChildFlags_AlwaysUseWindowPadding,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderTabRow();
	if (!m_collapsed && m_selected < m_tabs.size()) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f);
		renderTabContent(m_tabs[m_selected]);
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

}// namespace owl::gui::widgets
