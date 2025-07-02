/**
 * @file Device.h
 * @author Silmaen
 * @date 05/06/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

namespace owl::io::serial {

static const std::string g_deviceFriendlyName{"unknown"};

/**
 * @brief Description of a device.
 */
class OWL_API Device final {
public:
	/**
	 * @brief Default Constructor.
	 */
	Device();

	/**
	 * @brief Copy Constructor.
	 */
	Device(const Device&) = default;

	/**
	 * @brief Move Constructor.
	 */
	Device(Device&&) = default;

	/**
	 * @brief Copy assignation.
	 */
	Device& operator=(const Device&) = default;

	/**
	 * @brief Move assignation.
	 */
	Device& operator=(Device&&) = default;

	/**
	 * @brief Element Constructor.
	 * @param[in] iPort The serial port id.
	 * @param[in] iName The serial name.
	 * @param[in] iBusInfo The serial bus informations.
	 */
	Device(std::string iPort, std::string iName, std::string iBusInfo);

	/**
	 * @brief Destructor.
	 */
	~Device();

	/**
	 * @brief Access to a friendly name for the device
	 * @return Friendly name of the device.
	 */
	[[nodiscard]] const std::string& getFriendlyName() const {
		if (m_name.empty())
			return g_deviceFriendlyName;
		return m_name;
	}
	/**
	 * @brief Hashing function.
	 * @return Device's hash.
	 */
	[[nodiscard]] size_t hash() const {
		return std::hash<std::string>{}(m_port) ^ (std::hash<std::string>{}(m_name) << 1u);
	}

	/**
	 * @brief Access to the port.
	 * @return Const reference to the port.
	 */
	const std::string& port() const { return m_port; }

	/**
	 * @brief Access to the port.
	 * @return Reference to the port.
	 */
	std::string& port() { return m_port; }

	/**
	 * @brief Access to the name.
	 * @return Const reference to the name.
	 */
	const std::string& name() const { return m_name; }

	/**
	 * @brief Access to the name.
	 * @return Reference to the name.
	 */
	std::string& name() { return m_name; }

	/**
	 * @brief Access to the busInfo.
	 * @return Const reference to the busInfo.
	 */
	const std::string& busInfo() const { return m_busInfo; }

	/**
	 * @brief Access to the busInfo.
	 * @return Reference to the busInfo.
	 */
	std::string& busInfo() { return m_busInfo; }

private:
	/// The port of the device.
	std::string m_port;
	/// Devine Name
	std::string m_name;
	/// Devine Name
	std::string m_busInfo;
};

}// namespace owl::io::serial
