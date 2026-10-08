/**
 * @file RecoveryPrompt.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "RecoveryPrompt.h"

#include <gui/IconBank.h>
#include <imgui.h>

#include <string>

namespace owl::nest::panel {

namespace {
constexpr const char* g_popupName = "Recover Unsaved Work?";
}// namespace

void RecoveryPrompt::open(const std::vector<RecoveryEntry>& iEntries) {
	m_entries = iEntries;
	m_open = !m_entries.empty();
	m_openRequested = m_open;
}

auto RecoveryPrompt::onImGuiRender() -> Choice {
	if (!m_open)
		return Choice::None;
	if (m_openRequested) {
		ImGui::OpenPopup(g_popupName);
		m_openRequested = false;
	}
	Choice choice = Choice::None;
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("The last session ended with unsaved changes, autosaved for these documents:");
		ImGui::Spacing();
		for (const auto& entry: m_entries) {
			const auto path = entry.originalPath.empty() ? std::string{"untitled"} : entry.originalPath.string();
			ImGui::BulletText("%s  (%s, %s)", entry.title.c_str(), path.c_str(), entry.savedAt.c_str());
		}
		ImGui::Spacing();
		const auto& iconBank = gui::IconBank::instance();
		if (iconBank.iconButton("open", "Recover", {ImGui::GetFontSize() * 7.f, 0}))
			choice = Choice::Recover;
		ImGui::SameLine();
		if (iconBank.iconButton("delete", "Discard", {ImGui::GetFontSize() * 7.f, 0}))
			choice = Choice::Discard;
		if (choice != Choice::None) {
			m_open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	return choice;
}

}// namespace owl::nest::panel
