/**
 * @file main.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <app/Application.h>
#include <core/Log.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <thread>

namespace {

auto parseOptions(const int iArgc, char** iArgv) -> owl::bench::Options {
	owl::bench::Options options;
	const std::span<char*> args(iArgv, static_cast<size_t>(iArgc));
	for (const char* raw: args.subspan(1)) {
		const std::string_view arg{raw};
		const auto value = [&](const std::string_view iKey) -> std::optional<std::string> {
			if (arg.starts_with(iKey))
				return std::string{arg.substr(iKey.size())};
			return std::nullopt;
		};
		if (const auto v = value("--filter="))
			options.filter = *v;
		else if (const auto ex = value("--exclude="))
			options.exclude = *ex;
		else if (const auto s = value("--samples="))
			options.samples = static_cast<uint32_t>(std::stoul(*s));
		else if (const auto w = value("--warmup="))
			options.warmup = static_cast<uint32_t>(std::stoul(*w));
		else if (const auto m = value("--min-sample-ms="))
			options.minSampleMs = std::stod(*m);
		else if (const auto c = value("--csv="))
			options.csvPath = *c;
		else if (const auto j = value("--json="))
			options.jsonPath = *j;
		else if (arg == "--verbose")
			options.verbose = true;
		else
			std::println(stderr, "owl_bench: unknown argument '{}' (see bench/README.md).", arg);
	}
	return options;
}

auto cpuModel() -> std::string {
	std::ifstream file("/proc/cpuinfo");
	std::string line;
	while (std::getline(file, line)) {
		if (line.starts_with("model name")) {
			if (const auto pos = line.find(':'); pos != std::string::npos)
				return line.substr(pos + 2);
		}
	}
	return "unknown";
}

}// namespace

auto main(const int iArgc, char** iArgv) -> int {
	const auto options = parseOptions(iArgc, iArgv);
#if defined(NDEBUG)
	constexpr const char* buildType = "release";
#else
	constexpr const char* buildType = "debug";
#endif
	std::println("owl_bench: cpu [{}] threads [{}] compiler [{}] build [{}] samples [{}] warmup [{}]", cpuModel(),
				 std::thread::hardware_concurrency(), __VERSION__, buildType, options.samples, options.warmup);
	owl::core::Log::init(options.verbose ? owl::core::Log::Level::Warning : owl::core::Log::Level::Off);
	{
		owl::bench::Runner runner(options);
		const auto start = std::chrono::steady_clock::now();
		auto application = owl::mkShared<owl::app::Application>(
				owl::app::AppParams{.args = nullptr,
									.frameLogFrequency = 0,
									.name = "owl_bench",
									.assetsPattern = "sample_project",
									.icon = "",
									.width = 0,
									.height = 0,
									.argCount = 0,
									.renderer = owl::renderer::gpu::RenderAPI::Type::Null,
									.hasGui = false,
									.useDebugging = false,
									.isDummy = true});
		runner.metric("app/startup_ms/dummy_null_backend",
					  std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(),
					  "ms");
		owl::bench::runSceneBenches(runner);
		owl::bench::runSerializeBenches(runner);
		owl::bench::runRenderer2DBenches(runner);
		owl::bench::runFrameBenches(runner);
		owl::bench::runVoxelBenches(runner);
		owl::bench::runScriptBenches(runner);
		owl::bench::runPhysicsBenches(runner);
		owl::bench::runSampleBenches(runner);
		owl::bench::runSlangBenches(runner);
		runner.writeReports();
		owl::app::Application::invalidate();
		application.reset();
	}
	owl::core::Log::invalidate();
	return 0;
}
