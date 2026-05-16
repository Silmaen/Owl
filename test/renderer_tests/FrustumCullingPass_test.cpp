/**
 * @file FrustumCullingPass_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/utils/FrustumCullingPass.h>

TEST(FrustumCullingPass, initShutdownOnNullBackend) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::FrustumCullingPass pass;
	pass.init();
	EXPECT_EQ(pass.getMaxCommandCount(), 0u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(FrustumCullingPass, dispatchAllocatesBuffers) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::FrustumCullingPass pass;
	pass.init();
	std::vector<owl::renderer::utils::FrustumCullingPass::Aabb> aabbs;
	aabbs.resize(70);
	std::array<owl::math::vec4, 6> planes{};
	for (auto& p: planes) p = owl::math::vec4{0.0f, 0.0f, 1.0f, 1000.0f};
	const owl::renderer::utils::FrustumCullingPass::DrawCommand tmpl{.indexCount = 6,
																	 .instanceCount = 0,
																	 .firstIndex = 0,
																	 .baseVertex = 0,
																	 .baseInstance = 0};
	pass.dispatch(aabbs, planes, tmpl);
	EXPECT_NE(pass.getCommandBuffer(), nullptr);
	EXPECT_NE(pass.getCommandCountBuffer(), nullptr);
	// padded up to next multiple of 64 = 128.
	EXPECT_EQ(pass.getMaxCommandCount(), 128u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(FrustumCullingPass, extractFrustumPlanesIdentityYieldsClipSpaceBounds) {
	const auto planes = owl::renderer::utils::FrustumCullingPass::extractFrustumPlanes(owl::math::identity<float, 4>());
	// Identity VP — frustum planes are the clip-space cube faces, all unit
	// length after normalisation. Verify orientation by signs of the dominant
	// component.
	EXPECT_GT(planes[0].x(), 0.5f);// left:   +X
	EXPECT_LT(planes[1].x(), -0.5f);// right: -X
	EXPECT_GT(planes[2].y(), 0.5f);// bottom: +Y
	EXPECT_LT(planes[3].y(), -0.5f);// top:    -Y
	EXPECT_GT(planes[4].z(), 0.5f);// near:   +Z
	EXPECT_LT(planes[5].z(), -0.5f);// far:    -Z
}
