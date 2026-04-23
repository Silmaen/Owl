/**
 * @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include "EditorLayer.h"
#include "EditorSettings.h"
#include <core/EntryPoint.h>
#include <gui/UiLayer.h>

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
	// Load the editor settings early — the UI font size must be known before the engine builds the
	// ImGui font atlas in `UiLayer::onAttach`, which runs during `Application` construction.
	nest::EditorSettings preSettings;
	if (const auto settingsFile = std::filesystem::current_path() / "OwlNest_settings.yml"; exists(settingsFile))
		preSettings.loadFromFile(settingsFile);
	gui::UiLayer::setUiFontSize(static_cast<float>(preSettings.uiFontSize));
	gui::UiLayer::setCodeFontSize(static_cast<float>(preSettings.codeEditorFontSize));
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
