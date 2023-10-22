/**
 * @file UAVState.cpp
 * @author Silmaen
 * @date 21/09/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "UAVState.h"

namespace drone::controller {

UAVState::UAVState(size_t motorNumber) {
	motors.resize(motorNumber);
}

void UAVState::setMotorRates(const std::vector<float> &mot) {
	if (mot.size() == motors.size()) {
		motors = mot;
	}
}

UAVState::~UAVState() = default;

}// namespace drone::controller
