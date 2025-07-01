/**
 * @file Manager.cpp
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "io/video/Manager.h"

#if defined(OWL_PLATFORM_WINDOWS)
#include "windows/Device.h"
#elif defined(OWL_PLATFORM_LINUX)
#include "linux/Device.h"
#endif

namespace owl::io::video {

Manager::Manager() { updateDeviceList(); }

Manager::~Manager() { close(); }

void Manager::updateDeviceList(const bool iReset) {
	if (iReset)
		m_devices.clear();
#if defined(OWL_PLATFORM_WINDOWS)
	windows::updateList(m_devices);
#elif defined(OWL_PLATFORM_LINUX)
	linux64::updateList(m_devices);
#else
#error "Unsupported platform"
#endif
}

auto Manager::getDevicesNames() const -> std::vector<std::string> {
	std::vector<std::string> res;
	res.reserve(m_devices.size());
	for (const auto& dev: m_devices) res.emplace_back(dev->getName());
	return res;
}

auto Manager::getCurrentDevice() const -> shared<Device> {
	if (m_currentDevice >= g_maxDevices)
		return nullptr;
	return m_devices[m_currentDevice];
}

void Manager::open(const size_t iId) {
	if (iId >= m_devices.size() || iId == m_currentDevice)
		return;
	m_devices[iId]->open();
	if (m_devices[iId]->isOpened())
		m_currentDevice = iId;
}

void Manager::close() const {
	if (!isOpened())
		return;
	getCurrentDevice()->close();
}

void Manager::fillFrame(shared<renderer::Texture>& iFrame) const {
	if (!isOpened())
		return;
	getCurrentDevice()->fillFrame(iFrame);
}

auto Manager::getCurrentDeviceId() const -> int8_t {
	if (!isOpened())
		return -1;
	return static_cast<int8_t>(m_currentDevice);
}

}// namespace owl::io::video
