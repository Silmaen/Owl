/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <../../owl/owl.h>

#include <../../owl/core/EntryPoint.h>

#include "EditorLayer.h"

namespace owl {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public core::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const core::AppParams &param) : core::Application(param) {
		if (getState() == core::Application::State::Running)
			pushLayer(mkShared<EditorLayer>());
	}
};
OWL_DIAG_POP

shared<core::Application> core::createApplication(int argc, char **argv) {
	return mkShared<OwlNest>(core::AppParams{
			.args = argv,
			.name = "Owl Nest - Owl Engine Editor",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = argc,
	});
}

}// namespace owl
