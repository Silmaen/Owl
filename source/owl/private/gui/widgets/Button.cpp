/**
 * @file Button.cpp
 * @author Silmaen
 * @date 10/26/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/IconBank.h"
#include "gui/utils.h"
#include "gui/widgets/Button.h"
#include "renderer/Renderer.h"
#include "renderer/Texture.h"


namespace owl::gui::widgets {

Button::~Button() = default;

void Button::onRenderBase() const {
	const bool selectedRot = m_data.isSelected();
	bool action = false;
	if (selectedRot) {
		ImGui::PushStyleColor(ImGuiCol_Button, vec(math::vec4{0.23f, 0.23f, 0.23f, 1.f}));
	}

	// Try IconBank first, then fall back to texture library
	bool rendered = false;
	if (!m_data.icon.empty()) {
		if (const auto iconInfo = IconBank::instance().getIcon(m_data.icon); iconInfo.has_value()) {
			action = ImGui::ImageButton(m_data.id.c_str(), static_cast<ImTextureID>(iconInfo->textureId),
										vec(m_data.size), vec(iconInfo->uv0), vec(iconInfo->uv1));
			rendered = true;
		} else {
			auto& textureLibrary = renderer::Renderer::getTextureLibrary();
			if (const auto tex = imTexture(textureLibrary.get(m_data.icon)); tex.has_value()) {
				action = ImGui::ImageButton(m_data.id.c_str(), tex.value(), vec(m_data.size), {0, 1}, {1, 0});
				rendered = true;
			}
		}
	}
	if (!rendered) {
		action = ImGui::Button(m_data.replacementText.c_str(), vec(m_data.size));
	}
	if (action) {
		m_data.onClick();
	}
	if (!m_data.tooltip.empty() && ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", m_data.tooltip.c_str());
	if (selectedRot) {
		ImGui::PopStyleColor();
	}
}

}// namespace owl::gui::widgets
