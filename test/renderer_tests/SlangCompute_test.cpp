/**
 * @file SlangCompute_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/utils/shaderFileUtils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

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
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result = owl::renderer::utils::compileSlangToSpirv(kDoubleShader, "double_ssbo", /*iForVulkan=*/true);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(owl::renderer::gpu::ShaderType::Compute);
	ASSERT_NE(it, result.spirvData.end());
	EXPECT_FALSE(it->second.empty());
	owl::core::Log::invalidate();
}

TEST(SlangCompute, compilesSingleSsboOpenGl) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result =
			owl::renderer::utils::compileSlangToSpirv(kDoubleShader, "double_ssbo_gl", /*iForVulkan=*/false);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(owl::renderer::gpu::ShaderType::Compute);
	ASSERT_NE(it, result.spirvData.end());
	EXPECT_FALSE(it->second.empty());
	owl::core::Log::invalidate();
}

TEST(SlangCompute, reflectsSsboBindings) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto result = owl::renderer::utils::compileSlangToSpirv(kTwoSsboShader, "two_ssbo", /*iForVulkan=*/true);
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
	const auto result = owl::renderer::utils::compileSlangToSpirv(noEntry, "no_entry", /*iForVulkan=*/true);
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
	const auto path = findRoot() / "engine_assets" / "shaders" / iRenderer / "slang" / (iShaderName + ".slang");
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		return {};
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

}// namespace

TEST(SlangCompute, shippedWorldTransformShaderCompiles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("world_transform", "world_transform");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileSlangToSpirv(source, "world_transform_check", /*iForVulkan=*/true);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedBitonicSortShaderCompiles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("bitonic_sort", "bitonic_sort");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileSlangToSpirv(source, "bitonic_sort_check", /*iForVulkan=*/true);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedFrustumCullingShaderCompiles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("frustum_culling", "frustum_culling");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileSlangToSpirv(source, "frustum_culling_check", /*iForVulkan=*/true);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedRaycastDDAShaderCompiles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("raycast_dda", "raycast_dda");
	ASSERT_FALSE(source.empty());
	const auto result = owl::renderer::utils::compileSlangToSpirv(source, "raycast_dda_check", /*iForVulkan=*/true);
	EXPECT_TRUE(result.success);
	owl::core::Log::invalidate();
}

TEST(SlangCompute, shippedRaycastStripeShaderCompiles) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	const auto source = loadShipped("raycast_stripe", "raycast_stripe");
	ASSERT_FALSE(source.empty());
	const auto resultVk = owl::renderer::utils::compileSlangToSpirv(source, "raycast_stripe_vk_check", /*iForVulkan=*/true);
	EXPECT_TRUE(resultVk.success);
	const auto resultGl =
			owl::renderer::utils::compileSlangToSpirv(source, "raycast_stripe_gl_check", /*iForVulkan=*/false);
	EXPECT_TRUE(resultGl.success);
	owl::core::Log::invalidate();
}
