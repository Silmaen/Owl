/**
 * @file DroneSettings.h
 * @author Silmaen
 * @date 11/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include <owl.h>

namespace drone::IO {

/**
 * @brief Class DroneSettings.
 */
class DroneSettings final {
public:
	/**
	 * @brief Destructor.
	 */
	~DroneSettings();

	DroneSettings(const DroneSettings&) = delete;
	DroneSettings(DroneSettings&&) = delete;
	auto operator=(const DroneSettings&) -> DroneSettings& = delete;
	auto operator=(DroneSettings&&) -> DroneSettings& = delete;

	/**
	 * @brief Reads the configuration from file.
	 * @param iFile The file name to read.
	 */
	void readFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save the configuration to file.
	 * @param iFile The file name to write.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;

	/**
	 * @brief Access to Settings singleton.
	 * @return Settings instance.
	 */
	static auto get() -> DroneSettings& {
		static DroneSettings instance;
		return instance;
	}

	/// If the camera should be used.
	bool useCamera = true;
	/// If a serial port should be used.
	bool useSerialPort = true;
	/// Id of the last used camera.
	int32_t cameraId = 1;
	/// The serial port in use.
	std::string serialPort;

private:
	/**
	 * @brief Constructor.
	 */
	DroneSettings();
};

}// namespace drone::IO
