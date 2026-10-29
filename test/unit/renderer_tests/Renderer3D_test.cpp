/**
 * @file Renderer3D_test.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/LocalShadows.h>
#include <renderer/Renderer3D.h>
#if OWL_WITH_RENDER
#include <renderer/utils/shaderFileUtils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

#endif

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

using namespace owl;

TEST(Renderer3D, Mesh3DVertexIsTightlyPacked) {
	EXPECT_EQ(sizeof(renderer::Mesh3DVertex), 56u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, position), 0u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, normal), 12u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, uv), 24u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, textureIndex), 32u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, tileRect), 36u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, ao), 52u);
}

// The shader checks need the Slang compiler of the render module.
#if OWL_WITH_RENDER
namespace {
auto findRoot() -> std::filesystem::path {
	auto cwd = std::filesystem::current_path();
	if (std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return owl::test::getRootPath();
}

auto loadMesh3d() -> std::string {
	const auto path = findRoot() / "engine_assets" / "shaders" / "renderer3D" / "mesh3d.slang";
	return renderer::utils::readShaderSource(path);
}
}// namespace

TEST(Renderer3D, Mesh3dShaderCompilesVulkan) {
	core::Log::init(core::Log::Level::Off);
	const auto source = loadMesh3d();
	ASSERT_FALSE(source.empty());
	const auto result = renderer::utils::compileSlangToSpirv(source, "mesh3d_vk_check", /*iForVulkan=*/true);
	ASSERT_TRUE(result.success);
	EXPECT_TRUE(result.spirvData.contains(renderer::gpu::ShaderType::Vertex));
	EXPECT_TRUE(result.spirvData.contains(renderer::gpu::ShaderType::Fragment));
	core::Log::invalidate();
}

TEST(Renderer3D, Mesh3dShaderCompilesOpenGl) {
	core::Log::init(core::Log::Level::Off);
	const auto source = loadMesh3d();
	ASSERT_FALSE(source.empty());
	const auto result = renderer::utils::compileSlangToSpirv(source, "mesh3d_gl_check", /*iForVulkan=*/false);
	EXPECT_TRUE(result.success);
	core::Log::invalidate();
}

TEST(Renderer3D, Mesh3dShaderBindings) {
	core::Log::init(core::Log::Level::Off);
	const auto source = loadMesh3d();
	ASSERT_FALSE(source.empty());
	const auto result = renderer::utils::compileSlangToSpirv(source, "mesh3d_reflect", /*iForVulkan=*/true);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(renderer::gpu::ShaderType::Fragment);
	ASSERT_NE(it, result.spirvData.end());
	ASSERT_FALSE(it->second.empty());

	const spirv_cross::Compiler compiler(it->second);
	const auto resources = compiler.get_shader_resources();
	// Scene UBO at binding 0, lighting UBO at binding 3, material UBO at binding 4, lights SSBO at binding 2.
	ASSERT_EQ(resources.uniform_buffers.size(), 3u);
	std::vector<uint32_t> uniformBindings;
	for (const auto& buffer: resources.uniform_buffers)
		uniformBindings.push_back(compiler.get_decoration(buffer.id, spv::DecorationBinding));
	std::ranges::sort(uniformBindings);
	EXPECT_EQ(uniformBindings, (std::vector<uint32_t>{0u, 3u, 4u}));
	ASSERT_EQ(resources.storage_buffers.size(), 1u);
	EXPECT_EQ(compiler.get_decoration(resources.storage_buffers[0].id, spv::DecorationBinding), 2u);
	// Texture array at binding 1 (Vulkan offset), cube array at binding 7.
	ASSERT_EQ(resources.sampled_images.size(), 2u);
	std::vector<uint32_t> imageBindings;
	for (const auto& image: resources.sampled_images)
		imageBindings.push_back(compiler.get_decoration(image.id, spv::DecorationBinding));
	std::ranges::sort(imageBindings);
	EXPECT_EQ(imageBindings, (std::vector<uint32_t>{1u, 7u}));
	core::Log::invalidate();
}

TEST(Renderer3D, Mesh3dOpenGlSamplesAtMost32Textures) {
	core::Log::init(core::Log::Level::Off);
	const auto source = loadMesh3d();
	ASSERT_FALSE(source.empty());
	const auto result = renderer::utils::compileSlangToSpirv(source, "mesh3d_gl_units", /*iForVulkan=*/false);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(renderer::gpu::ShaderType::Fragment);
	ASSERT_NE(it, result.spirvData.end());
	const spirv_cross::Compiler compiler(it->second);
	const auto resources = compiler.get_shader_resources();
	// The 2D textures take the units from 0, the cubes the units right after them.
	uint32_t units = 0;
	uint32_t cubeBinding = 0;
	for (const auto& image: resources.sampled_images) {
		const auto& type = compiler.get_type(image.type_id);
		const uint32_t count = type.array.empty() ? 1u : type.array[0];
		units += count;
		if (type.image.dim == spv::DimCube)
			cubeBinding = compiler.get_decoration(image.id, spv::DecorationBinding);
		else
			EXPECT_EQ(compiler.get_decoration(image.id, spv::DecorationBinding), 0u);
	}
	EXPECT_LE(units, 32u);
	EXPECT_EQ(cubeBinding, renderer::Renderer3D::g_spotShadowSlot + renderer::g_openGlLocalShadowBudget.spot);
	core::Log::invalidate();
}
TEST(Renderer3D, SkinnedShaderCompilesAndReadsThePaletteAtTheInstanceBinding) {
	core::Log::init(core::Log::Level::Off);
	const auto path = findRoot() / "engine_assets" / "shaders" / "renderer3D" / "skinned3d.slang";
	const auto source = renderer::utils::readShaderSource(path);
	ASSERT_FALSE(source.empty());
	EXPECT_TRUE(renderer::utils::compileSlangToSpirv(source, "skinned3d_gl_check", /*iForVulkan=*/false).success);
	const auto result = renderer::utils::compileSlangToSpirv(source, "skinned3d_vk_check", /*iForVulkan=*/true);
	ASSERT_TRUE(result.success);
	const auto it = result.spirvData.find(renderer::gpu::ShaderType::Vertex);
	ASSERT_NE(it, result.spirvData.end());
	const spirv_cross::Compiler compiler(it->second);
	const auto resources = compiler.get_shader_resources();
	ASSERT_EQ(resources.storage_buffers.size(), 1u);
	EXPECT_EQ(compiler.get_decoration(resources.storage_buffers[0].id, spv::DecorationBinding),
			  renderer::Renderer3D::g_skinBinding);
	EXPECT_EQ(resources.stage_inputs.size(), 8u);
	core::Log::invalidate();
}

TEST(Renderer3D, SkinnedVertexAddsTheJointsToTheMeshVertex) {
	EXPECT_EQ(sizeof(renderer::SkinnedMesh3DVertex), 88u);
	EXPECT_EQ(offsetof(renderer::SkinnedMesh3DVertex, joints), 56u);
	EXPECT_EQ(offsetof(renderer::SkinnedMesh3DVertex, weights), 72u);
}
#endif
