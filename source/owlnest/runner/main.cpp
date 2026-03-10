/**
* @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include "RunnerLayer.h"
#include <core/EntryPoint.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl {

namespace {

/// Read the pack file path from runner.yml if present.
auto readPackFileFromConfig(const std::filesystem::path& iWorkDir) -> std::string {
	const auto config = iWorkDir / "runner.yml";
	if (!std::filesystem::exists(config))
		return {};
	try {
		const auto data = YAML::LoadFile(config.string());
		if (const auto rc = data["RunnerConfig"]; rc) {
			if (const auto pf = rc["PackFile"]; pf)
				return pf.as<std::string>();
		}
	} catch (...) { return {}; }
	return {};
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public core::Application {
public:
	OwlNest() = delete;
	explicit OwlNest(const core::AppParams& iParam) : Application(iParam) {
		if (getState() == State::Running)
			pushLayer(mkShared<nest::runner::RunnerLayer>());
	}
};
OWL_DIAG_POP

auto core::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	// Set working directory to the executable's directory so packaged games
	// work regardless of where the user launches from.
	if (iArgc > 0 && iArgv[0] != nullptr) {
		if (auto exeDir = std::filesystem::absolute(std::filesystem::path(iArgv[0])).parent_path();
			std::filesystem::exists(exeDir)) {
			std::filesystem::current_path(exeDir);
		}
	}

	const auto workDir = std::filesystem::current_path();
	const auto packFile = readPackFileFromConfig(workDir);

	return mkShared<OwlNest>(AppParams{
			.args = iArgv,
			.name = "Owl Runner",
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = "icons/logo_owl_icon.png",
			.argCount = iArgc,
			.packFile = packFile,
	});
}

}// namespace owl
