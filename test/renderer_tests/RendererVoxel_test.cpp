/**
 * @file RendererVoxel_test.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/RenderLayerFactory.h>
#include <renderer/RendererVoxelLayer.h>
#include <renderer/utils/shaderFileUtils.h>

using namespace owl;

namespace {
auto findRoot() -> std::filesystem::path {
	auto cwd = std::filesystem::current_path();
	if (std::filesystem::exists(cwd / "CMakeLists.txt") && std::filesystem::exists(cwd / "engine_assets"))
		return cwd;
	return owl::test::getRootPath();
}

auto loadVoxelShader() -> std::string {
	const auto path = findRoot() / "engine_assets" / "shaders" / "renderer3D" / "slang" / "voxel.slang";
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open())
		return {};
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}
}// namespace

TEST(RendererVoxel, VoxelShaderCompiles) {
	core::Log::init(core::Log::Level::Off);
	const auto source = loadVoxelShader();
	ASSERT_FALSE(source.empty());
	const auto vk = renderer::utils::compileSlangToSpirv(source, "voxel_vk_check", /*iForVulkan=*/true);
	EXPECT_TRUE(vk.success);
	EXPECT_TRUE(vk.spirvData.contains(renderer::gpu::ShaderType::Vertex));
	EXPECT_TRUE(vk.spirvData.contains(renderer::gpu::ShaderType::Fragment));
	const auto gl = renderer::utils::compileSlangToSpirv(source, "voxel_gl_check", /*iForVulkan=*/false);
	EXPECT_TRUE(gl.success);
	core::Log::invalidate();
}

TEST(RendererVoxel, LayerRegistersAndCreates) {
	core::Log::init(core::Log::Level::Off);
	renderer::RendererVoxelLayer::registerWithFactory();
	EXPECT_TRUE(renderer::RenderLayerFactory::hasType("RendererVoxel"));
	const auto layer = renderer::RenderLayerFactory::create("RendererVoxel", "voxel_world");
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererVoxel");
	EXPECT_EQ(layer->getName(), "voxel_world");
	core::Log::invalidate();
}
