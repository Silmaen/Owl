/**
 * @file BasePanel.cpp
 * @author Silmaen
 * @date 10/16/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "gui/BasePanel.h"
#include <app/Application.h>
#include <imgui.h>

namespace owl::gui {

BasePanel::BasePanel(std::string&& iName) : m_name{std::move(iName)} {}

BasePanel::~BasePanel() = default;

void BasePanel::onRender() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin(m_name.c_str());
	m_focused = ImGui::IsWindowFocused();
	m_hovered = ImGui::IsWindowHovered();
	app::Application::get().getImGuiLayer()->blockEvents(!m_focused && !m_hovered);

	// Optional header (e.g. tab bar). Drawn before bounds capture so it eats its own height.
	onHeaderRender();

	// Bounds of the scene-rendering region (below the header, above the end of the window).
	const auto cursorStart = ImGui::GetCursorScreenPos();
	const auto contentRegionAvail = ImGui::GetContentRegionAvail();
	const float safeW = std::max(0.f, contentRegionAvail.x);
	const float safeH = std::max(0.f, contentRegionAvail.y);
	m_lower = {cursorStart.x, cursorStart.y};
	m_upper = {cursorStart.x + safeW, cursorStart.y + safeH};
	m_size = {static_cast<uint32_t>(safeW), static_cast<uint32_t>(safeH)};

	onRenderInternal();

	ImGui::End();
	ImGui::PopStyleVar();
}

}// namespace owl::gui
