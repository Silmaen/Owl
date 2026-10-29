/**
 * @file SettingsImportPrompt.cpp
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "SettingsImportPrompt.h"

#include <gui/IconFont.h>
#include <gui/widgets/IconButton.h>
#include <imgui.h>

namespace owl::nest::panel {

namespace {
constexpr const char* g_popupName = "Import Settings?";
}// namespace

void SettingsImportPrompt::open() {
	m_open = true;
	m_openRequested = true;
}

auto SettingsImportPrompt::onImGuiRender() -> Choice {
	if (!m_open)
		return Choice::None;
	if (m_openRequested) {
		ImGui::OpenPopup(g_popupName);
		m_openRequested = false;
	}
	Choice choice = Choice::None;
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("This installation of Owl Nest has no settings yet.");
		ImGui::TextUnformatted("Import them from the folder of a previous version?");
		ImGui::Spacing();
		const float width = ImGui::GetFontSize() * 9.f;
		if (gui::widgets::button(
					"Import",
					{.glyph = gui::icon::folderOpen, .label = "Choose folder...", .primary = true, .width = width}))
			choice = Choice::Import;
		ImGui::SameLine();
		if (gui::widgets::button("Skip", {.label = "Not now", .width = width}))
			choice = Choice::Skip;
		if (choice != Choice::None) {
			m_open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	return choice;
}

}// namespace owl::nest::panel
