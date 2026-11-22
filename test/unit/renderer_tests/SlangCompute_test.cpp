/**
 * @file SlangCompute_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

// Slang and SPIRV-Cross come with the render module.
#if OWL_WITH_RENDER
#include "testHelper.h"

#include <core/Macros.h>
#include <renderer/utils/shaderFileUtils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <ranges>
#include <string>
#include <vector>

namespace {

/**
 * @brief
 *  Minimal compute shader doubling every element of a single SSBO. Exercises
 *  the full Slang→SPIR-V pipeline (entry-point discovery, target codegen)
 *  plus SPIR-V reflection of `storage_buffers`.
 */
constexpr auto kDoubleShader = R"SLANG(
[[vk::binding(0)]]
RWStructuredBuffer<float> values;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchID : SV_DispatchThreadID) {
    const uint i = dispatchID.x;
    values[i] = values[i] * 2.0f;
}
)SLANG";

constexpr auto kTwoSsboShader = R"SLANG(
[[vk::binding(0)]]
StructuredBuffer<float> inputBuffer;

[[vk::binding(2)]]
RWStructuredBuffer<float> outputBuffer;

[shader("compute")]
[numthreads(32, 1, 1)]
void computeMain(uint3 dispatchID : SV_DispatchThreadID) {
    const uint i = dispatchID.x;
    outputBuffer[i] = inputBuffer[i] + 1.0f;
}
)SLANG";

}// namespace

TEST(SlangCompute, compilesSingleSsboVulkan) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result = owl::renderer::utils::compileShader(kDoubleShader, "double_ssbo",
															owl::renderer::utils::ShaderTarget::Vulkan);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(owl::renderer::gpu::ShaderType::Compute);
	ASSERT_NE(it, result.spirvData.end());
	EXPECT_FALSE(it->second.empty());
	owl::core::Log::invalidate();
}

TEST(SlangCompute, compilesSingleSsboOpenGl) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result = owl::renderer::utils::compileShader(kDoubleShader, "double_ssbo_gl",
															owl::renderer::utils::ShaderTarget::OpenGl);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(owl::renderer::gpu::ShaderType::Compute);
	ASSERT_NE(it, result.spirvData.end());
	EXPECT_FALSE(it->second.empty());
	owl::core::Log::invalidate();
}

TEST(SlangCompute, reflectsSsboBindings) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result =
			owl::renderer::utils::compileShader(kTwoSsboShader, "two_ssbo", owl::renderer::utils::ShaderTarget::Vulkan);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(owl::renderer::gpu::ShaderType::Compute);
	ASSERT_NE(it, result.spirvData.end());
	const auto& spirv = it->second;
	ASSERT_FALSE(spirv.empty());

	const spirv_cross::Compiler compiler(spirv);
	const auto resources = compiler.get_shader_resources();
	ASSERT_EQ(resources.storage_buffers.size(), 2u);

	std::vector<uint32_t> bindings;
	bindings.reserve(2);
	for (const auto& r: resources.storage_buffers)
		bindings.push_back(compiler.get_decoration(r.id, spv::DecorationBinding));
	std::ranges::sort(bindings);
	EXPECT_EQ(bindings[0], 0u);
	EXPECT_EQ(bindings[1], 2u);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, rejectsMissingEntryPoint) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	constexpr auto noEntry = R"SLANG(
		[[vk::binding(0)]]
		RWStructuredBuffer<float> values;
	)SLANG";
	const auto result =
			owl::renderer::utils::compileShader(noEntry, "no_entry", owl::renderer::utils::ShaderTarget::Vulkan);
	EXPECT_FALSE(result.success);
	owl::core::Log::invalidate();
}

namespace {

auto findRoot() -> std::filesystem::path {
	// `owl::test::getRootPath()` infinite-loops if CWD already is the project
	// root (it skips to the parent first). Try CWD as-is, then fall back to
	// the helper walk for the typical out-of-tree test runs.
	auto cwd = std::filesystem::current_path();
	if (std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return owl::test::getRootPath();
}

auto loadShipped(const std::string& iRenderer, const std::string& iShaderName) -> std::string {
	// The engine reads a shader with its includes expanded (`common/*.slangh`).
	const auto path = findRoot() / "engine_assets" / "shaders" / iRenderer / (iShaderName + ".slang");
	if (!std::filesystem::exists(path))
		return {};
	return owl::renderer::utils::readShaderSource(path);
}

}// namespace

TEST(SlangCompute, shippedBitonicSortShaderCompiles) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("bitonic_sort", "bitonic_sort");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileShader(source, "bitonic_sort_check",
															owl::renderer::utils::ShaderTarget::Vulkan);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedFrustumCullingShaderCompiles) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("frustum_culling", "frustum_culling");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileShader(source, "frustum_culling_check",
															owl::renderer::utils::ShaderTarget::Vulkan);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedRaycastDDAShaderCompiles) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("raycast_dda", "raycast_dda");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileShader(source, "raycast_dda_check",
															owl::renderer::utils::ShaderTarget::Vulkan);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedRaycastStripeShaderCompiles) {
	OWL_REQUIRE_SHADER_COMPILER;
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("raycast_stripe", "raycast_stripe");
	ASSERT_FALSE(source.empty());
	const auto resultVk = owl::renderer::utils::compileShader(source, "raycast_stripe_vk_check",
															  owl::renderer::utils::ShaderTarget::Vulkan);
	EXPECT_TRUE(resultVk.success);
	const auto resultGl = owl::renderer::utils::compileShader(source, "raycast_stripe_gl_check",
															  owl::renderer::utils::ShaderTarget::OpenGl);
	EXPECT_TRUE(resultGl.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedShadersGiveEachStorageBufferItsOwnBlockType) {
	OWL_REQUIRE_SHADER_COMPILER;
	// The OpenGL SPIR-V path (NVIDIA) binds every storage buffer sharing a block type to the same buffer.
	owl::core::Log::init(owl::core::Log::Level::Off);
	uint32_t checked = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(findRoot() / "engine_assets" / "shaders")) {
		if (entry.path().extension() != ".slang")
			continue;
		const auto name = entry.path().stem().string();
		const auto result =
				owl::renderer::utils::compileShader(owl::renderer::utils::readShaderSource(entry.path()),
													name + "_gl_blocks", owl::renderer::utils::ShaderTarget::OpenGl);
		ASSERT_TRUE(result.success) << name;
		for (const auto& spirv: result.spirvData | std::views::values) {
			const spirv_cross::Compiler compiler(spirv);
			std::vector<uint32_t> blockTypes;
			for (const auto& buffer: compiler.get_shader_resources().storage_buffers) {
				const auto blockType = static_cast<uint32_t>(buffer.base_type_id);
				EXPECT_EQ(std::ranges::count(blockTypes, blockType), 0)
						<< name << ": " << buffer.name << " shares its block type with another storage buffer";
				blockTypes.push_back(blockType);
			}
		}
		++checked;
	}
	EXPECT_GE(checked, 10u);
	owl::core::Log::invalidate();
}
#endif
