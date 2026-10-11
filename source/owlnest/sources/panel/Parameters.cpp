/**
 * @file Parameters.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Parameters.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <magic_enum/magic_enum.hpp>

#include <cstdint>
#include <string>
#include <tuple>

namespace owl::nest::panel {

namespace {
constexpr auto g_popupName = "Parameters";
constexpr auto g_restartPopupName = "Restart Required";

template<typename Type>
void renderBackendCombo(const char* iId, Type& ioValue, const Type iHidden) {
	if (!gui::widgets::beginCombo(iId, std::string(magic_enum::enum_name(ioValue)).c_str()))
		return;
	for (const auto& type: magic_enum::enum_values<Type>()) {
		if (type == iHidden)
			continue;
		const bool isSelected = ioValue == type;
		if (ImGui::Selectable(std::string(magic_enum::enum_name(type)).c_str(), isSelected))
			ioValue = type;
		if (isSelected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
}

}// namespace

void Parameters::open() {
	const auto& params = app::Application::get().getInitParams();
	m_localParams = params;
	m_originalRenderer = params.renderer;
	m_originalSound = params.sound;
	m_showRestartWarning = false;
	m_pendingOpen = true;
}

auto Parameters::apply() -> bool {
	auto& params = app::Application::get().getInitParams();
	params.width = m_localParams.width;
	params.height = m_localParams.height;
	params.renderer = m_localParams.renderer;
	params.sound = m_localParams.sound;
	params.useDebugging = m_localParams.useDebugging;
	params.frameLogFrequency = m_localParams.frameLogFrequency;
	app::Application::get().setHotReloadEnabled(m_localParams.hotReload);

	const auto configPath = app::Application::get().getWorkingDirectory() / "config.yml";
	params.saveToFile(configPath);
	core::Log::setFrameFrequency(params.frameLogFrequency);

	return m_localParams.renderer != m_originalRenderer || m_localParams.sound != m_originalSound;
}

void Parameters::onImGuiRender() {
	if (m_pendingOpen) {
		ImGui::OpenPopup(g_popupName);
		m_pendingOpen = false;
	}

	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		// The grids stretch: this sets the width of the dialog.
		ImGui::Dummy({ImGui::GetFontSize() * 28.f, 0.f});
		const auto params = m_localParams;
		if (gui::widgets::sectionHeader("Window", gui::icon::appWindow, false).open) {
			if (gui::widgets::beginProperties("##window")) {
				auto width = static_cast<int>(m_localParams.width);
				auto height = static_cast<int>(m_localParams.height);
				if (gui::widgets::propertyInt("Width", width, static_cast<int>(params.width), 1, 16384, "%d px"))
					m_localParams.width = static_cast<uint32_t>(width);
				if (gui::widgets::propertyInt("Height", height, static_cast<int>(params.height), 1, 16384, "%d px"))
					m_localParams.height = static_cast<uint32_t>(height);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Backends", gui::icon::cuboid, false).open) {
			if (gui::widgets::beginProperties("##backends")) {
				gui::widgets::propertyLabel("Renderer", "Graphics backend; takes effect after a restart.");
				renderBackendCombo("##Renderer", m_localParams.renderer, renderer::gpu::RenderAPI::Type::Null);
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::propertyLabel("Sound", "Audio backend; takes effect after a restart.");
				renderBackendCombo("##Sound", m_localParams.sound, sound::SoundAPI::Type::Null);
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		if (gui::widgets::sectionHeader("Debug", gui::icon::terminal, false).open) {
			if (gui::widgets::beginProperties("##debug")) {
				std::ignore = gui::widgets::propertyBool("Use Debugging", m_localParams.useDebugging,
														 params.useDebugging, "Enable the graphics debug layers.");
				std::ignore = gui::widgets::propertyBool(
						"Hot Reload", m_localParams.hotReload, params.hotReload,
						"Reload textures, shaders, scenes, tilesets and scripts when their file changes.");
				auto freq = static_cast<int>(m_localParams.frameLogFrequency);
				if (gui::widgets::propertyInt("Frame Log Frequency", freq, static_cast<int>(params.frameLogFrequency),
											  0, 100000, "%d", "Frames between two frame-sampled log lines."))
					m_localParams.frameLogFrequency = static_cast<uint64_t>(freq);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}

		ImGui::Separator();

		// --- OK / Apply / Cancel (centred) ---
		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetFontSize() +

								  ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;

		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (gui::widgets::button("OK##params",
								 {.glyph = gui::icon::check, .label = "OK", .primary = true, .width = buttonWidth})) {
			const bool needRestart = apply();
			m_showRestartWarning = needRestart;
			m_pendingClose = true;
		}

		ImGui::SameLine();
		if (gui::widgets::button("Apply", {.glyph = gui::icon::check, .label = "Apply", .width = buttonWidth})) {
			if (apply())
				m_showRestartWarning = true;
		}

		ImGui::SameLine();
		if (gui::widgets::button("Cancel", {.glyph = gui::icon::x, .label = "Cancel", .width = buttonWidth})) {
			ImGui::CloseCurrentPopup();
		}

		// --- Restart warning sub-popup ---
		if (m_showRestartWarning)

			ImGui::OpenPopup(g_restartPopupName);
		if (ImGui::BeginPopupModal(g_restartPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Renderer or Sound backend has changed.\nPlease restart the application for the changes to "
						"take effect.");

			ImGui::Separator();
			if (gui::widgets::button("OK##restart", {.glyph = gui::icon::x, .label = "OK", .primary = true})) {
				m_showRestartWarning = false;

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (m_pendingClose && !m_showRestartWarning) {
			m_pendingClose = false;

			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

}// namespace owl::nest::panel
