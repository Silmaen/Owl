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
#include "gui/utils.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

namespace owl::gui::widgets {

namespace {
constexpr float sk_largeIconSize = 32.0f;
constexpr float sk_largeWidthMin = 56.0f;
constexpr float sk_largeInnerPadX = 6.0f;
constexpr float sk_largeInnerPadY = 4.0f;
constexpr float sk_smallIconSize = 16.0f;
constexpr float sk_smallLabelPadX = 6.0f;
constexpr float sk_smallWidthMin = 80.0f;
constexpr float sk_caretSize = 8.0f;
constexpr float sk_caretPadX = 4.0f;
constexpr float sk_groupInnerPadX = 6.0f;
constexpr float sk_groupContentHeight = 72.0f;
constexpr float sk_groupLabelGap = 2.0f;
constexpr float sk_groupFooterPadding = 4.0f;
constexpr size_t sk_maxSmallStack = 3;// exactly three small buttons = one large button's content

void drawButtonBackground(const Ribbon::Button& iButton) {
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	const bool checked = iButton.isChecked && iButton.isChecked();
	if (!hovered && !active && !checked)
		return;
	const ImU32 col =
			hovered && !active ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) : ImGui::GetColorU32(ImGuiCol_ButtonActive);
	ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), col, 3.f);
}

void drawButtonIcon(const std::string& iIconName, const ImVec2& iCenter, const float iSize) {
	const auto info = IconBank::instance().getIcon(iIconName);
	if (!info.has_value())
		return;
	const ImVec2 iconMin{iCenter.x - iSize * 0.5f, iCenter.y - iSize * 0.5f};
	const ImVec2 iconMax{iCenter.x + iSize * 0.5f, iCenter.y + iSize * 0.5f};
	ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(info->textureId), iconMin, iconMax, vec(info->uv0),
										 vec(info->uv1));
}

void drawCaret(const ImVec2& iCenter, const float iSize) {
	const ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
	const float h = iSize * 0.5f;
	const ImVec2 a{iCenter.x - h, iCenter.y - h * 0.6f};
	const ImVec2 b{iCenter.x + h, iCenter.y - h * 0.6f};
	const ImVec2 c{iCenter.x, iCenter.y + h * 0.6f};
	ImGui::GetWindowDrawList()->AddTriangleFilled(a, b, c, col);
}

void maybeShowTooltip(const std::string& iTooltip) {
	if (iTooltip.empty())
		return;
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
		ImGui::SetTooltip("%s", iTooltip.c_str());
}

auto renderLargeButton(const Ribbon::Button& iButton) -> bool {
	const auto labelSize = ImGui::CalcTextSize(iButton.label.c_str());
	const float width = std::max(sk_largeWidthMin, labelSize.x + sk_largeInnerPadX * 2.f);
	const bool enabled = iButton.isEnabled ? iButton.isEnabled() : true;
	const bool hasPopup = static_cast<bool>(iButton.popupContents);

	if (!enabled)
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);

	ImGui::PushID(&iButton);
	const bool clicked = ImGui::InvisibleButton("##large", ImVec2{width, sk_groupContentHeight}) && enabled;
	const auto rectMin = ImGui::GetItemRectMin();
	const auto rectMax = ImGui::GetItemRectMax();
	drawButtonBackground(iButton);
	drawButtonIcon(iButton.iconName,
				   {rectMin.x + width * 0.5f, rectMin.y + sk_largeInnerPadY + sk_largeIconSize * 0.5f},
				   sk_largeIconSize);
	const ImVec2 textPos{rectMin.x + (width - labelSize.x) * 0.5f, rectMax.y - labelSize.y - sk_largeInnerPadY};
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), iButton.label.c_str());
	if (hasPopup)
		drawCaret({rectMax.x - sk_caretSize - sk_caretPadX, rectMax.y - sk_caretSize - sk_largeInnerPadY},
				  sk_caretSize);
	if (enabled)
		maybeShowTooltip(iButton.tooltip);
	if (clicked && hasPopup)
		ImGui::OpenPopup("##rb_popup");
	if (hasPopup && ImGui::BeginPopup("##rb_popup")) {
		iButton.popupContents();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (!enabled)
		ImGui::PopStyleVar();
	return clicked && !hasPopup;
}

auto renderSmallButton(const Ribbon::Button& iButton, const float iColumnWidth, const float iHeight) -> bool {
	const bool enabled = iButton.isEnabled ? iButton.isEnabled() : true;
	const bool hasPopup = static_cast<bool>(iButton.popupContents);
	if (!enabled)
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);

	ImGui::PushID(&iButton);
	const bool clicked = ImGui::InvisibleButton("##small", ImVec2{iColumnWidth, iHeight}) && enabled;
	const auto rectMin = ImGui::GetItemRectMin();
	const auto rectMax = ImGui::GetItemRectMax();
	drawButtonBackground(iButton);
	const float cy = (rectMin.y + rectMax.y) * 0.5f;
	drawButtonIcon(iButton.iconName, {rectMin.x + sk_smallLabelPadX + sk_smallIconSize * 0.5f, cy}, sk_smallIconSize);
	const auto labelSize = ImGui::CalcTextSize(iButton.label.c_str());
	const ImVec2 textPos{rectMin.x + sk_smallLabelPadX + sk_smallIconSize + sk_smallLabelPadX, cy - labelSize.y * 0.5f};
	ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), iButton.label.c_str());
	if (hasPopup)
		drawCaret({rectMax.x - sk_caretSize * 0.5f - sk_caretPadX, cy}, sk_caretSize);
	if (enabled)
		maybeShowTooltip(iButton.tooltip);
	if (clicked && hasPopup)
		ImGui::OpenPopup("##rb_popup");
	if (hasPopup && ImGui::BeginPopup("##rb_popup")) {
		iButton.popupContents();
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if (!enabled)
		ImGui::PopStyleVar();
	return clicked && !hasPopup;
}

