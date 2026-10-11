/**
 * @file PropertyGrid.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "gui/widgets/PropertyGrid.h"

#include "gui/IconFont.h"
#include "gui/UiLayer.h"
#include "gui/utils.h"
#include "gui/widgets/IconButton.h"
#include "gui/widgets/Toggle.h"
#include "gui/widgets/Tooltip.h"

#include <imgui.h>

#include <array>
#include <cfloat>
#include <cmath>
#include <span>

namespace owl::gui::widgets {

namespace {

// Two floats this close are the same property value: the reset button hides.
constexpr float g_sameValue = 1e-6f;

auto isDifferent(const float iFirst, const float iSecond) -> bool { return std::abs(iFirst - iSecond) > g_sameValue; }

auto axisColor(const size_t iAxis) -> math::vec4 {
	const auto& theme = UiLayer::getTheme();
	switch (iAxis) {
		case 0:
			return theme.axisX;
		case 1:
			return theme.axisY;
		case 2:
			return theme.axisZ;
		default:
			return theme.textDisabled;
	}
}

}// namespace

auto beginProperties(const char* iId) -> bool {
	constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
	if (!ImGui::BeginTable(iId, 3, flags))
		return false;
	ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.38f);
	ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.62f);
	ImGui::TableSetupColumn("reset", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
	return true;
}

void endProperties() { ImGui::EndTable(); }

void propertyLabel(const char* iLabel, const char* iTooltip) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::PushStyleColor(ImGuiCol_Text, vec(UiLayer::getTheme().textMuted));
	ImGui::TextUnformatted(iLabel);
	ImGui::PopStyleColor();
	if (iTooltip != nullptr)
		itemTooltip(iLabel, nullptr, iTooltip);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::PushID(iLabel);
}

auto propertyReset(const bool iModified) -> bool {
	ImGui::TableSetColumnIndex(2);
	bool clicked = false;
	if (iModified)
		clicked = iconButton("##reset", {.glyph = icon::rotateCcw, .tooltip = "Reset to default"});
	ImGui::PopID();
	return clicked;
}

auto vectorField(const char* iId, const std::span<float> ioValues, const float iSpeed, const char* iFormat,
				 const float iWidth) -> bool {
	if (ioValues.empty())
		return false;
	constexpr float spacing = 3.f;
	constexpr float markWidth = 3.f;
	const auto count = static_cast<float>(ioValues.size());
	const float total = iWidth > 0.f ? iWidth : ImGui::GetContentRegionAvail().x;
	const float width = (total - spacing * (count - 1.f)) / count;
	const float rounding = UiLayer::getTheme().frameRounding;
	bool changed = false;
	ImGui::PushID(iId);
	for (size_t axis = 0; axis < ioValues.size(); ++axis) {
		if (axis > 0)
			ImGui::SameLine(0.f, spacing);
		ImGui::PushID(static_cast<int>(axis));
		ImGui::SetNextItemWidth(width);
		changed |= ImGui::DragFloat("##axis", &ioValues[axis], iSpeed, 0.f, 0.f, iFormat);
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddRectFilled(min, {min.x + markWidth, max.y},
												  ImGui::GetColorU32(vec(axisColor(axis))), rounding,
												  ImDrawFlags_RoundCornersLeft);
		ImGui::PopID();
	}
	ImGui::PopID();
	return changed;
}

auto propertyFloat(const char* iLabel, float& ioValue, const float iDefault, const float iSpeed, const char* iFormat,
				   const char* iTooltip) -> bool {
	propertyLabel(iLabel, iTooltip);
	bool changed = ImGui::DragFloat("##value", &ioValue, iSpeed, 0.f, 0.f, iFormat);
	if (propertyReset(isDifferent(ioValue, iDefault))) {
		ioValue = iDefault;
		changed = true;
	}
	return changed;
}

auto propertyVec3(const char* iLabel, math::vec3& ioValue, const math::vec3& iDefault, const float iSpeed,
				  const char* iTooltip) -> bool {
	propertyLabel(iLabel, iTooltip);
	std::array values{ioValue.x(), ioValue.y(), ioValue.z()};
	bool changed = vectorField("##value", values, iSpeed);
	if (changed)
		ioValue = {values[0], values[1], values[2]};
	const bool modified = isDifferent(ioValue.x(), iDefault.x()) || isDifferent(ioValue.y(), iDefault.y()) ||
						  isDifferent(ioValue.z(), iDefault.z());
	if (propertyReset(modified)) {
		ioValue = iDefault;
		changed = true;
	}
	return changed;
}

auto propertyBool(const char* iLabel, bool& ioValue, const bool iDefault, const char* iTooltip) -> bool {
	propertyLabel(iLabel, iTooltip);
	bool changed = toggle("##value", &ioValue);
	if (propertyReset(ioValue != iDefault)) {
		ioValue = iDefault;
		changed = true;
	}
	return changed;
}

}// namespace owl::gui::widgets
