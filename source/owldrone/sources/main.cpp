/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include <core/EntryPoint.h>

#include "DroneLayer.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public owl::core::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const owl::core::AppParams& iParameters) : Application(iParameters) {
		if (getState() == State::Running)
			pushLayer(owl::mkShared<drone::DroneLayer>());
	}
};
OWL_DIAG_POP

auto owl::core::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	return mkShared<OwlNest>(AppParams{
			.args = iArgv,
			.name = "Owl Drone - Navigator for drone",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_drone.png",
			.argCount = iArgc,
	});
}
