/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include <core/EntryPoint.h>

#include "EditorLayer.h"

namespace owl {

class OwlNest final : public core::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const core::AppParams &param) : core::Application(param) {
		if (getState() == core::Application::State::Running)
			pushLayer(mk_shared<EditorLayer>());
	}
};

shared<core::Application> core::createApplication(int argc, char **argv) {
	return mk_shared<OwlNest>(core::AppParams{
			.name = "Owl Nest - Owl Engine Editor",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = argc,
			.args = argv,
	});
}

}// namespace owl
