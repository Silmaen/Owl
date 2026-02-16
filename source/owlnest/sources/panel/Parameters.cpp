/**
 * @file Parameters.cpp
 * @author Silmaen
 * @date 16/02/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "Parameters.h"

#include <magic_enum/magic_enum.hpp>

namespace owl::nest::panel {

namespace {
constexpr auto g_popupName = "Parameters";
constexpr auto g_restartPopupName = "Restart Required";
}// namespace

void Parameters::open() {
	const auto& params = core::Application::get().getInitParams();
	m_localParams = params;
	m_originalRenderer = params.renderer;
	m_originalSound = params.sound;
	m_showRestartWarning = false;
	m_pendingOpen = true;
}

auto Parameters::apply() -> bool {
	auto& params = core::Application::get().getInitParams();
	params.width = m_localParams.width;
	params.height = m_localParams.height;
	params.renderer = m_localParams.renderer;
	params.sound = m_localParams.sound;
	params.useDebugging = m_localParams.useDebugging;
	params.frameLogFrequency = m_localParams.frameLogFrequency;

	const auto configPath = core::Application::get().getWorkingDirectory() / "config.yml";
	params.saveToFile(configPath);
	core::Log::setFrameFrequency(params.frameLogFrequency);

	return m_localParams.renderer != m_originalRenderer || m_localParams.sound != m_originalSound;
}

void Parameters::onImGuiRender() {
	if (m_pendingOpen) {
		ImGui::OpenPopup(g_popupName);
		m_pendingOpen = false;
	}
	ImGui::SetNextWindowSize({400, 0}, ImGuiCond_FirstUseEver);
	if (ImGui::BeginPopupModal(g_popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		// --- Window Section ---
		if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto width = static_cast<int>(m_localParams.width);
			auto height = static_cast<int>(m_localParams.height);
			if (ImGui::InputInt("Width", &width)) {
				if (width > 0)
					m_localParams.width = static_cast<uint32_t>(width);
			}
			if (ImGui::InputInt("Height", &height)) {
				if (height > 0)
					m_localParams.height = static_cast<uint32_t>(height);
			}
		}

		// --- Renderer Section ---
		if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginCombo("##Renderer", std::string(magic_enum::enum_name(m_localParams.renderer)).c_str())) {
				for (const auto& type: magic_enum::enum_values<renderer::RenderAPI::Type>()) {
					if (type == renderer::RenderAPI::Type::Null)
						continue;
					const bool isSelected = m_localParams.renderer == type;
					if (ImGui::Selectable(std::string(magic_enum::enum_name(type)).c_str(), isSelected))
						m_localParams.renderer = type;
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		// --- Sound Section ---
		if (ImGui::CollapsingHeader("Sound", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::BeginCombo("##Sound", std::string(magic_enum::enum_name(m_localParams.sound)).c_str())) {
				for (const auto& type: magic_enum::enum_values<sound::SoundAPI::Type>()) {
					if (type == sound::SoundAPI::Type::Null)
						continue;
					const bool isSelected = m_localParams.sound == type;
					if (ImGui::Selectable(std::string(magic_enum::enum_name(type)).c_str(), isSelected))
						m_localParams.sound = type;
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		// --- Debug Section ---
		if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Use Debugging", &m_localParams.useDebugging);
			auto freq = static_cast<int>(m_localParams.frameLogFrequency);
			if (ImGui::InputInt("Frame Log Frequency", &freq)) {
				if (freq >= 0)
					m_localParams.frameLogFrequency = static_cast<uint64_t>(freq);
			}
		}

		ImGui::Separator();

		// --- OK / Apply / Cancel (centered) ---
		const float buttonWidth = ImGui::CalcTextSize("Cancel").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float totalWidth = buttonWidth * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowSize().x - totalWidth) * 0.5f);
		if (ImGui::Button("OK##params", {buttonWidth, 0})) {
			const bool needRestart = apply();
			m_showRestartWarning = needRestart;
			m_pendingClose = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Apply", {buttonWidth, 0})) {
			if (apply())
				m_showRestartWarning = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", {buttonWidth, 0})) {
			ImGui::CloseCurrentPopup();
		}

		// --- Restart warning sub-popup ---
		if (m_showRestartWarning)
			ImGui::OpenPopup(g_restartPopupName);
		if (ImGui::BeginPopupModal(g_restartPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Renderer or Sound backend has changed.\nPlease restart the application for the changes to take effect.");
			ImGui::Separator();
			if (ImGui::Button("OK##restart")) {
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
