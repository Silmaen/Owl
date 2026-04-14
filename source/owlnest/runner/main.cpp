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

#include <cstdio>
#include <format>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl {

namespace {

/// Early configuration read from runner.yml before Application creation.
struct EarlyConfig {
	/// Pack file path.
	std::string packFile;
	/// Game display name for the window title.
	std::string gameName{"Owl Runner"};
	/// Icon file path.
	std::string icon{"icons/logo_owl_icon.png"};
	/// Window width.
	uint32_t width{1280};
	/// Window height.
	uint32_t height{720};
};

/// Read runner.yml for fields needed before Application creation.
auto readEarlyConfig(const std::filesystem::path& iWorkDir) -> EarlyConfig {
	EarlyConfig cfg;
	const auto config = iWorkDir / "runner.yml";
	if (!std::filesystem::exists(config))
		return cfg;
	try {
		const auto data = YAML::LoadFile(config.string());
		if (const auto rc = data["RunnerConfig"]; rc) {
			if (const auto pf = rc["PackFile"]; pf)
				cfg.packFile = pf.as<std::string>();
			if (const auto gn = rc["GameName"]; gn)
				cfg.gameName = gn.as<std::string>();
			if (const auto ic = rc["Icon"]; ic) {
				// Only use the icon if the file exists on disk (not just in the pack).
				if (const auto iconPath = ic.as<std::string>(); std::filesystem::exists(iWorkDir / iconPath))
					cfg.icon = iconPath;
			}
			if (const auto w = rc["WindowWidth"]; w)
				cfg.width = w.as<uint32_t>();
			if (const auto h = rc["WindowHeight"]; h)
				cfg.height = h.as<uint32_t>();
		}
	} catch (const std::exception& iEx) {
		std::fputs(std::format("Warning: failed to parse runner.yml: {}\n", iEx.what()).c_str(), stderr);
	}
	return cfg;
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
		if (const auto exeDir = std::filesystem::absolute(std::filesystem::path(iArgv[0])).parent_path();
			std::filesystem::exists(exeDir)) {
			std::filesystem::current_path(exeDir);
		}
	}

	const auto workDir = std::filesystem::current_path();
	const auto [packFile, gameName, icon, width, height] = readEarlyConfig(workDir);

	return mkShared<OwlNest>(AppParams{
			.args = iArgv,
			.name = gameName,
#ifdef OWL_ASSETS_LOCATION
			.assetsPattern = OWL_ASSETS_LOCATION,
#endif
			.icon = icon,
			.width = width,
			.height = height,
			.argCount = iArgc,
			.packFile = packFile,
	});
}

}// namespace owl
