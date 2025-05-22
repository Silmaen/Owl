/**
 * @file CameraSystem.h
 * @author Silmaen
 * @date 19/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DeviceManager.h"

#ifdef OWL_PLATFORM_WINDOWS
#include <windows.h>

namespace drone::IO {

namespace {
/**
 * @brief List The serial Port Available
 * @param listToUpdate The device's list to update.
 *
 * @note https://stackoverflow.com/questions/2674048/what-is-proper-way-to-detect-all-available-serial-ports-on-windows
 */
void enumerateSerialDevices(DeviceManager::DeviceList& listToUpdate) {
	constexpr int maxPort = 32;
	char lpTargetPath[5000];// buffer to store the path of the COMPORTS
	for (int i = 0; i < maxPort; i++)// checking ports from COM0 to COM255
	{
		const std::string ComName = "COM" + std::to_string(i);// converting to COM0, COM1, COM2


		// Test the return value and error if any
		if (QueryDosDevice(reinterpret_cast<LPCSTR>(ComName.c_str()), reinterpret_cast<LPSTR>(lpTargetPath), 5000) !=
			0) {//QueryDosDevice returns zero if it didn't find an object
			const std::string name{"usbserial"};
			const std::string path{lpTargetPath};
			OWL_TRACE("Serial Found: ({}) [{}] ", ComName.c_str(), path.c_str())
			listToUpdate.push_back({.port = "COM" + std::to_string(i), .name = name, .busInfo = path});
		}
		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {}
	}
}
}// namespace

}// namespace drone::IO
#elif OWL_PLATFORM_LINUX

#include <cerrno>
#include <fcntl.h>
#include <linux/serial.h>
#include <sys/ioctl.h>

namespace drone::IO {

namespace {
/**
 * @brief List The serial Port Available
 * @param ioListToUpdate The device's list to update.
 */
void enumerateSerialDevices(DeviceManager::DeviceList& ioListToUpdate) {
	if (const std::filesystem::path base{"/dev/serial/by-id"}; exists(base)) {
		for (const auto& devLink: std::filesystem::directory_iterator(base)) {
			if (is_symlink(devLink.symlink_status())) {
				std::string port = std::filesystem::canonical(base / read_symlink(devLink)).string();
				// todo: get device informations
				std::string name = "usbserial";
				OWL_TRACE("Serial Found: ({}) [{}] ", port.c_str(), name.c_str())
				ioListToUpdate.push_back({.port = std::move(port), .name = std::move(name)});
			}
		}
	} else {
		OWL_WARN("Unable to list serial devices by Id.")
	}
}
}// namespace

}// namespace drone::IO

#else
#error "Unsupported plateform"
#endif

namespace drone::IO {

DeviceManager::DeviceManager() = default;

DeviceManager::~DeviceManager() = default;

void DeviceManager::updateList() {
	m_devices.clear();
	// list COM Ports
	enumerateSerialDevices(m_devices);
	if (m_currentDevice)
		setCurrentDevice(m_currentDevice->port);
}

auto DeviceManager::getDeviceByName(const std::string& iName) const -> owl::shared<Device> {
	const auto it = std::ranges::find_if(m_devices.begin(), m_devices.end(),
										 [&iName](const Device& iDevice) { return iDevice.name == iName; });
	if (it == m_devices.end())
		return nullptr;
	return owl::mkShared<Device>(*it);
}

auto DeviceManager::getDeviceByPort(const std::string& iPort) const -> owl::shared<Device> {
	const auto it = std::ranges::find_if(m_devices.begin(), m_devices.end(),
										 [&iPort](const Device& iDevice) { return iDevice.port == iPort; });
	if (it == m_devices.end())
		return nullptr;
	return owl::mkShared<Device>(*it);
}

void DeviceManager::setCurrentDevice(const std::string& iPort) { m_currentDevice = getDeviceByPort(iPort); }


}// namespace drone::IO
