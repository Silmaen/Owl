/**
 * @file ProjectSettings.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ProjectSettings.h"

#include <gui/IconBank.h>
#include <imgui_stdlib.h>
#include <renderer/RenderLayerFactory.h>

namespace owl::nest::panel {

namespace {
constexpr auto g_popupName = "Project Settings";

/**
 * @brief
 *  Render the renderer-stack editor: ordered list of layers with
 * Type / Name editing and reorder/remove buttons. Mutates `ioStack` in place.
 */
auto drawRendererStackEditor(renderer::RendererStackConfig& ioStack) -> void {
	const auto availableTypes = renderer::RenderLayerFactory::registeredTypes();
	auto& entries = ioStack.entries;

	int moveUp = -1;
	int moveDown = -1;
	int removeAt = -1;

	for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
		auto& e = entries[static_cast<size_t>(i)];
		ImGui::PushID(i);
		ImGui::SetNextItemWidth(140.f);
		const char* typePreview = e.typeKey.empty() ? "(none)" : e.typeKey.c_str();
		if (ImGui::BeginCombo("##type", typePreview)) {
			for (const auto& t: availableTypes) {
				const bool selected = (e.typeKey == t);
				if (ImGui::Selectable(t.c_str(), selected))
					e.typeKey = t;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.f);
		ImGui::InputText("##name", &e.name);
		ImGui::SameLine();
		ImGui::BeginDisabled(i == 0);
		if (ImGui::ArrowButton("##up", ImGuiDir_Up))
			moveUp = i;
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(i + 1 == static_cast<int>(entries.size()));
		if (ImGui::ArrowButton("##down", ImGuiDir_Down))
			moveDown = i;
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
			removeAt = i;
		ImGui::PopID();
	}

	if (moveUp > 0) {
		const auto idx = static_cast<size_t>(moveUp);
		std::swap(entries[idx], entries[idx - 1]);
	}
	if (moveDown >= 0 && moveDown + 1 < static_cast<int>(entries.size())) {
		const auto idx = static_cast<size_t>(moveDown);
		std::swap(entries[idx], entries[idx + 1]);
	}
	if (removeAt >= 0)
		entries.erase(entries.begin() + removeAt);

	if (ImGui::SmallButton("+ Add Layer")) {
		renderer::RendererStackEntry entry;
		entry.typeKey = availableTypes.empty() ? std::string{"Renderer2D"} : availableTypes.front();
		// Generate a default unique name layer_N.
		int idx = 0;
		std::string candidate;
		do { candidate = std::format("layer_{}", idx++); } while (ioStack.find(candidate) != nullptr);
		entry.name = candidate;
		entries.push_back(std::move(entry));
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(empty → fallback to single Renderer2D)");
}
}// namespace

void ProjectSettings::open(const Project& iProject) {
	m_localProject = iProject;
	m_nameBuffer = iProject.name;
	m_versionBuffer = iProject.version;
	m_authorBuffer = iProject.author;
	m_descriptionBuffer = iProject.description;
	m_iconBuffer = iProject.icon;
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
	ImGui::SetNextWindowSize({450, 0}, ImGuiCond_FirstUseEver);
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
		ImGui::Text("Metadata");
		ImGui::InputText("Version", &m_versionBuffer);
		ImGui::InputText("Author", &m_authorBuffer);
		ImGui::InputText("Description", &m_descriptionBuffer);
		ImGui::InputText("Icon", &m_iconBuffer);

		ImGui::Separator();
		ImGui::Text("Renderer Stack");
		drawRendererStackEditor(m_localProject.rendererStack);

		ImGui::Separator();
		ImGui::Text("Window");
		auto widthI = static_cast<int>(m_localProject.window.width);
		auto heightI = static_cast<int>(m_localProject.window.height);
		if (ImGui::DragInt("Width", &widthI, 1.0f, 320, 7680))
			m_localProject.window.width = static_cast<uint32_t>(widthI);
		if (ImGui::DragInt("Height", &heightI, 1.0f, 240, 4320))
			m_localProject.window.height = static_cast<uint32_t>(heightI);
		ImGui::Checkbox("Fullscreen", &m_localProject.window.fullscreen);
		ImGui::Checkbox("Resizable", &m_localProject.window.resizable);

		ImGui::Separator();

		const auto& iconBank = gui::IconBank::instance();
		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() +
								  ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (iconBank.iconButton("save", "OK##projSettings", {buttonWidth, 0})) {
			m_localProject.name = m_nameBuffer;
			m_localProject.firstScene =
					m_selectedSceneIndex >= 0 ? m_availableScenes[static_cast<size_t>(m_selectedSceneIndex)] : "";
			m_localProject.version = m_versionBuffer;
			m_localProject.author = m_authorBuffer;
			m_localProject.description = m_descriptionBuffer;
			m_localProject.icon = m_iconBuffer;
			m_hasResult = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (iconBank.iconButton("close", "Cancel", {buttonWidth, 0})) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

}// namespace owl::nest::panel
