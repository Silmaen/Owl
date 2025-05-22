/**
 * @file RemoteController.cpp
 * @author Silmaen
 * @date 21/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "RemoteController.h"

namespace drone::controller {

RemoteController::RemoteController(size_t iMotorNumber) { m_motors.resize(iMotorNumber); }

void RemoteController::setMotorRates(const std::vector<float>& iMotorRates) {
	if (iMotorRates.size() == m_motors.size()) {
		m_motors = iMotorRates;
	}
}

RemoteController::~RemoteController() = default;

}// namespace drone::controller
