/**
 * @file EditorLayer.cpp
 * @author Silmaen
 * @date 23/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "EditorLayer.h"

#include <core/Application.h>
#include <debug/Profiler.h>
#include <imgui.h>

namespace owl::raycaster {

EditorLayer::EditorLayer() = default;

EditorLayer::~EditorLayer() = default;

void EditorLayer::onAttach() {
	OWL_PROFILE_FUNCTION()
	core::Application::get().enableDocking();

	m_mapWindow = mkUniq<MapWindow>();
	m_viewPort = mkUniq<ViewPort>();
}

void EditorLayer::onDetach() {
	m_mapWindow.reset();
	m_viewPort.reset();
}

void EditorLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	m_viewPort->onUpdate(iTimeStep);
	m_mapWindow->onUpdate(iTimeStep);
}

void EditorLayer::onEvent(event::Event& ioEvent) {
	OWL_PROFILE_FUNCTION()

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioArgs) { return onKeyPressed(std::forward<T0>(ioArgs)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioArgs) { return onMouseButtonPressed(std::forward<T0>(ioArgs)); });
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
auto EditorLayer::onKeyPressed(event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (static_cast<int>(ioEvent.getRepeatCount()) > 0)
		return false;

	// return non-blocking.
	return false;
}
auto EditorLayer::onMouseButtonPressed(event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		return false;
	}

	// return non-blocking.
	return false;
}
// NOLINTEND(readability-convert-member-functions-to-static)

void EditorLayer::onImGuiRender(const core::Timestep&) {
	OWL_PROFILE_FUNCTION()
	//=============================================================
	renderMenu();
	//============================================================
	m_viewPort->onRender();
	m_mapWindow->onRender();
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
void EditorLayer::renderMenu() {
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit"))
				owl::core::Application::get().close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}
// NOLINTEND(readability-convert-member-functions-to-static)

}// namespace owl::raycaster
