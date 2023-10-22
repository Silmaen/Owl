/**
 * @file RemoteManager.cpp
 * @author Silmaen
 * @date 15/10/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "RemoteManager.h"

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

namespace drone::controller {

RemoteManager::RemoteManager() = default;

RemoteManager::~RemoteManager() = default;

std::vector<std::string> RemoteManager::getFlightControllerList() {
	mavsdk::Mavsdk sdk;
	auto systems = sdk.systems();
	for (const auto &sys: systems) {
		auto ver = sys->get_autopilot_version_data();
		ver.
	}
	return std::vector<std::string>();
}
}// namespace drone::controller