auto measureSmallColumnWidth(const std::vector<const Ribbon::Button*>& iStack) -> float {
	float maxLabel = 0.f;
	bool anyHasPopup = false;
	for (const auto* btn: iStack) {
		const auto sz = ImGui::CalcTextSize(btn->label.c_str());
		maxLabel = std::max(maxLabel, sz.x);
		if (btn->popupContents)
			anyHasPopup = true;
	}
	const float caretWidth = anyHasPopup ? (sk_caretSize + sk_caretPadX * 2.f) : 0.f;
	return std::max(sk_smallWidthMin, sk_smallLabelPadX + sk_smallIconSize + sk_smallLabelPadX + maxLabel +
											  sk_smallLabelPadX + caretWidth);
}

void renderGroup(const Ribbon::Group& iGroup) {
	// Remember the starting position so we can place the group label below the content.
	const auto groupOrigin = ImGui::GetCursorScreenPos();

	ImGui::BeginGroup();
	std::vector<const Ribbon::Button*> pendingSmalls;
	const auto flushSmalls = [&pendingSmalls]() -> void {
		if (pendingSmalls.empty())
			return;
		const float colWidth = measureSmallColumnWidth(pendingSmalls);
		const float rowHeight = sk_groupContentHeight / static_cast<float>(sk_maxSmallStack);
		ImGui::SameLine();
		ImGui::BeginGroup();
		for (const auto* btn: pendingSmalls) {
			if (renderSmallButton(*btn, colWidth, rowHeight))
				btn->onClick();
			// the next small falls below (default cursor advance after a child widget in a Group)
		}
		ImGui::EndGroup();
		pendingSmalls.clear();
	};

	bool isFirst = true;
	for (const auto& btn: iGroup.buttons) {
		if (btn.size == Ribbon::ButtonSize::Large) {
			flushSmalls();
			if (!isFirst)
				ImGui::SameLine();
			if (renderLargeButton(btn))
				btn.onClick();
			isFirst = false;
		} else {
			if (pendingSmalls.size() >= sk_maxSmallStack)
				flushSmalls();
			if (pendingSmalls.empty() && !isFirst)
				ImGui::SameLine();
			pendingSmalls.push_back(&btn);
			isFirst = false;
		}
	}
	flushSmalls();
	ImGui::EndGroup();

	const float groupWidth = ImGui::GetItemRectSize().x;
	// Label under the content, centred.
	const auto labelSize = ImGui::CalcTextSize(iGroup.label.c_str());
	const float offset = std::max(0.f, (groupWidth - labelSize.x) * 0.5f);
	ImGui::GetWindowDrawList()->AddText(
			{groupOrigin.x + offset, groupOrigin.y + sk_groupContentHeight + sk_groupLabelGap},
			ImGui::GetColorU32(ImGuiCol_TextDisabled), iGroup.label.c_str());
}

void renderTabContent(const Ribbon::Tab& iTab) {
	const float totalHeight = sk_groupContentHeight + sk_groupLabelGap + ImGui::GetFontSize() + sk_groupFooterPadding;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{sk_groupInnerPadX, 2.f});

	const auto startCursor = ImGui::GetCursorScreenPos();
	auto* drawList = ImGui::GetWindowDrawList();
	const ImU32 separatorColor = ImGui::GetColorU32(ImGuiCol_Separator);

	for (size_t gi = 0; gi < iTab.groups.size(); ++gi) {
		if (gi > 0)
			ImGui::SameLine();
		ImGui::BeginGroup();
		renderGroup(iTab.groups[gi]);
		ImGui::EndGroup();
		// Draw a subtle vertical separator on the right of each group except the last.
		if (gi + 1 < iTab.groups.size()) {
			const auto rectMax = ImGui::GetItemRectMax();
			const float x = rectMax.x + sk_groupInnerPadX * 0.5f;
			drawList->AddLine({x, startCursor.y + 2}, {x, startCursor.y + totalHeight - 2}, separatorColor);
		}
	}
	// Advance the cursor past the full ribbon content height so external callers see it.
	ImGui::Dummy(ImVec2{1, totalHeight});

	ImGui::PopStyleVar();
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

void Ribbon::clear() { m_tabs.clear(); }

auto Ribbon::height() -> float {
	// Tab bar (font size + 2 * FramePadding.y = 18 + 12 ≈ 30) + group content + label row + padding.
	return 30.f + sk_groupContentHeight + sk_groupLabelGap + 16.f + sk_groupFooterPadding + 4.f;
}

void Ribbon::onRender() {
	if (m_tabs.empty())
		return;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{4.f, 2.f});
	ImGui::BeginChild("##ribbon", ImVec2{0.f, height()}, ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{14.f, 6.f});
	const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_TabSelectedOverline);
	ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));

	if (ImGui::BeginTabBar("##ribbon_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
		for (auto& tab: m_tabs) {
			const bool pushText = tab.highlighted;
			if (pushText)
				ImGui::PushStyleColor(ImGuiCol_Text, accent);
			const bool opened = ImGui::BeginTabItem(tab.label.c_str());
			if (pushText)
				ImGui::PopStyleColor();
			if (opened) {
				renderTabContent(tab);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

}// namespace owl::gui::widgets
