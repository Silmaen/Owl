/**
 * @file DroneLayer.cpp
 * @author Silmaen
 * @date 16/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DroneLayer.h"

#include "IO/CameraSystem.h"
#include "IO/DeviceManager.h"
#include "IO/DroneSettings.h"
#include "panels/Gauges.h"
#include "panels/Information.h"
#include "panels/Settings.h"
#include "panels/Viewport.h"

#include <imgui.h>

using namespace owl;

namespace drone {


DroneLayer::DroneLayer() : Layer("DroneLayer") {}

void DroneLayer::onAttach() {
	OWL_PROFILE_FUNCTION()
	core::Application::get().enableDocking();

	// read settings
	if (const auto file = core::Application::get().getWorkingDirectory() / "droneConfig.yml"; exists(file))
		IO::DroneSettings::get().readFromFile(file);

	// device manager
	IO::DeviceManager::get().updateList();
	IO::CameraSystem::get().actualiseList();

	// icons
	auto& textureLib = renderer::Renderer::getTextureLibrary();
	textureLib.load("icons/exit");
	textureLib.load("icons/gauges");
	textureLib.load("icons/settings");
	textureLib.load("icons/connected");
	textureLib.load("icons/connect");

	// remote controller
	m_rc = owl::mkShared<controller::RemoteController>();

	// Panels
	m_settings = owl::mkShared<panels::Settings>();
	m_gauges = owl::mkShared<panels::Gauges>();
	m_information = owl::mkShared<panels::Information>();
	m_viewport = owl::mkShared<panels::Viewport>();

	// attach remote controller
	m_settings->setRemoteController(m_rc);
	m_gauges->setRemoteController(m_rc);
	m_information->setRemoteController(m_rc);
	m_viewport->setRemoteController(m_rc);
}

void DroneLayer::onDetach() {
	OWL_PROFILE_FUNCTION()

	IO::CameraSystem::get().invalidate();
	m_rc.reset();
	m_settings.reset();
	m_gauges.reset();
	m_information.reset();
	m_viewport.reset();

	const auto file = core::Application::get().getWorkingDirectory() / "droneConfig.yml";
	IO::DroneSettings::get().saveToFile(file);
}

void DroneLayer::onUpdate(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	renderer::Renderer2D::resetStats();

	switch (m_mode) {
		case DisplayMode::Settings:
			m_settings->onUpdate(iTimeStep);
			break;
		case DisplayMode::Gauges:
			m_viewport->onUpdate(iTimeStep);
			m_gauges->onUpdate(iTimeStep);
			m_information->onUpdate(iTimeStep);
			break;
	}
}

void DroneLayer::onEvent(event::Event& ioEvent) {

	event::EventDispatcher dispatcher(ioEvent);
	dispatcher.dispatch<event::KeyPressedEvent>(
			[this]<typename T0>(T0&& ioArgs) { return onKeyPressed(std::forward<T0>(ioArgs)); });
	dispatcher.dispatch<event::MouseButtonPressedEvent>(
			[this]<typename T0>(T0&& ioArgs) { return onMouseButtonPressed(std::forward<T0>(ioArgs)); });
}

void DroneLayer::onImGuiRender(const core::Timestep& iTimeStep) {
	OWL_PROFILE_FUNCTION()

	// ==================================================================
	if (m_showStats)
		renderStats(iTimeStep);
	//=============================================================
	renderMenu();
	//============================================================
	switch (m_mode) {
		case DisplayMode::Settings:
			m_settings->onRender();
			break;
		case DisplayMode::Gauges:
			if (m_showFakeDrone)
				renderFakeDrone(iTimeStep);
			m_viewport->onRender();
			m_gauges->onRender();
			m_information->onRender();
			break;
	}
	// ===== Toolbar: last, to be on top ========================
	renderToolbar();
}

void DroneLayer::renderStats(const core::Timestep& iTimeStep) {
	ImGui::Begin("Stats");
	ImGui::Text("%s", std::format("FPS: {:.2f}", iTimeStep.getFps()).c_str());
	ImGui::Separator();
	ImGui::Text("%s",
				std::format("Current used memory: {}", debug::TrackerAPI::globals().allocatedMemory).c_str());
	ImGui::Text("%s", std::format("Max used memory: {}", debug::TrackerAPI::globals().memoryPeek).c_str());
	ImGui::Text("%s", std::format("Allocation calls: {}", debug::TrackerAPI::globals().allocationCalls).c_str());
	ImGui::Text("%s", std::format("Deallocation calls: {}", debug::TrackerAPI::globals().deallocationCalls).c_str());
	ImGui::Separator();
	const auto stats = renderer::Renderer2D::getStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %ud", stats.drawCalls);
	ImGui::Text("Quads: %ud", stats.quadCount);
	ImGui::Text("Vertices: %ud", stats.getTotalVertexCount());
	ImGui::Text("Indices: %ud", stats.getTotalIndexCount());
	ImGui::Separator();
	ImGui::Text("%s", std::format("UAV stats").c_str());
	ImGui::Text("%s", std::format("Vel: {}", m_rc->getHorizontalVelocity()).c_str());
	ImGui::Text("%s", std::format("VSI: {} ", m_rc->getVerticalVelocity()).c_str());
	ImGui::End();
}


void DroneLayer::renderFakeDrone(const core::Timestep&) {
	ImGui::Begin("FakeDrone");
	float vel = m_rc->getHorizontalVelocity();
	if (ImGui::SliderFloat("Velocity", &vel, -5, 100))
		m_rc->setHorizontalVelocity(vel);

	float verVel = m_rc->getVerticalVelocity();
	if (ImGui::SliderFloat("VerticalVelocity", &verVel, -25, 25))
		m_rc->setVerticalVelocity(verVel);

	float alt = m_rc->getAltitude();
	if (ImGui::SliderFloat("Altitude", &alt, -10, 250))
		m_rc->setAltitude(alt);

	math::vec3 rot = m_rc->getRotations();
	if (ImGui::SliderFloat3("Rotations", &rot.x(), -180, 180))
		m_rc->setRotation(rot);

	auto motors = m_rc->getMotorRates();
	bool modif = false;
	int motorId = 1;
	for (auto& mot: motors) {
		modif |= ImGui::SliderFloat(std::format("motor {}", motorId).c_str(), &mot, -10, 9000);
		motorId++;
	}
	if (modif)
		m_rc->setMotorRates(motors);


	ImGui::End();
}

void DroneLayer::renderMenu() {
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit"))
				core::Application::get().close();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) {
			ImGui::MenuItem("Show Stats", nullptr, &m_showStats, true);
			ImGui::MenuItem("Show fake drone", nullptr, &m_showFakeDrone, true);

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

//NOLINTBEGIN(readability-convert-member-functions-to-static)
auto DroneLayer::onKeyPressed(event::KeyPressedEvent& ioEvent) -> bool {
	// Shortcuts
	if (ioEvent.getRepeatCount() > 0)
		return false;

	// return non-blocking.
	return false;
}

auto DroneLayer::onMouseButtonPressed(event::MouseButtonPressedEvent& ioEvent) -> bool {
	if (ioEvent.getMouseButton() == input::mouse::ButtonLeft) {
		return false;
	}

	// return non-blocking.
	return false;
}
//NOLINTEND(readability-convert-member-functions-to-static)

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void DroneLayer::renderToolbar() {
	ImGui::Begin("##toolbar", nullptr,
				 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	const auto& colors = ImGui::GetStyle().Colors;
	const auto& buttonHovered = colors[ImGuiCol_ButtonHovered];
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f));
	const auto& buttonActive = colors[ImGuiCol_ButtonActive];
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(buttonActive.x, buttonActive.y, buttonActive.z, 0.5f));


	const float size = ImGui::GetWindowHeight() * .95f;
	const float padding = ImGui::GetWindowHeight() * .025f;
	float posX = padding;
	const auto vsize = ImVec2(size, size);

	auto& textureLib = renderer::Renderer::getTextureLibrary();

	ImGui::SetCursorPos(ImVec2(posX, padding));
	if (const auto tex = gui::imTexture(textureLib.get("icons/settings")); tex.has_value()) {
		if (ImGui::ImageButton("##toolbar_btn_settings", tex.value(), vsize)) {
			m_mode = DisplayMode::Settings;
		}
	} else {
		if (ImGui::Button("settings", vsize)) {
			m_mode = DisplayMode::Settings;
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Settings");
	posX += size + 2.f * padding;
	ImGui::SetCursorPos(ImVec2(posX, padding));
	if (const auto tex = gui::imTexture(textureLib.get("icons/gauges")); tex.has_value()) {
		if (ImGui::ImageButton("##toolbar_btn_gauges", tex.value(), vsize)) {
			m_mode = DisplayMode::Gauges;
		}
	} else {
		if (ImGui::Button("gauges", vsize)) {
			m_mode = DisplayMode::Gauges;
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Gauges");


	const shared<renderer::Texture> iconCc =
			isConnected() ? textureLib.get("icons/connected") : textureLib.get("icons/connect");
	ImGui::SetCursorPos(ImVec2((ImGui::GetWindowContentRegionMax().x) - (2.f * size + 3.f * padding), padding));
	if (const auto tex = gui::imTexture(iconCc); tex.has_value()) {
		if (ImGui::ImageButton("##toolbar_btn_connect", tex.value(), vsize)) {
			toggleConnect();
		}
	} else {
		if (ImGui::Button(isConnected() ? "disconnect" : "connect", vsize)) {
			toggleConnect();
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(isConnected() ? "Disconnect" : "Connect");

	ImGui::SetCursorPos(ImVec2((ImGui::GetWindowContentRegionMax().x) - (size + padding), padding));
	if (const auto tex = gui::imTexture(textureLib.get("icons/exit")); tex.has_value()) {
		if (ImGui::ImageButton("##toolbar_btn_exit", tex.value(), vsize)) {
			core::Application::get().close();
		}
	} else {
		if (ImGui::Button("exit", vsize)) {
			core::Application::get().close();
		}
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Exit");
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}
OWL_DIAG_POP

auto DroneLayer::isConnected() const -> bool { return m_connected; }
void DroneLayer::toggleConnect() { m_connected = !m_connected; }

}// namespace drone
