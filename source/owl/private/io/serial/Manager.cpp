/**
 * @file Manager.cpp
 * @author Silmaen
 * @date 04/06/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "io/serial/Manager.h"

#ifdef OWL_PLATFORM_WINDOWS
#include <windows.h>
#elif OWL_PLATFORM_LINUX
#include <cerrno>
#include <fcntl.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#else
#error "Unsupported plateform"
#endif

namespace owl::io::serial {


namespace {
/**
 * @brief List The serial Port Available
 * @param ioListToUpdate The device's list to update.
 *
 * @note https://stackoverflow.com/questions/2674048/what-is-proper-way-to-detect-all-available-serial-ports-on-windows
 */
void enumerateSerialDevices(Manager::DeviceList& ioListToUpdate) {
#ifdef OWL_PLATFORM_WINDOWS
	constexpr int maxPort = 32;
	char lpTargetPath[5000];// buffer to store the path of the COMPORTS
	for (int i = 0; i < maxPort; i++)// checking ports from COM0 to COM255
	{
		const std::string comName = "COM" + std::to_string(i);// converting to COM0, COM1, COM2
		// Test the return value and error if any
		if (QueryDosDevice(reinterpret_cast<LPCSTR>(comName.c_str()), reinterpret_cast<LPSTR>(lpTargetPath), 5000) !=
			0) {//QueryDosDevice returns zero if it didn't find an object
			const std::string name{"usbserial"};
			const std::string path{lpTargetPath};
			OWL_CORE_TRACE("Serial Found: ({}) [{}] ", comName.c_str(), path.c_str())
			ioListToUpdate.emplace_back("COM" + std::to_string(i), name, path);
		}
		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {}
	}
#elif OWL_PLATFORM_LINUX
	if (const std::filesystem::path base{"/dev/serial/by-id"}; exists(base)) {
		for (const auto& devLink: std::filesystem::directory_iterator(base)) {
			if (is_symlink(devLink.symlink_status())) {
				std::string port = std::filesystem::canonical(base / read_symlink(devLink)).string();
				// todo: get device informations
				std::string name = "usbserial";
				OWL_CORE_TRACE("Serial Found: ({}) [{}] ", port.c_str(), name.c_str())
				ioListToUpdate.emplace_back(std::move(port), std::move(name), "");
			}
		}
	} else {
		OWL_CORE_WARN("Unable to list serial devices by Id.")
	}
#endif
}

}// namespace

Manager::Manager() = default;

Manager::~Manager() = default;

void Manager::updateDeviceList() {
	m_devices.clear();
	// list COM Ports
	enumerateSerialDevices(m_devices);
}

auto Manager::getDeviceByName(const std::string& iName) const -> shared<Device> {
	const auto it = std::ranges::find_if(m_devices.begin(), m_devices.end(),
										 [&iName](const Device& iDevice) { return iDevice.name() == iName; });
	if (it == m_devices.end())
		return nullptr;
	return owl::mkShared<Device>(*it);
}

auto Manager::getDeviceByPort(const std::string& iPort) const -> shared<Device> {
	const auto it = std::ranges::find_if(m_devices.begin(), m_devices.end(),
										 [&iPort](const Device& iDevice) { return iDevice.port() == iPort; });
	if (it == m_devices.end())
		return nullptr;
	return owl::mkShared<Device>(*it);
}

void Manager::setCurrentDevice(const std::string& iPort) { m_currentDevice = getDeviceByPort(iPort); }


}// namespace owl::io::serial
