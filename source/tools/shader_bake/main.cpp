/**
 * @file main.cpp
 * @author Silmaen
 * @date 09/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include <core/Log.h>
#include <renderer/utils/shaderFileUtils.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Build-time compilation of the engine shaders: every `shaders/<renderer>/<name>.slang`, for each target asked
// (Vulkan and OpenGL by default), written where `loadOrCompileSpirv` looks first (`getPrecompiledShaderPath` under
// the output folder; `getPrecompiledWgslPath` for WebGPU).

namespace {

using owl::renderer::utils::ShaderTarget;

// Not std::println: on MinGW its console path lives in libstdc++exp, which nothing links.
template<typename... Args>
void printLine(std::FILE* iStream, std::format_string<Args...> iFormat, Args&&... iArgs) {
	std::fputs(std::format(iFormat, std::forward<Args>(iArgs)...).append("\n").c_str(), iStream);
}

auto writeText(const std::filesystem::path& iFile, const std::string& iText) -> bool {
	std::ofstream out(iFile, std::ios::binary);
	out << iText;
	return out.good();
}

auto bakeShader(const std::filesystem::path& iSourceFile, const std::filesystem::path& iAssetsDir,
				const std::vector<ShaderTarget>& iTargets) -> int {
	using namespace owl::renderer::utils;
	const std::string name = iSourceFile.stem().string();
	const std::string renderer = iSourceFile.parent_path().filename().string();
	const std::string source = readShaderSource(iSourceFile);
	int stages = 0;
	for (const auto target: iTargets) {
		const auto compiled = compileShader(source, name, target);
		if (!compiled.success) {
			printLine(stderr, "OwlShaderBake: {} failed for {}.", iSourceFile.string(), getShaderTargetFolder(target));
			return -1;
		}
		const auto cacheKey = getShaderCacheKey(source, std::format("{}/{}", renderer, name), target);
		const auto store = [&](const std::filesystem::path& iOutput, const auto& iWrite) -> bool {
			std::filesystem::create_directories(iOutput.parent_path());
			if (!iWrite(iOutput)) {
				printLine(stderr, "OwlShaderBake: cannot write {}.", iOutput.string());
				return false;
			}
			writeShaderHash(iOutput, cacheKey);
			++stages;
			return true;
		};
		for (const auto& [stage, spirv]: compiled.spirvData) {
			const auto output =
					iAssetsDir / getPrecompiledShaderPath(name, renderer, getShaderTargetFolder(target), stage);
			if (!store(output,
					   [&](const std::filesystem::path& iFile) -> bool { return writeCachedShader(iFile, spirv); }))
				return -1;
		}
		for (const auto& [stage, wgsl]: compiled.wgslData) {
			if (!store(iAssetsDir / getPrecompiledWgslPath(name, renderer, stage),
					   [&](const std::filesystem::path& iFile) -> bool { return writeText(iFile, wgsl); }))
				return -1;
		}
	}
	return stages;
}

// `--targets=vulkan,opengl,...` (folder names of `getShaderTargetFolder`), Vulkan and OpenGL when absent.
auto parseTargets(const std::string_view iArgument) -> std::optional<std::vector<ShaderTarget>> {
	constexpr std::string_view prefix = "--targets=";
	if (!iArgument.starts_with(prefix))
		return std::nullopt;
	std::vector<ShaderTarget> targets;
	for (const auto part: std::views::split(iArgument.substr(prefix.size()), ',')) {
		const auto target = owl::renderer::utils::parseShaderTarget(std::string_view{part.begin(), part.end()});
		if (!target.has_value())
			return std::nullopt;
		targets.push_back(*target);
	}
	if (targets.empty())
		return std::nullopt;
	return targets;
}

auto bake(const int iArgc, char** iArgv) -> int {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	std::vector targets{ShaderTarget::Vulkan, ShaderTarget::OpenGl};
	if (args.size() == 4) {
		const auto parsed = parseTargets(args[3]);
		if (!parsed.has_value()) {
			printLine(stderr, "OwlShaderBake: bad target list '{}' (vulkan, opengl, vulkan_android, webgpu).", args[3]);
			return 2;
		}
		targets = *parsed;
	} else if (args.size() != 3) {
		printLine(stderr, "Usage: OwlShaderBake <engine shaders folder> <output assets folder> "
						  "[--targets=vulkan,opengl,vulkan_android,webgpu]");
		return 2;
	}
	const std::filesystem::path shadersDir{args[1]};
	const std::filesystem::path assetsDir{args[2]};
	owl::core::Log::init(owl::core::Log::Level::Warning);
	const auto start = std::chrono::steady_clock::now();
	int shaders = 0;
	int stages = 0;
	int failures = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(shadersDir)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".slang")
			continue;
		if (const int baked = bakeShader(entry.path(), assetsDir, targets); baked < 0) {
			++failures;
		} else {
			++shaders;
			stages += baked;
		}
	}
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
	printLine(stdout, "OwlShaderBake: {} shaders, {} stages for {} targets in {:.0f} ms, {} failures.", shaders, stages,
			  targets.size(), elapsed.count(), failures);
	owl::core::Log::invalidate();
	return failures == 0 ? 0 : 1;
}

}// namespace

auto main(const int iArgc, char** iArgv) -> int {
	try {
		return bake(iArgc, iArgv);
	} catch (const std::exception& iError) {
		std::fputs("OwlShaderBake: ", stderr);
		std::fputs(iError.what(), stderr);
		std::fputs("\n", stderr);
	} catch (...) { std::fputs("OwlShaderBake: unknown error.\n", stderr); }
	return 1;
}
