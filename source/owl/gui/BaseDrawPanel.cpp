/**
 * @file BaseDrawPanel.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "BaseDrawPanel.h"

#include "core/Application.h"

#include <debug/Profiler.h>
#include <imgui.h>

namespace owl::gui {

constexpr ImVec2 vec(math::vec2 iVec) { return {iVec.x(), iVec.y()}; }

BaseDrawPanel::BaseDrawPanel() = default;

BaseDrawPanel::~BaseDrawPanel() = default;

void BaseDrawPanel::onUpdate([[maybe_unused]] const core::Timestep &iTimeStep) { OWL_PROFILE_FUNCTION() }

// NOLINTBEGIN(performance-no-int-to-ptr)
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
	core::Application::get().getImGuiLayer()->blockEvents(!m_viewportFocused && !m_viewportHovered);

	const ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	m_viewportSize = {static_cast<uint32_t>(viewportPanelSize.x), static_cast<uint32_t>(viewportPanelSize.y)};
	if (const uint64_t textureId = mp_framebuffer->getColorAttachmentRendererId(0); textureId != 0)
		ImGui::Image(reinterpret_cast<void *>(textureId), viewportPanelSize, vec(mp_framebuffer->getLowerData()),
					 vec(mp_framebuffer->getUpperData()));
	else
		OWL_WARN("No frameBuffer to render...")

	ImGui::End();
	ImGui::PopStyleVar();
}
// NOLINTEND(performance-no-int-to-ptr)

}// namespace owl::gui
