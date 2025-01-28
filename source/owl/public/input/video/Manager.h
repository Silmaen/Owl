/**
 * @file Manager.h
 * @author Silmaen
 * @date 03/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Device.h"

/**
 * @brief namespace for video input devices.
 */
namespace owl::input::video {

constexpr size_t g_maxDevices = 10;

/**
 * @brief Class managing the video input devices.
 */
class OWL_API Manager final {
public:
	Manager(const Manager&) = delete;
	Manager(Manager&&) = delete;
	auto operator=(const Manager&) -> Manager& = delete;
	auto operator=(Manager&&) -> Manager& = delete;

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
	 * @param[in] iReset the list will be reset before search, closing all the devices.
	 */
	void updateDeviceList(bool iReset = false);

	/**
	 * @brief Get an ordered list of device names.
	 * @return List of device name.
	 */
	[[nodiscard]] auto getDevicesNames() const -> std::vector<std::string>;

	/**
	 * @brief Access to the current device.
	 * @return Pointer to the current device of nullptr if not opened.
	 */
	[[nodiscard]] auto getCurrentDevice() const -> shared<Device>;
	/**
	 * @brief Access to the current device id.
	 * @return Id of the current device of -1 if not opened.
	 */
	[[nodiscard]] auto getCurrentDeviceId() const -> int8_t;

	/**
	 * @brief Check if a device is open.
	 * @return True if a device is open.
	 */
	[[nodiscard]] auto isOpened() const -> bool { return m_currentDevice < g_maxDevices; }

	/**
	 * @brief Check if some devices exist in the list.
	 * @return True if a device exist.
	 */
	[[nodiscard]] auto hasDevice() const -> bool { return !m_devices.empty(); }

	/**
	 * @brief Open the device with the selected id.
	 * @param[in] iId The id in the list of devices.
	 */
	void open(size_t iId);

	/**
	 * @brief Close the current device.
	 */
	void close() const;

	/**
	 * @brief Grab a frame and put it into the Texture.
	 * @param[in] iFrame The texture to update.
	 */
	void fillFrame(shared<renderer::Texture>& iFrame) const;

private:
	/**
	 * @brief Default Constructor.
	 */
	Manager();
	/// List of devices.
	std::vector<shared<Device>> m_devices;
	/// index of the current device in the list.
	size_t m_currentDevice = g_maxDevices;
};

}// namespace owl::input::video
