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


OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class Sandbox final : public owl::core::Application {
public:
	explicit Sandbox(const owl::core::AppParams& iParam) : Application(iParam) {
		pushLayer(owl::mkShared<owl::Base2D>());
	}
};
OWL_DIAG_POP

auto owl::core::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	return owl::mkShared<Sandbox>(AppParams{
			.args = iArgv,
			.name = "Sandbox",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.argCount = iArgc,
	});
}
