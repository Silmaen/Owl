/**
 * @file Manager.h
 * @author Silmaen
 * @date 04/06/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Device.h"

namespace owl::io::serial {

/**
 * @brief Class Manager.
 */
class OWL_API Manager final {
public:
	using DeviceList = std::vector<Device>;

	Manager(const Manager&) = delete;
	Manager(Manager&&) = delete;
	Manager& operator=(const Manager&) = delete;
	Manager& operator=(Manager&&) = delete;

	/**
	 * @brief Destructor.
	 */
	~Manager();

	/**
	 * @brief Singleton accessor.
	 * @return Instance of the manager.
	 */
	static auto get() -> Manager& {
		static Manager instance;
		return instance;
	}

	/**
	 * @brief Get the number of found devices.
	 * @return The number of found devices.
	 */
	[[nodiscard]] auto getDeviceCount() const -> size_t { return m_devices.size(); }

	/**
	 * @brief Request a refresh of the devices list.
	 */
	void updateDeviceList();

	/**
	 * @brief Check if some devices exist in the list.
	 * @return True if a device exist.
	 */
	[[nodiscard]] auto hasDevice() const -> bool { return !m_devices.empty(); }

	/**
	 * @brief Get all the devices.
	 * @return List of all devices.
	 */
	[[nodiscard]] auto getAllDevices() const -> const DeviceList& { return m_devices; }

	/**
	 * @brief Get device by its name.
	 * @param iName Name of device.
	 * @return Pointer to the device of nullptr if not exists.
	 */
	[[nodiscard]] auto getDeviceByName(const std::string& iName) const -> shared<Device>;

	/**
	 * @brief Get device by its port.
	 * @param iPort Port of device.
	 * @return Pointer to the device of nullptr if not exists.
	 */
	[[nodiscard]] auto getDeviceByPort(const std::string& iPort) const -> shared<Device>;

	/**
	 * @brief Define the new current device.
	 * @param iPort The port of the new current device.
	 */
	void setCurrentDevice(const std::string& iPort);

	/**
	 * @brief Get a pointer to the current Device.
	 * @return Pointer to the current device.
	 */
	[[nodiscard]] auto getCurrentDevice() const -> const shared<Device>& { return m_currentDevice; }

private:
	/**
	 * @brief Default Constructor.
	 */
	Manager();
	/// List of device names.
	DeviceList m_devices;
	/// Pointer to the current device.
	shared<Device> m_currentDevice = nullptr;
};

}// namespace owl::io::serial
