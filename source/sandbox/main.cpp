/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include <core/EntryPoint.h>

#include "base2D.h"

class Sandbox final : public owl::core::Application {
public:
	explicit Sandbox(const owl::core::AppParams &param) : Application(param) { pushLayer(owl::mk_shared<owl::base2D>()); }
};

owl::shared<owl::core::Application> owl::core::createApplication(int argc, char **argv) {
	return owl::mk_shared<Sandbox>(core::AppParams{
			.name = "Sandbox",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.argCount = argc,
			.args = argv,
	});
}
