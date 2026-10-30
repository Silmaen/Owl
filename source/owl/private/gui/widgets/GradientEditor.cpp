/**
 * @file GradientEditor.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/GradientEditor.h"

#include "gui/utils.h"

#include <imgui.h>

#include <algorithm>
#include <cstddef>

namespace owl::gui::widgets {

namespace {

auto colorOf(const math::vec4& iColor) -> ImU32 { return ImGui::ColorConvertFloat4ToU32(vec(iColor)); }

void drawBar(ImDrawList& ioDraw, const ImVec2& iMin, const ImVec2& iMax, const renderer::ColorGradient& iGradient) {
	constexpr int steps = 32;
	const float width = iMax.x - iMin.x;
	for (int step = 0; step < steps; ++step) {
		const float from = static_cast<float>(step) / steps;
		const float to = static_cast<float>(step + 1) / steps;
		const ImU32 left = colorOf(iGradient.evaluate(from));
		const ImU32 right = colorOf(iGradient.evaluate(to));
		ioDraw.AddRectFilledMultiColor({iMin.x + width * from, iMin.y}, {iMin.x + width * to, iMax.y}, left, right,
									   right, left);
	}
	ioDraw.AddRect(iMin, iMax, ImGui::GetColorU32(ImGuiCol_Border));
}

}// namespace

auto gradientEditor(const char* iLabel, renderer::ColorGradient& ioGradient) -> bool {
	bool changed = false;
	ImGui::PushID(iLabel);
	auto* storage = ImGui::GetStateStorage();
	const ImGuiID selectedId = ImGui::GetID("##selected");
	int selected = storage->GetInt(selectedId, -1);
	const float width = std::max(ImGui::GetContentRegionAvail().x, 40.f);
	const float barHeight = ImGui::GetFrameHeight();
	const float markerSize = barHeight * 0.4f;
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 barMax{origin.x + width, origin.y + barHeight};
	auto& draw = *ImGui::GetWindowDrawList();
	drawBar(draw, origin, barMax, ioGradient);
	ImGui::InvisibleButton("##bar", {width, barHeight + markerSize * 2.f});
	const bool barHovered = ImGui::IsItemHovered();
	const float mouseTime = std::clamp((ImGui::GetIO().MousePos.x - origin.x) / width, 0.f, 1.f);
	if (barHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		selected = static_cast<int>(ioGradient.addKey({.time = mouseTime, .color = ioGradient.evaluate(mouseTime)}));
		changed = true;
	}
	for (size_t index = 0; index < ioGradient.keys.size(); ++index) {
		const auto& key = ioGradient.keys[index];
		const float x = origin.x + width * key.time;
		const ImVec2 top{x, barMax.y};
		const ImVec2 left{x - markerSize, barMax.y + markerSize * 2.f};
		const ImVec2 right{x + markerSize, barMax.y + markerSize * 2.f};
		draw.AddTriangleFilled(top, right, left, colorOf(key.color));
		draw.AddTriangle(top, right, left,
						 static_cast<int>(index) == selected ? ImGui::GetColorU32(ImGuiCol_Text)
															 : ImGui::GetColorU32(ImGuiCol_Border));
		const bool near = std::abs(ImGui::GetIO().MousePos.x - x) <= markerSize &&
						  ImGui::GetIO().MousePos.y >= barMax.y - 1.f && barHovered;
		if (near && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			selected = static_cast<int>(index);
		if (near && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ioGradient.keys.erase(ioGradient.keys.begin() + static_cast<std::ptrdiff_t>(index));
			selected = -1;
			changed = true;
			break;
		}
	}
	if (selected >= static_cast<int>(ioGradient.keys.size()))
		selected = -1;
	if (selected >= 0 && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		auto key = ioGradient.keys[static_cast<size_t>(selected)];
		key.time = mouseTime;
		ioGradient.keys.erase(ioGradient.keys.begin() + selected);
		selected = static_cast<int>(ioGradient.addKey(key));
		changed = true;
	}
	if (selected >= 0) {
		auto& key = ioGradient.keys[static_cast<size_t>(selected)];
		ImGui::SetNextItemWidth(width * 0.3f);
		if (ImGui::DragFloat("##time", &key.time, 0.005f, 0.f, 1.f, "%.3f")) {
			const auto moved = key;
			ioGradient.keys.erase(ioGradient.keys.begin() + selected);
			selected = static_cast<int>(ioGradient.addKey(moved));
			changed = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (selected >= 0 && ImGui::ColorEdit4("##color", ioGradient.keys[static_cast<size_t>(selected)].color.data()))
			changed = true;
	} else {
		ImGui::TextDisabled(ioGradient.keys.empty() ? "White all along: double-click the bar to add a stop."
													: "Click a stop to edit it, right-click to remove it.");
	}
	storage->SetInt(selectedId, selected);
	ImGui::PopID();
	return changed;
}

}// namespace owl::gui::widgets
