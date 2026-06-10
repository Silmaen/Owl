/**
 * @file Renderer3D_test.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/Renderer3D.h>
#include <renderer/utils/shaderFileUtils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wshadow")
OWL_DIAG_DISABLE_GCC("-Wshadow")
#include <spirv_cross.hpp>
OWL_DIAG_POP

using namespace owl;

namespace {
auto findRoot() -> std::filesystem::path {
	auto cwd = std::filesystem::current_path();
	if (std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return owl::test::getRootPath();
}

auto loadMesh3d() -> std::string {
	const auto path = findRoot() / "engine_assets" / "shaders" / "renderer3D" / "slang" / "mesh3d.slang";
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		return {};
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}
}// namespace

TEST(Renderer3D, Mesh3DVertexIsTightlyPacked) {
	EXPECT_EQ(sizeof(renderer::Mesh3DVertex), 56u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, position), 0u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, normal), 12u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, uv), 24u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, textureIndex), 32u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, tileRect), 36u);
	EXPECT_EQ(offsetof(renderer::Mesh3DVertex, ao), 52u);
}

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
	// Scene UBO at binding 0.
	ASSERT_EQ(resources.uniform_buffers.size(), 1u);
	EXPECT_EQ(compiler.get_decoration(resources.uniform_buffers[0].id, spv::DecorationBinding), 0u);
	// Texture array at binding 1 (Vulkan offset).
	ASSERT_EQ(resources.sampled_images.size(), 1u);
	EXPECT_EQ(compiler.get_decoration(resources.sampled_images[0].id, spv::DecorationBinding), 1u);
	core::Log::invalidate();
}
