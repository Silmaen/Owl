/**
 * @file SlangBench.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "cases/Cases.h"

#include <renderer/utils/shaderFileUtils.h>

#include <format>
#include <fstream>
#include <sstream>

namespace owl::bench {

namespace {

auto readText(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream file(iPath);
	std::stringstream content;
	content << file.rdbuf();
	return content.str();
}

}// namespace

void runSlangBenches(Runner& ioRunner) {
	if (!ioRunner.wants("slang"))
		return;
	const std::filesystem::path shaders = std::filesystem::path(OWL_BENCH_SOURCE_DIR) / "engine_assets" / "shaders";
	const std::string quad = readText(shaders / "renderer2D" / "slang" / "quad.slang");
	const std::string voxel = readText(shaders / "renderer3D" / "slang" / "voxel.slang");
	if (quad.empty() || voxel.empty())
		return;
	renderer::utils::SlangCompilationResult first;
	ioRunner.measureOnce("slang/compile_cold/quad_vulkan_first_in_process",
						 [&]() -> void { first = renderer::utils::compileSlangToSpirv(quad, "quad", true); });
	ioRunner.metric("slang/compile_cold/success", first.success ? 1.0 : 0.0, "bool");
	ioRunner.measure("slang/compile_warm/quad_vulkan", 1, [&]() -> void {
		doNotOptimize(renderer::utils::compileSlangToSpirv(quad, "quad", true).success);
	});
	ioRunner.measure("slang/compile_warm/quad_opengl", 1, [&]() -> void {
		doNotOptimize(renderer::utils::compileSlangToSpirv(quad, "quad", false).success);
	});
	ioRunner.measure("slang/compile_warm/voxel_vulkan", 1, [&]() -> void {
		doNotOptimize(renderer::utils::compileSlangToSpirv(voxel, "voxel", true).success);
	});
	std::vector<std::pair<std::string, std::string>> all;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(shaders))
		if (entry.path().extension() == ".slang")
			all.emplace_back(entry.path().stem().string(), readText(entry.path()));
	ioRunner.metric("slang/engine_shader_files", static_cast<double>(all.size()), "files");
	ioRunner.measure("slang/compile_warm/all_engine_shaders_vulkan", all.size(), [&]() -> void {
		for (const auto& [name, source]: all)
			doNotOptimize(renderer::utils::compileSlangToSpirv(source, name, true).success);
	});
	if (!first.success || first.spirvData.empty())
		return;
	const auto dir = std::filesystem::temp_directory_path() / "owl_bench_spv";
	std::filesystem::create_directories(dir);
	const auto cached = dir / "quad.vert.spv";
	const auto& spirv = first.spirvData.begin()->second;
	std::ignore = renderer::utils::writeCachedShader(cached, spirv);
	renderer::utils::writeShaderHash(cached, quad);
	ioRunner.metric("slang/spirv_words/quad_first_stage", static_cast<double>(spirv.size()), "words");
	ioRunner.measure("slang/cache_hit/quad_validate_and_read", 1, [&]() -> void {
		doNotOptimize(renderer::utils::isShaderCacheValid(cached, quad));
		doNotOptimize(renderer::utils::readCachedShader(cached).size());
	});
	std::filesystem::remove_all(dir);
}

}// namespace owl::bench
