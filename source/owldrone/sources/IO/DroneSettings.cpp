/**
 * @file CameraSystem.h
 * @author Silmaen
 * @date 11/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DroneSettings.h"
OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace drone::IO {

DroneSettings::DroneSettings() = default;

DroneSettings::~DroneSettings() = default;

void DroneSettings::readFromFile(const std::filesystem::path& iFile) {
	YAML::Node data = YAML::LoadFile(iFile.string());

	if (auto appConfig = data["DroneConfig"]; appConfig) {
		if (appConfig["useCamera"])
			useCamera = appConfig["useCamera"].as<bool>();
		if (appConfig["cameraId"])
			cameraId = appConfig["cameraId"].as<int>();
		if (appConfig["useSerialPort"])
			useSerialPort = appConfig["useSerialPort"].as<bool>();
		if (appConfig["SerialPort"])
			serialPort = appConfig["SerialPort"].as<std::string>();
	}
}

void DroneSettings::saveToFile(const std::filesystem::path& iFile) const {
	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "DroneConfig" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "useCamera" << YAML::Value << useCamera;
	out << YAML::Key << "cameraId" << YAML::Value << cameraId;
	out << YAML::Key << "useSerialPort" << YAML::Value << useSerialPort;
	out << YAML::Key << "SerialPort" << YAML::Value << serialPort;
	out << YAML::EndMap;
	out << YAML::EndMap;

	std::ofstream fileOut(iFile);
	fileOut << out.c_str();
	fileOut.close();
}

}// namespace drone::IO
