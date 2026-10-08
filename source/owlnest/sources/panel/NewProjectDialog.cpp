/**
 * @file NewProjectDialog.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "NewProjectDialog.h"

#include <gui/IconBank.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <platform/FileDialog.h>

namespace owl::nest::panel {

namespace {
constexpr const char* g_popupName = "New Project";
}// namespace

void NewProjectDialog::open(const std::vector<ProjectTemplate>& iTemplates,
							const std::filesystem::path& iParentDirectory) {
	m_templates = iTemplates;
	m_name = "MyGame";
	m_parentDirectory = iParentDirectory.string();
	m_selected = 0;
	m_open = true;
	m_openRequested = true;
}

void NewProjectDialog::setFields(const std::string& iName, const std::string& iParentDirectory,
								 const size_t iTemplateIndex) {
	m_name = iName;
	m_parentDirectory = iParentDirectory;
	m_selected = iTemplateIndex;
}

auto NewProjectDialog::makeRequest() const -> std::optional<NewProjectRequest> {
	if (m_name.empty() || m_parentDirectory.empty())
		return std::nullopt;
	NewProjectRequest request{.projectTemplate = std::nullopt,
							  .directory = std::filesystem::path{m_parentDirectory} / m_name,
							  .name = m_name};
	if (m_selected < m_templates.size())
		request.projectTemplate = m_templates[m_selected];
	return request;
}

auto NewProjectDialog::onImGuiRender() -> std::optional<NewProjectRequest> {
	if (!m_open)
		return std::nullopt;
	if (m_openRequested) {
		ImGui::OpenPopup(g_popupName);
		m_openRequested = false;
	}
	std::optional<NewProjectRequest> request;
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputText("Name", &m_name);
		ImGui::InputText("Location", &m_parentDirectory);
		ImGui::SameLine();
		if (ImGui::SmallButton("...")) {
			if (const auto picked = platform::FileDialog::pickFolder(); !picked.empty())
				m_parentDirectory = picked.string();
		}
		ImGui::SeparatorText("Template");
		if (m_templates.empty())
			ImGui::TextDisabled("No template installed: the project starts empty.");
		for (size_t i = 0; i < m_templates.size(); ++i) {
			const auto& tpl = m_templates[i];
			if (ImGui::RadioButton(tpl.name.c_str(), m_selected == i))
				m_selected = i;
			ImGui::SameLine();
			ImGui::TextDisabled("%s", tpl.description.c_str());
		}
		ImGui::Spacing();
		const auto target = std::filesystem::path{m_parentDirectory} / m_name;
		ImGui::TextDisabled("Creates %s", target.string().c_str());
		ImGui::Spacing();
		const auto& iconBank = gui::IconBank::instance();
		ImGui::BeginDisabled(m_name.empty() || m_parentDirectory.empty());
		if (iconBank.iconButton("project", "Create", {ImGui::GetFontSize() * 7.f, 0})) {
			request = makeRequest();
			m_open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel", {ImGui::GetFontSize() * 7.f, 0})) {
			m_open = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	return request;
}

}// namespace owl::nest::panel
