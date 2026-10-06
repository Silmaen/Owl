/**
* @file main.cpp
 * @author Silmaen
 * @date 24/11/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include <owl.h>

#include "RunnerLayer.h"
#include <app/EntryPoint.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
#include <yaml-cpp/yaml.h>
OWL_DIAG_POP

namespace owl {

namespace {
// Early configuration read from runner.yml before Application creation.
struct EarlyConfig {
	// Pack file path.
	std::string packFile;
	// Game display name for the window title.
	std::string gameName{"Owl Runner"};
	// Icon file path.
	std::string icon{"icons/logo_owl_icon.png"};
	// Window width.
	uint32_t width{1280};
	// Window height.
	uint32_t height{720};
};

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
		std::fputs("Warning: failed to parse runner.yml: ", stderr);
		std::fputs(iEx.what(), stderr);
		std::fputc('\n', stderr);
	}
	return cfg;
}

// Command-line options of the runner.
struct RunnerOptions {
	// Run without window, GPU nor audio (Null backends).
	bool headless{false};
	// Smoke-test options (inactive when frames is 0).
	nest::runner::SmokeTest smokeTest;
};

auto parseOptions(const int iArgc, char** iArgv) -> RunnerOptions {
	RunnerOptions options;
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	for (size_t i = 1; i < args.size(); ++i) {
		const std::string_view arg(args[i]);
		if (arg == "--headless") {
			options.headless = true;
		} else if (arg == "--smoke-test") {
			options.smokeTest.frames = 60;
			if (i + 1 < args.size()) {
				if (const std::string_view next(args[i + 1]); !next.empty() && std::isdigit(next.front()) != 0) {
					options.smokeTest.frames = static_cast<uint32_t>(std::stoul(std::string(next)));
					++i;
				}
			}
		}
	}
	return options;
}

}// namespace

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wweak-vtables")
class OwlNest final : public app::Application {
public:
	OwlNest() = delete;
	OwlNest(const app::AppParams& iParam, const nest::runner::SmokeTest& iSmokeTest) : Application(iParam) {
		if (getState() == State::Running)
			pushLayer(mkShared<nest::runner::RunnerLayer>(iSmokeTest));
	}

	OwlNest(const app::AppParams& iParam, const nest::runner::FrameBenchOptions& iBench) : Application(iParam) {
		if (getState() == State::Running)
			pushLayer(mkShared<nest::runner::RunnerLayer>(iBench));
		else
			setExitCode(4);
	}
};

namespace {
auto createFrameBenchApplication(const int iArgc, char** iArgv, const std::filesystem::path& iCallerDir)
		-> shared<app::Application> {
	auto options = nest::runner::parseFrameBenchOptions(iArgc, iArgv, iCallerDir);
	if (!options.has_value()) {
		std::fputs("OwlRunner --frame-bench: ", stderr);
		std::fputs(options.error().c_str(), stderr);
		std::fputs(".\nUsage: OwlRunner --frame-bench <scene.owl> [--frames N] [--warmup M] "
				   "[--backend vulkan|opengl|null] [--out results.json] [--project <dir>] [--size WxH] "
				   "[--timestep-ms T] [--vsync] [--validation] [--capture frame.png]\n",
				   stderr);
		std::exit(2);// NOLINT(concurrency-mt-unsafe)
	}
	const bool headless = options->backend == renderer::gpu::RenderAPI::Type::Null;
	return mkShared<OwlNest>(
			app::AppParams{
					.args = iArgv,
					.name = "Owl Frame Bench",
#ifdef OWL_ASSETS_LOCATION
					.assetsPattern = OWL_ASSETS_LOCATION,
#endif
					.width = options->size.x(),
					.height = options->size.y(),
					.argCount = iArgc,
					.renderer = options->backend,
					.sound = sound::SoundAPI::Type::Null,
					.hasGui = !headless,
					.useDebugging = options->validation,
					.isDummy = headless,
					.useConfigFile = false,
					.vSync = options->vSync,
			},
			*options);
}
}// namespace
OWL_DIAG_POP

auto app::createApplication(int iArgc, char** iArgv) -> shared<Application> {
	const auto callerDir = std::filesystem::current_path();
	if (iArgc > 0 && iArgv[0] != nullptr) {
		if (const auto exeDir = std::filesystem::absolute(std::filesystem::path(iArgv[0])).parent_path();
			std::filesystem::exists(exeDir)) {
			std::filesystem::current_path(exeDir);
		}
	}

	if (nest::runner::hasFrameBenchFlag(iArgc, iArgv))
		return createFrameBenchApplication(iArgc, iArgv, callerDir);

	const auto workDir = std::filesystem::current_path();
	const auto [packFile, gameName, icon, width, height] = readEarlyConfig(workDir);
	const auto options = parseOptions(iArgc, iArgv);

	AppParams params{
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
	};
	if (options.headless) {
		params.renderer = renderer::gpu::RenderAPI::Type::Null;
		params.sound = sound::SoundAPI::Type::Null;
		params.hasGui = false;
		params.isDummy = true;
	}
	return mkShared<OwlNest>(params, options.smokeTest);
}

}// namespace owl
