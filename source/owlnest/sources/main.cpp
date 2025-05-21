/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include "EditorLayer.h"
#include <core/EntryPoint.h>

namespace owl {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public core::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const core::AppParams& iParam) : Application(iParam) {
		if (getState() == State::Running)
			pushLayer(mkShared<nest::EditorLayer>());
	}
};
OWL_DIAG_POP

auto core::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	return mkShared<OwlNest>(AppParams{
			.args = iArgv,
			.name = "Owl Nest - Owl Engine Editor",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = iArgc,
	});
}

}// namespace owl
