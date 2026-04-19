/**
 * @file AsyncProgressModal.cpp
 * @author Silmaen
 * @date 16/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/AsyncProgressModal.h"

#include <gui/IconBank.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
OWL_DIAG_POP

namespace owl::nest::panel {

void AsyncProgressModal::open(const std::string& iTitle, shared<AsyncProgressState> iState, const bool iCancellable) {
	m_title = iTitle;
	m_state = std::move(iState);
	m_cancellable = iCancellable;
	m_pendingOpen = true;
	m_active = true;
}

void AsyncProgressModal::onImGuiRender() {
	if (!m_active || !m_state)
		return;

	if (m_pendingOpen) {
		ImGui::OpenPopup(m_title.c_str());
		m_pendingOpen = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Always);

	if (ImGui::BeginPopupModal(m_title.c_str(), nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		const float progress = m_state->progress.load();
		const auto message = m_state->getMessage();

		const auto& iconBank = gui::IconBank::instance();
		if (m_state->hasError.load()) {
			// Error state: show error in red + Close button.
			const auto errorMsg = m_state->getError();
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error:");
			ImGui::TextWrapped("%s", errorMsg.c_str());
			ImGui::Spacing();
			if (iconBank.iconButton("close", "Close", {ImGui::GetFontSize() * 7.f, 0})) {
				ImGui::CloseCurrentPopup();
				close();
			}
		} else if (m_state->completed.load()) {
			// Completed: show success + summary message + Close button.
			ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "Done!");
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Operation completed successfully.");
			const auto summary = m_state->getMessage();
			if (!summary.empty() && summary != "Done!") {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextWrapped("%s", summary.c_str());
			}
			ImGui::Spacing();
			if (iconBank.iconButton("close", "Close", {ImGui::GetFontSize() * 7.f, 0})) {
				ImGui::CloseCurrentPopup();
				close();
			}
		} else {
			// In progress: show progress bar + status text + optional cancel.
			ImGui::ProgressBar(progress, ImVec2(-1, 0));
			ImGui::TextWrapped("%s", message.c_str());
			ImGui::Spacing();
			if (m_cancellable) {
				if (m_state->cancelRequested.load()) {
					ImGui::BeginDisabled();
					iconBank.iconButton("close", "Cancelling...", {ImGui::GetFontSize() * 8.f, 0});
					ImGui::EndDisabled();
				} else {
					if (iconBank.iconButton("close", "Cancel", {ImGui::GetFontSize() * 7.f, 0}))
						m_state->cancelRequested.store(true);
				}
			}
		}
		ImGui::EndPopup();
	}
}

auto AsyncProgressModal::isFinished() const -> bool {
	if (!m_state)
		return true;
	return m_state->completed.load() || m_state->hasError.load();
}

void AsyncProgressModal::close() {
	m_active = false;
	m_state.reset();
	m_title.clear();
}

}// namespace owl::nest::panel
