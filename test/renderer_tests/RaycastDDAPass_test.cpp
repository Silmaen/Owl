/**
 * @file RaycastDDAPass_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/utils/RaycastDDAPass.h>

TEST(RaycastDDAPass, initShutdownOnNullBackend) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::RaycastDDAPass pass;
	pass.init();
	EXPECT_EQ(pass.getColumnCapacity(), 0u);
	EXPECT_EQ(pass.getColumnHitBuffer(), nullptr);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(RaycastDDAPass, updateAllocatesGridAndMeta) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::RaycastDDAPass pass;
	pass.init();
	std::vector<int32_t> grid(8 * 8, -1);
	std::vector<owl::renderer::utils::RaycastDDAPass::TileMeta> meta(4);
	pass.update(grid, 8, 8, meta);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(RaycastDDAPass, dispatchAllocatesOutputBuffers) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::RaycastDDAPass pass;
	pass.init();
	std::vector<int32_t> grid(8 * 8, -1);
	std::vector<owl::renderer::utils::RaycastDDAPass::TileMeta> meta(1);
	pass.update(grid, 8, 8, meta);
	owl::renderer::utils::RaycastDDAPass::Params params;
	params.numRays = 1280;
	params.maxSteps = 32;
	params.gridWidth = 8;
	params.gridHeight = 8;
	pass.dispatch(params);
	EXPECT_EQ(pass.getColumnCapacity(), 1280u);
	EXPECT_NE(pass.getColumnHitBuffer(), nullptr);
	EXPECT_NE(pass.getHitCountBuffer(), nullptr);
	EXPECT_NE(pass.getZBufferBuffer(), nullptr);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(RaycastDDAPass, dispatchWithoutGridIsSafe) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::RaycastDDAPass pass;
	pass.init();
	owl::renderer::utils::RaycastDDAPass::Params params;
	params.numRays = 1280;
	params.gridWidth = 8;
	params.gridHeight = 8;
	pass.dispatch(params);
	EXPECT_EQ(pass.getColumnCapacity(), 0u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
