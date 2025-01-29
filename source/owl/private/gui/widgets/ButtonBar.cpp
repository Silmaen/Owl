/**
 * @file ButtonBar.cpp
 * @author Silmaen
 * @date 10/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "gui/widgets/ButtonBar.h"

#include <imgui.h>

namespace owl::gui::widgets {


ButtonBar::~ButtonBar() = default;

void ButtonBar::addButton(ButtonData&& iButton) {
	m_buttons.emplace_back();
	m_buttons.back().init(std::move(iButton));
}

void ButtonBar::onRenderBase() const {
	ImGuiWindowFlags flags = 0;
	if (!m_data.hasDecoration)
		flags |= ImGuiWindowFlags_NoDecoration;
	if (!m_data.hasScrollBars)
		flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	ImGui::Begin(m_data.id.c_str(), nullptr, flags);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	for (const auto& button: m_buttons) {
		button.onRender();
		if (m_data.isHorizontal)
			ImGui::SameLine();
	}
	ImGui::PopStyleColor();
	ImGui::End();
}

void ButtonBar::clearButtons() { m_buttons.clear(); }

}// namespace owl::gui::widgets
