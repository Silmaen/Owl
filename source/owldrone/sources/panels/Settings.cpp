/**
 * @file Settings.cpp
 * @author Silmaen
 * @date 18/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "Settings.h"

#include "IO/CameraSystem.h"
#include "IO/DeviceManager.h"
#include "IO/DroneSettings.h"
#include <fmt/format.h>
#include <imgui.h>

namespace drone::panels {

Settings::Settings() = default;

Settings::~Settings() = default;

void Settings::onUpdate(const owl::core::Timestep&) {}

// NOLINTBEGIN(misc-const-correctness)
void Settings::onRender() {
	auto& settings = IO::DroneSettings::get();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
	ImGui::Begin("Drone Settings");
	static bool camOpen = true;
	if (ImGui::CollapsingHeader("Camera Settings", static_cast<ImGuiTreeNodeFlags>(camOpen))) {
		bool val = settings.useCamera;
		auto& camSys = IO::CameraSystem::get();
		const auto& cameras = camSys.getListOfCameraNames();
		const size_t nbCam = cameras.size();
		if (ImGui::Checkbox("Use the camera", &val)) {
			settings.useCamera = val && (nbCam > 0);
		}
		if (val && (nbCam > 0)) {
			auto cam = camSys.getCurrentCameraName();
			int32_t sCam = camSys.getCurrentCameraId();
			const int32_t cCam = sCam;
			if (ImGui::BeginCombo("Camera", fmt::format("Camera {}: {}", sCam, cam).c_str())) {
				int32_t i = 0;
				for (const auto& camera: cameras) {
					const bool isSelected = (i == cCam);
					if (ImGui::Selectable(fmt::format("Camera {}: {}", i, camera).c_str(), isSelected))
						sCam = i;
					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (isSelected)
						ImGui::SetItemDefaultFocus();
					++i;
				}
				ImGui::EndCombo();
				if (sCam != cCam) {
					camSys.setCamera(sCam);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh List")) {
				camSys.actualiseList();
			}
		}
	}
	static bool comOpen = true;
	if (ImGui::CollapsingHeader("Serial Port Setting", static_cast<ImGuiTreeNodeFlags>(comOpen))) {
		bool val = settings.useSerialPort;
		auto& deviceManager = IO::DeviceManager::get();
		const auto& devices = deviceManager.getAllDevices();
		const size_t nbCom = devices.size();
		if (ImGui::Checkbox("Use the serial ports", &val)) {
			settings.useSerialPort = val && (nbCom > 0);
		}
		if (val && (nbCom > 0)) {
			const auto device = deviceManager.getCurrentDevice();
			std::string cName = "No device";
			std::string cPort{};
			if (device) {
				cName = device->getFriendlyName();
				cPort = device->port;
			}
			if (ImGui::BeginCombo("Serial port", fmt::format("Serial {} ({})", cName, cPort).c_str())) {
				for (const auto& dev: devices) {
					const bool isSelected = (dev.port == cPort);
					if (ImGui::Selectable(fmt::format("Serial {} ({})", dev.getFriendlyName(), dev.port).c_str(),
										  isSelected))
						cPort = dev.port;
					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
				if (!cPort.empty()) {
					if (!device || cPort != device->port) {
						deviceManager.setCurrentDevice(cPort);
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh List")) {
				IO::CameraSystem::get().actualiseList();
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
// NOLINTEND(misc-const-correctness)

}// namespace drone::panels
