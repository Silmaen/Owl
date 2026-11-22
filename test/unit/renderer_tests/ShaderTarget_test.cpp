/**
 * @file ShaderTarget_test.cpp
 * @author Silmaen
 * @date 17/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// The shader targets and the compiler hook come with the render module.
#if OWL_WITH_RENDER
#include "testHelper.h"

#include <app/Application.h>
#include <core/Log.h>
#include <renderer/utils/shaderFileUtils.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

using namespace owl;
using owl::renderer::gpu::ShaderType;
using owl::renderer::utils::ShaderTarget;

namespace {

constexpr ShaderTarget g_targets[] = {ShaderTarget::Vulkan, ShaderTarget::OpenGl, ShaderTarget::VulkanAndroid,
									  ShaderTarget::WebGpu};

/// Slang source with a vertex and a fragment entry point.
const std::string g_drawSource = "struct VOut { float4 pos: SV_Position; };\n"
								 "[shader(\"vertex\")] VOut vertexMain(uint id: SV_VertexID) {\n"
								 "  VOut o; o.pos = float4(float(id), 0.0, 0.0, 1.0); return o; }\n"
								 "[shader(\"fragment\")] float4 fragmentMain(VOut i): SV_Target {\n"
								 "  return float4(1.0, 0.0, 0.0, 1.0); }\n";

/// Slang source with one compute entry point.
const std::string g_computeSource = "[shader(\"compute\")]\n[numthreads(1, 1, 1)]\n"
									"void computeMain(uint3 iId: SV_DispatchThreadID) {}\n";

/// Removes the shader compiler for the lifetime of the guard, as in the runner of an exported game.
class NoShaderCompiler {
public:
	NoShaderCompiler() : m_saved{renderer::utils::setShaderCompiler({})} {}
	NoShaderCompiler(const NoShaderCompiler&) = delete;
	NoShaderCompiler(NoShaderCompiler&&) = delete;
	auto operator=(const NoShaderCompiler&) -> NoShaderCompiler& = delete;
	auto operator=(NoShaderCompiler&&) -> NoShaderCompiler& = delete;
	~NoShaderCompiler() { renderer::utils::setShaderCompiler(std::move(m_saved)); }

private:
	renderer::utils::ShaderCompiler m_saved;
};

}// namespace

TEST(ShaderTarget, FolderNamesRoundTrip) {
	for (const auto target: g_targets) {
		const auto folder = renderer::utils::getShaderTargetFolder(target);
		EXPECT_EQ(renderer::utils::parseShaderTarget(folder), target) << folder;
	}
	EXPECT_EQ(renderer::utils::getShaderTargetFolder(ShaderTarget::VulkanAndroid), "vulkan_android");
	EXPECT_FALSE(renderer::utils::parseShaderTarget("metal").has_value());
	EXPECT_FALSE(renderer::utils::parseShaderTarget("").has_value());
}

TEST(ShaderTarget, EveryTargetHasItsOwnCacheKey) {
	for (const auto first: g_targets) {
		for (const auto second: g_targets) {
			if (first == second)
				continue;
			EXPECT_NE(renderer::utils::getShaderCacheKey("src", "renderer2D/quad", first),
					  renderer::utils::getShaderCacheKey("src", "renderer2D/quad", second));
		}
	}
	// Both Vulkan targets compile the Vulkan branch of the shaders.
	EXPECT_EQ(renderer::utils::getShaderTargetMacro(ShaderTarget::VulkanAndroid), "BACKEND_VULKAN");
	EXPECT_EQ(renderer::utils::getShaderTargetMacro(ShaderTarget::WebGpu), "BACKEND_WEBGPU");
	EXPECT_EQ(renderer::utils::getShaderTargetProfile(ShaderTarget::VulkanAndroid), "spirv_1_3");
}

TEST(ShaderTarget, AndroidTargetWritesSpirv13) {
	OWL_REQUIRE_SHADER_COMPILER;
	core::Log::init(core::Log::Level::Off);
	const auto result = renderer::utils::compileShader(g_drawSource, "android_check", ShaderTarget::VulkanAndroid);
	ASSERT_TRUE(result.success);
	ASSERT_EQ(result.spirvData.size(), 2u);
	EXPECT_TRUE(result.wgslData.empty());
	for (const auto& [stage, spirv]: result.spirvData) {
		ASSERT_GT(spirv.size(), 1u);
		// Word 1 of the header is the SPIR-V version: 1.3 runs on Vulkan 1.1, the floor of the Android devices.
		EXPECT_EQ(spirv[1], uint32_t{0x00010300});
		EXPECT_FALSE(renderer::utils::checkSpirv(spirv, stage).has_value());
	}
	core::Log::invalidate();
}

TEST(ShaderTarget, WebGpuTargetWritesWgsl) {
	OWL_REQUIRE_SHADER_COMPILER;
	core::Log::init(core::Log::Level::Off);
	const auto draw = renderer::utils::compileShader(g_drawSource, "wgsl_draw", ShaderTarget::WebGpu);
	ASSERT_TRUE(draw.success);
	EXPECT_TRUE(draw.spirvData.empty());
	ASSERT_TRUE(draw.wgslData.contains(ShaderType::Vertex));
	ASSERT_TRUE(draw.wgslData.contains(ShaderType::Fragment));
	EXPECT_NE(draw.wgslData.at(ShaderType::Vertex).find("@vertex"), std::string::npos);
	EXPECT_NE(draw.wgslData.at(ShaderType::Fragment).find("@fragment"), std::string::npos);
	const auto compute = renderer::utils::compileShader(g_computeSource, "wgsl_compute", ShaderTarget::WebGpu);
	ASSERT_TRUE(compute.success);
	EXPECT_NE(compute.wgslData.at(ShaderType::Compute).find("@compute"), std::string::npos);
	EXPECT_EQ(renderer::utils::getPrecompiledWgslPath("quad", "renderer2D", ShaderType::Fragment),
			  std::filesystem::path("shaders/renderer2D/wgsl/quad.frag.wgsl"));
	core::Log::invalidate();
}

// The test programs link the compiler where Slang exists: not on the Web, whose runner reads baked shaders.
TEST(ShaderTarget, TestsHaveTheShaderCompilerWhereTheyLinkIt) {
	EXPECT_EQ(renderer::utils::hasShaderCompiler(), OWL_WITH_SHADER_COMPILER == 1);
}

TEST(ShaderTarget, WithoutCompilerNothingCompiles) {
	OWL_REQUIRE_SHADER_COMPILER;
	core::Log::init(core::Log::Level::Off);
	{
		const NoShaderCompiler guard;
		EXPECT_FALSE(renderer::utils::hasShaderCompiler());
		EXPECT_FALSE(renderer::utils::compileShader(g_computeSource, "no_compiler", ShaderTarget::Vulkan).success);
		// No baked stage for this source: the runner reports the shader instead of compiling it.
		EXPECT_FALSE(renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_unbaked", "owl_test_no_compiler",
														 ShaderTarget::Vulkan, {ShaderType::Compute})
							 .has_value());
		// A driver refused a baked stage: nothing better can be produced.
		EXPECT_FALSE(renderer::utils::recompileSpirv(g_computeSource, "owl_unbaked", "owl_test_no_compiler",
													 ShaderTarget::Vulkan, "shaders/owl_test_no_compiler",
													 "refused by the test")
							 .has_value());
	}
	EXPECT_TRUE(renderer::utils::hasShaderCompiler());
	EXPECT_TRUE(renderer::utils::compileShader(g_computeSource, "compiler_back", ShaderTarget::Vulkan).success);
	core::Log::invalidate();
}

TEST(ShaderTarget, WithoutCompilerBakedStagesStillLoad) {
	OWL_REQUIRE_SHADER_COMPILER;
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "shaderNoCompiler",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_baked", ""));
	// Compiled once with the compiler (into the cache), then read back without it.
	ASSERT_TRUE(renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_baked",
													ShaderTarget::Vulkan, {ShaderType::Compute})
						.has_value());
	{
		const NoShaderCompiler guard;
		const auto loaded = renderer::utils::loadOrCompileSpirv(g_computeSource, "owl_empty", "owl_test_baked",
																ShaderTarget::Vulkan, {ShaderType::Compute});
		ASSERT_TRUE(loaded.has_value());
		EXPECT_FALSE(loaded->origin.empty());
		// An edited source has no baked stage any more.
		EXPECT_FALSE(renderer::utils::loadOrCompileSpirv(g_computeSource + "\n// edited\n", "owl_empty",
														 "owl_test_baked", ShaderTarget::Vulkan, {ShaderType::Compute})
							 .has_value());
	}
	std::filesystem::remove_all(renderer::utils::getCacheDirectory("owl_test_baked", ""));
	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
#endif
