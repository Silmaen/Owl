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
#include <filesystem>
#include <fstream>
#include <print>
#include <span>
#include <sstream>
#include <string>

// Build-time compilation of the engine shaders: every `shaders/<renderer>/slang/<name>.slang`, for Vulkan and
// OpenGL, written where `loadOrCompileSpirv` looks first (`getPrecompiledShaderPath` under the output folder).

namespace {

auto readText(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file(iPath, std::ios::binary);
	std::stringstream content;
	content << file.rdbuf();
	return content.str();
}

auto bakeShader(const std::filesystem::path& iSourceFile, const std::filesystem::path& iAssetsDir) -> int {
	using namespace owl::renderer::utils;
	const std::string name = iSourceFile.stem().string();
	const std::string renderer = iSourceFile.parent_path().parent_path().filename().string();
	const std::string source = readText(iSourceFile);
	int stages = 0;
	for (const bool forVulkan: {true, false}) {
		const std::string api = forVulkan ? "vulkan" : "opengl";
		const auto compiled = compileSlangToSpirv(source, name, forVulkan);
		if (!compiled.success) {
			std::println(stderr, "OwlShaderBake: {} failed for {}.", iSourceFile.string(), api);
			return -1;
		}
		const auto cacheKey = getShaderCacheKey(source, renderer + "/" + name, forVulkan);
		for (const auto& [stage, spirv]: compiled.spirvData) {
			const auto output = iAssetsDir / getPrecompiledShaderPath(name, renderer, api, stage);
			std::filesystem::create_directories(output.parent_path());
			if (!writeCachedShader(output, spirv)) {
				std::println(stderr, "OwlShaderBake: cannot write {}.", output.string());
				return -1;
			}
			writeShaderHash(output, cacheKey);
			++stages;
		}
	}
	return stages;
}

}// namespace

auto main(const int iArgc, char** iArgv) -> int {
	const std::span args(iArgv, static_cast<size_t>(iArgc));
	if (args.size() != 3) {
		std::println(stderr, "Usage: OwlShaderBake <engine shaders folder> <output assets folder>");
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
		if (!entry.is_regular_file() || entry.path().extension() != ".slang" ||
			entry.path().parent_path().filename() != "slang")
			continue;
		if (const int baked = bakeShader(entry.path(), assetsDir); baked < 0) {
			++failures;
		} else {
			++shaders;
			stages += baked;
		}
	}
	const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start);
	std::println("OwlShaderBake: {} shaders, {} SPIR-V stages in {:.0f} ms, {} failures.", shaders, stages,
				 elapsed.count(), failures);
	owl::core::Log::invalidate();
	return failures == 0 ? 0 : 1;
}
