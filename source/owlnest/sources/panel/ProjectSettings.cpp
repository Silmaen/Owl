/**
 * @file ProjectSettings.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ProjectSettings.h"

#include <imgui_stdlib.h>

namespace owl::nest::panel {

namespace {
constexpr auto g_popupName = "Project Settings";
}// namespace

void ProjectSettings::open(const Project& iProject) {
	m_localProject = iProject;
	m_nameBuffer = iProject.name;
	m_hasResult = false;
	scanScenes();
	m_selectedSceneIndex = -1;
	for (int i = 0; i < static_cast<int>(m_availableScenes.size()); ++i) {
		if (m_availableScenes[static_cast<size_t>(i)] == iProject.firstScene) {
			m_selectedSceneIndex = i;
			break;
		}
	}
	m_pendingOpen = true;
}

auto ProjectSettings::consumeResult() -> Project {
	m_hasResult = false;
	return m_localProject;
}

void ProjectSettings::scanScenes() {
	m_availableScenes.clear();
	if (m_localProject.projectDirectory.empty())
		return;
	const auto& projectDir = m_localProject.projectDirectory;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(projectDir)) {
		if (!entry.is_regular_file())
			continue;
		if (entry.path().extension() != ".owl")
			continue;
		m_availableScenes.push_back(std::filesystem::relative(entry.path(), projectDir).string());
	}
	std::ranges::sort(m_availableScenes);
}

void ProjectSettings::onImGuiRender() {
	if (m_pendingOpen) {
		ImGui::OpenPopup(g_popupName);
		m_pendingOpen = false;
	}
	ImGui::SetNextWindowSize({400, 0}, ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Directory: %s", m_localProject.projectDirectory.string().c_str());
		ImGui::Separator();

		ImGui::InputText("Name", &m_nameBuffer);

		// First scene dropdown
		const char* preview = m_selectedSceneIndex >= 0
									  ? m_availableScenes[static_cast<size_t>(m_selectedSceneIndex)].c_str()
									  : "(none)";
		if (ImGui::BeginCombo("First Scene", preview)) {
			if (ImGui::Selectable("(none)", m_selectedSceneIndex < 0))
				m_selectedSceneIndex = -1;
			for (int i = 0; i < static_cast<int>(m_availableScenes.size()); ++i) {
				const bool isSelected = m_selectedSceneIndex == i;
				if (ImGui::Selectable(m_availableScenes[static_cast<size_t>(i)].c_str(), isSelected))
					m_selectedSceneIndex = i;
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();

		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (ImGui::Button("OK##projSettings", {buttonWidth, 0})) {
			m_localProject.name = m_nameBuffer;
			m_localProject.firstScene =
					m_selectedSceneIndex >= 0 ? m_availableScenes[static_cast<size_t>(m_selectedSceneIndex)] : "";
			m_hasResult = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", {buttonWidth, 0})) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

}// namespace owl::nest::panel
