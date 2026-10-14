/**
 * @file ProjectSettings.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ProjectSettings.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <imgui_stdlib.h>
#include <renderer/RenderLayerFactory.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <tuple>
#include <utility>

namespace owl::nest::panel {

namespace {
constexpr auto g_popupName = "Project Settings";

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
		if (gui::widgets::beginCombo("##type", typePreview)) {
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
		const float side = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - (side + spacing) * 3.f));
		ImGui::InputTextWithHint("##name", "Layer name", &e.name);
		ImGui::SameLine();
		if (gui::widgets::iconButton("##up", {.glyph = gui::icon::chevronUp, .tooltip = "Move up", .disabled = i == 0}))
			moveUp = i;
		ImGui::SameLine();
		if (gui::widgets::iconButton("##down", {.glyph = gui::icon::chevronDown,
												.tooltip = "Move down",
												.disabled = i + 1 == static_cast<int>(entries.size())}))
			moveDown = i;
		ImGui::SameLine();
		if (gui::widgets::iconButton("##remove", {.glyph = gui::icon::trash2, .tooltip = "Remove the layer"}))
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

	if (gui::widgets::button("##addLayer", {.glyph = gui::icon::plus, .label = "Add Layer"})) {
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
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("An empty stack draws with a single Renderer2D.");
}

void drawSimulationSettings(scene::SimulationSettings& ioSimulation) {
	const scene::SimulationSettings defaults;
	std::ignore = gui::widgets::propertyFloat("Tick Rate", ioSimulation.tickRate, defaults.tickRate, 1.f, "%.0f Hz",
											  "Fixed simulation ticks per second, whatever the frame rate: rate of "
											  "the Lua on_tick and of the physics steps.");
	auto maxTicks = static_cast<int>(ioSimulation.maxTicksPerFrame);
	if (gui::widgets::propertyInt("Max Ticks per Frame", maxTicks, static_cast<int>(defaults.maxTicksPerFrame), 1,
								  static_cast<int>(scene::SimulationSettings::maxTicksLimit), "%d",
								  "Time beyond this many ticks in one frame is dropped: the game slows down instead "
								  "of stalling."))
		ioSimulation.maxTicksPerFrame = static_cast<uint32_t>(maxTicks);
	ioSimulation = ioSimulation.clamped();
}

}// namespace

void ProjectSettings::open(const Project& iProject) {
	m_localProject = iProject;
	m_nameBuffer = iProject.name;
	m_versionBuffer = iProject.version;
	m_authorBuffer = iProject.author;
	m_descriptionBuffer = iProject.description;
	m_iconBuffer = iProject.icon;
	m_gamepadMappingsBuffer = iProject.gamepadMappings;
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
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		// The grids stretch: this sets the width of the dialog.
		ImGui::Dummy({ImGui::GetFontSize() * 32.f, 0.f});
		ImGui::TextDisabled("%s", m_localProject.projectDirectory.string().c_str());
		if (gui::widgets::sectionHeader("General", gui::icon::package, false).open) {
			if (gui::widgets::beginProperties("##general")) {
				std::ignore = gui::widgets::propertyText("Name", m_nameBuffer, "Project name");
				gui::widgets::propertyLabel("First Scene", "Scene the game opens on.");
				const char* preview = m_selectedSceneIndex >= 0
											  ? m_availableScenes[static_cast<size_t>(m_selectedSceneIndex)].c_str()
											  : "(none)";
				if (gui::widgets::beginCombo("##firstScene", preview)) {
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
				std::ignore = gui::widgets::propertyReset(false);
				std::ignore = gui::widgets::propertyText("Version", m_versionBuffer, "1.0.0");
				std::ignore = gui::widgets::propertyText("Author", m_authorBuffer);
				std::ignore = gui::widgets::propertyText("Description", m_descriptionBuffer);
				std::ignore = gui::widgets::propertyText("Icon", m_iconBuffer, "icon.png",
														 "Image of the game window, relative to the project.");
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Window", gui::icon::appWindow, false).open) {
			if (gui::widgets::beginProperties("##window")) {
				const Project::WindowSettings defaults;
				auto widthI = static_cast<int>(m_localProject.window.width);
				auto heightI = static_cast<int>(m_localProject.window.height);
				if (gui::widgets::propertyInt("Width", widthI, static_cast<int>(defaults.width), 320, 7680, "%d px"))
					m_localProject.window.width = static_cast<uint32_t>(widthI);
				if (gui::widgets::propertyInt("Height", heightI, static_cast<int>(defaults.height), 240, 4320, "%d px"))
					m_localProject.window.height = static_cast<uint32_t>(heightI);
				std::ignore =
						gui::widgets::propertyBool("Fullscreen", m_localProject.window.fullscreen, defaults.fullscreen);
				std::ignore =
						gui::widgets::propertyBool("Resizable", m_localProject.window.resizable, defaults.resizable);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Input", gui::icon::gamepad2, false).open) {
			if (gui::widgets::beginProperties("##input")) {
				std::ignore = gui::widgets::propertyText(
						"Gamepad Mappings", m_gamepadMappingsBuffer, "gamecontrollerdb.txt",
						"Optional SDL_GameControllerDB file, relative to the project: extra or corrected gamepad "
						"mappings, packed with the game.");
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Simulation", gui::icon::timer, false).open) {
			if (gui::widgets::beginProperties("##simulation")) {
				drawSimulationSettings(m_localProject.simulation);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Renderer Stack", gui::icon::layers, false).open) {
			drawRendererStackEditor(m_localProject.rendererStack);
			gui::widgets::endSection();
		}
		ImGui::Spacing();

		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() +
								  ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (gui::widgets::button("OK##projSettings",
								 {.glyph = gui::icon::check, .label = "OK", .primary = true, .width = buttonWidth})) {
			m_localProject.name = m_nameBuffer;
			m_localProject.firstScene =
					m_selectedSceneIndex >= 0 ? m_availableScenes[static_cast<size_t>(m_selectedSceneIndex)] : "";
			m_localProject.version = m_versionBuffer;
			m_localProject.author = m_authorBuffer;
			m_localProject.description = m_descriptionBuffer;
			m_localProject.icon = m_iconBuffer;
			m_localProject.gamepadMappings = m_gamepadMappingsBuffer;
			m_hasResult = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (gui::widgets::button("Cancel", {.glyph = gui::icon::x, .label = "Cancel", .width = buttonWidth})) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

}// namespace owl::nest::panel
