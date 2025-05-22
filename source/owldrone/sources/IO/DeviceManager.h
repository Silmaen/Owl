/**
 * @file CameraSystem.h
 * @author Silmaen
 * @date 19/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace drone::IO {

static const std::string g_DeviceFriendlyName{"unknown"};
/**
 * @brief Description of a device
 */
struct Device {
	// NOLINTBEGIN(readability-redundant-member-init)
	/// The port of the device.
	std::string port;
	/// Devine Name
	std::string name;
	/// Devine Name
	std::string busInfo{};
	// NOLINTEND(readability-redundant-member-init)
	/**
	 * @brief Access to a friendly name for the device
	 * @return Friendly name of the device.
	 */
	[[nodiscard]] auto getFriendlyName() const -> const std::string& {
		if (name.empty())
			return g_DeviceFriendlyName;
		return name;
	}
	/**
	 * @brief Hashing function.
	 * @return Device's hash.
	 */
	[[nodiscard]] auto hash() const -> size_t {
		return std::hash<std::string>{}(port) ^ (std::hash<std::string>{}(name) << 1u);
	}
};

/**
 * @brief Class DeviceManager.
 */
class DeviceManager final {
public:
	using DeviceList = std::vector<Device>;

	/**
	 * @brief Destructor.
	 */
	~DeviceManager();

	DeviceManager(const DeviceManager&) = delete;
	DeviceManager(DeviceManager&&) = delete;
	auto operator=(const DeviceManager&) -> DeviceManager& = delete;
	auto operator=(DeviceManager&&) -> DeviceManager& = delete;

	/**
	 * @brief Singleton accessor.
	 * @return Instance of this object.
	 */
	static auto get() -> DeviceManager& {
		static DeviceManager instance;
		return instance;
	}

	/**
	 * @brief Update the list of devices.
	 */
	void updateList();

	/**
	 * @brief Get all the devices.
	 * @return List of all devices.
	 */
	[[nodiscard]] auto getAllDevices() const -> const DeviceList& { return m_devices; }

	/**
	 * @brief Get the number of device by type.
	 * @return The number of devices.
	 */
	[[nodiscard]] auto getDeviceCount() const -> size_t { return m_devices.size(); }

	/**
	 * @brief Get device by its name.
	 * @param iName Name of device.
	 * @return Pointer to the device of nullptr if not exists.
	 */
	[[nodiscard]] auto getDeviceByName(const std::string& iName) const -> owl::shared<Device>;

	/**
	* @brief Get device by its port.
	* @param iPort Port of device.
	* @return Pointer to the device of nullptr if not exists.
	*/
	[[nodiscard]] auto getDeviceByPort(const std::string& iPort) const -> owl::shared<Device>;

	/**
	 * @brief Define the new current device.
	 * @param iPort The port of the new current device.
	 */
	void setCurrentDevice(const std::string& iPort);

	/**
	 * @brief Get a pointer to the current Device.
	 * @return Pointer to the current device.
	 */
	[[nodiscard]] auto getCurrentDevice() const -> const owl::shared<Device>& { return m_currentDevice; }

private:
	/// List of devices.
	DeviceList m_devices;
	/// Pointer to the current device.
	owl::shared<Device> m_currentDevice = nullptr;
	/**
	 * @brief Constructor.
	 */
	DeviceManager();
};

}// namespace drone::IO
