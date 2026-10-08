/**
 * @file GlslFallback_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// Slang and SPIRV-Cross come with the render module.
#if OWL_WITH_RENDER
#include "testHelper.h"

#include <core/Log.h>
#include <renderer/utils/shaderFileUtils.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace owl;
using owl::renderer::gpu::ShaderType;

namespace {

auto findRoot() -> std::filesystem::path {
	if (const auto cwd = std::filesystem::current_path();
		std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return test::getRootPath();
}

auto readFile(const std::filesystem::path& iPath) -> std::string {
	const std::ifstream in(iPath, std::ios::binary);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

}// namespace

TEST(GlslFallback, everyShippedShaderTranslatesToGlsl450) {
	core::Log::init(core::Log::Level::Off);
	const auto shaders = findRoot() / "engine_assets" / "shaders";
	size_t stageCount = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(shaders)) {
		if (entry.path().extension() != ".slang")
			continue;
		const auto name = entry.path().stem().string();
		const auto compiled = renderer::utils::compileSlangToSpirv(readFile(entry.path()), name, /*iForVulkan=*/false);
		ASSERT_TRUE(compiled.success) << name;
		for (const auto& [stage, spirv]: compiled.spirvData) {
			const auto glsl = renderer::utils::crossCompileToGlsl(spirv, name);
			ASSERT_TRUE(glsl.has_value()) << name;
			EXPECT_TRUE(glsl->starts_with("#version 450\n")) << name;
			EXPECT_EQ(glsl->find("GL_NV_gpu_shader5 : require"), std::string::npos) << name;
			EXPECT_NE(glsl->find("void main()"), std::string::npos) << name;
			++stageCount;
		}
	}
	EXPECT_GE(stageCount, 21u);
	core::Log::invalidate();
}

TEST(GlslFallback, openGlSpirvUsesOpenGlInstanceBuiltin) {
	core::Log::init(core::Log::Level::Off);
	const auto source = readFile(findRoot() / "engine_assets" / "shaders" / "renderer2D" / "slang" / "quad.slang");
	auto compiled = renderer::utils::compileSlangToSpirv(source, "quad_builtins", /*iForVulkan=*/false);
	ASSERT_TRUE(compiled.success);
	auto& vertex = compiled.spirvData[ShaderType::Vertex];
	EXPECT_GE(renderer::utils::remapBuiltinsForOpenGl(vertex), 1u);
	EXPECT_EQ(renderer::utils::remapBuiltinsForOpenGl(vertex), 0u);
	const auto glsl = renderer::utils::crossCompileToGlsl(vertex, "quad_builtins");
	ASSERT_TRUE(glsl.has_value());
	EXPECT_NE(glsl->find("gl_InstanceID"), std::string::npos);
	core::Log::invalidate();
}

TEST(GlslFallback, crossCompileRejectsEmptySpirv) {
	core::Log::init(core::Log::Level::Off);
	EXPECT_FALSE(renderer::utils::crossCompileToGlsl({}, "empty").has_value());
	EXPECT_FALSE(renderer::utils::crossCompileToGlsl({0xdeadbeef, 1, 2, 3, 4}, "garbage").has_value());
	core::Log::invalidate();
}

TEST(GlslFallback, cacheKeyCoversBackendModuleAndSource) {
	const auto key = renderer::utils::getShaderCacheKey("src", "renderer2D/quad", /*iForVulkan=*/false);
	EXPECT_NE(key, renderer::utils::getShaderCacheKey("src", "renderer2D/quad", /*iForVulkan=*/true));
	EXPECT_NE(key, renderer::utils::getShaderCacheKey("src", "renderer2D/text", /*iForVulkan=*/false));
	EXPECT_NE(key, renderer::utils::getShaderCacheKey("src2", "renderer2D/quad", /*iForVulkan=*/false));
	EXPECT_EQ(key, renderer::utils::getShaderCacheKey("src", "renderer2D/quad", /*iForVulkan=*/false));
	EXPECT_NE(key.find("slang="), std::string::npos);
	EXPECT_NE(key.find("BACKEND_OPENGL=1"), std::string::npos);
}
#endif
