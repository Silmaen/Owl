/**
 * @file BaseDrawPanel.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "core/Application.h"
#include "debug/Profiler.h"
#include "gui/BaseDrawPanel.h"
#include "gui/utils.h"
#include <imgui.h>

namespace owl::gui {

BaseDrawPanel::BaseDrawPanel(std::string&& iName) : m_panelName{std::move(iName)} {}

BaseDrawPanel::~BaseDrawPanel() = default;

void BaseDrawPanel::onUpdate([[maybe_unused]] const core::Timestep& iTimeStep) { OWL_PROFILE_FUNCTION() }

void BaseDrawPanel::onRender() {
	OWL_PROFILE_FUNCTION()

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin(getName().c_str());
	const auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
	const auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	const auto viewportOffset = ImGui::GetWindowPos();
	m_viewportLower = {viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
	m_viewportUpper = {viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

	m_viewportFocused = ImGui::IsWindowFocused();
	m_viewportHovered = ImGui::IsWindowHovered();
	if (core::Application::instanced())
		core::Application::get().getImGuiLayer()->blockEvents(!m_viewportFocused && !m_viewportHovered);

	const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_viewportSize = {static_cast<uint32_t>(viewportPanelSize.x), static_cast<uint32_t>(viewportPanelSize.y)};
	if (const auto tex = imTexture(mp_framebuffer, 0); tex.has_value()) {
		ImGui::Image(tex.value(), viewportPanelSize, vec(mp_framebuffer->getLowerData()),
					 vec(mp_framebuffer->getUpperData()));
	} else
		OWL_WARN("No frameBuffer to render...")

	ImGui::End();
	ImGui::PopStyleVar();
}

}// namespace owl::gui
