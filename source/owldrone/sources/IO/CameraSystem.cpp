/**
 * @file CameraSystem.cpp
 * @author Silmaen
 * @date 28/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "CameraSystem.h"
#include "DroneSettings.h"

namespace drone::IO {

CameraSystem::CameraSystem() { resize({1, 1}); }

CameraSystem::~CameraSystem() = default;

void CameraSystem::onUpdate(const owl::core::Timestep& iTs) {
	++m_frameCount;
	auto& cameraManager = owl::io::video::Manager::get();
	auto& settings = DroneSettings::get();

	// read image from camera
	if (m_frameCount % m_frameCheck == 0) {
		if (const size_t nbCam = cameraManager.getDeviceCount(); nbCam == 0) {
			settings.useCamera = false;
		} else {
			if (settings.cameraId > static_cast<int32_t>(nbCam)) {
				settings.cameraId = 0;
			}
		}
		if (!cameraManager.isOpened()) {
			// open the right one
			if (settings.useCamera) {
				OWL_TRACE("Opening camera {}", settings.cameraId)
				cameraManager.open(static_cast<size_t>(settings.cameraId));
			}
		}
		if (cameraManager.isOpened()) {
			if (!settings.useCamera) {
				cameraManager.close();
			}
			// Define the frameSkip
			if (const float frameRateTs = iTs.getStabilizedFps(); frameRateTs > 0) {
				m_frameSkip = static_cast<int32_t>(240.f / frameRateTs);
			}
		}
	}

	if (m_frameSkip <= 0 || m_frameCount % m_frameSkip == 0) {
		if (cameraManager.isOpened()) {
			cameraManager.fillFrame(m_frame);
		} else {
			resize({1, 1});
			uint8_t color[] = {45, 87, 96};
			m_frame->setData(&color, 3);
		}
	}
}

void CameraSystem::resize(const owl::math::vec2ui& iSize) {
	if (iSize == m_size)
		return;
	m_size = iSize;
	m_frame.reset();
	m_frame = owl::renderer::Texture2D::create(
			{.size = m_size, .format = owl::renderer::ImageFormat::Rgb8, .generateMips = false});
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
void CameraSystem::setCamera(int32_t iId) {
	auto& settings = DroneSettings::get();
	auto& videoInputManager = owl::io::video::Manager::get();
	iId = owl::math::clamp(iId, 0, static_cast<int32_t>(videoInputManager.getDeviceCount() - 1));
	settings.cameraId = iId;
	if (iId == videoInputManager.getCurrentDeviceId())
		return;
	OWL_TRACE("Opening camera {}", iId)
	videoInputManager.open(static_cast<size_t>(iId));
}

void CameraSystem::actualiseList() {
	auto& videoInputManager = owl::io::video::Manager::get();
	videoInputManager.updateDeviceList();
	if (videoInputManager.getDeviceCount() == 0)
		OWL_WARN("No video capture device found.")
}

auto CameraSystem::getListOfCameraNames() const -> std::vector<std::string> {
	return owl::io::video::Manager::get().getDevicesNames();
}

auto CameraSystem::getCurrentCameraId() const -> int32_t { return owl::io::video::Manager::get().getCurrentDeviceId(); }

auto CameraSystem::getCurrentCameraName() const -> std::string {
	const auto& sys = owl::io::video::Manager::get();
	if (!sys.isOpened())
		return "";
	return sys.getDevicesNames()[static_cast<size_t>(sys.getCurrentDeviceId())];
}
// NOLINTEND(readability-convert-member-functions-to-static)

void CameraSystem::invalidate() {
	m_size = {0, 0};
	m_frameSkip = 0;
	m_frameCheck = 50;
	m_frameCount = 0;
	m_frame.reset();
}

}// namespace drone::IO
