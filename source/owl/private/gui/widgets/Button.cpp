/**
 * @file Button.cpp
 * @author Silmaen
 * @date 10/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/widgets/Button.h"

#include "gui/utils.h"
#include "renderer/Renderer.h"
#include "renderer/Texture.h"


namespace owl::gui::widgets {

Button::~Button() = default;

void Button::onRenderBase() const {
	shared<renderer::Texture> icon = nullptr;
	if (!m_data.icon.empty()) {
		auto& textureLibrary = renderer::Renderer::getTextureLibrary();
		icon = textureLibrary.get(m_data.icon);
	}
	const bool selectedRot = m_data.isSelected();
	bool action = false;
	if (selectedRot) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.23f, 0.23f, 0.23f, 1));
	}
	if (const auto tex = imTexture(icon); tex.has_value()) {
		action = ImGui::ImageButton(m_data.id.c_str(), tex.value(), vec(m_data.size), {0, 1}, {1, 0});
	} else {
		action = ImGui::Button(m_data.replacementText.c_str(), vec(m_data.size));
	}
	if (action) {
		m_data.onClick();
	}
	if (selectedRot) {
		ImGui::PopStyleColor();
	}
}

}// namespace owl::gui::widgets
