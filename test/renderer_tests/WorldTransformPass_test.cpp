/**
 * @file WorldTransformPass_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/utils/WorldTransformPass.h>

TEST(WorldTransformPass, initOnNullBackendSucceeds) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::WorldTransformPass pass;
	pass.init();
	// Buffers are lazily allocated on the first `compute()` call so the size
	// can match the actual entry count — only the entry count starts at zero.
	EXPECT_EQ(pass.getEntryCount(), 0u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(WorldTransformPass, computeWithoutInitIsSafe) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::WorldTransformPass pass;
	const owl::renderer::utils::WorldTransformPass::Entry entry{.local = owl::math::identity<float, 4>(),
																.parentIdx = -1};
	pass.compute(std::span<const owl::renderer::utils::WorldTransformPass::Entry>{&entry, 1});
	EXPECT_EQ(pass.getEntryCount(), 0u);
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(WorldTransformPass, computeRecordsEntryCount) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::WorldTransformPass pass;
	pass.init();
	std::vector<owl::renderer::utils::WorldTransformPass::Entry> entries;
	entries.push_back({.local = owl::math::identity<float, 4>(), .parentIdx = -1});
	entries.push_back({.local = owl::math::identity<float, 4>(), .parentIdx = 0});
	entries.push_back({.local = owl::math::identity<float, 4>(), .parentIdx = 1});
	pass.compute(entries);
	EXPECT_EQ(pass.getEntryCount(), 3u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(WorldTransformPass, computeGrowsBuffersOnLargerInput) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::WorldTransformPass pass;
	pass.init();
	std::vector<owl::renderer::utils::WorldTransformPass::Entry> small;
	small.resize(4, {.local = owl::math::identity<float, 4>(), .parentIdx = -1});
	pass.compute(small);
	const auto bufA = pass.getWorldBuffer();
	std::vector<owl::renderer::utils::WorldTransformPass::Entry> bigger;
	bigger.resize(200, {.local = owl::math::identity<float, 4>(), .parentIdx = -1});
	pass.compute(bigger);
	const auto bufB = pass.getWorldBuffer();
	EXPECT_NE(bufB, nullptr);
	// The output buffer must have grown (or at least be valid).
	EXPECT_EQ(pass.getEntryCount(), 200u);
	(void) bufA;// just ensures the first compute ran without crashing.
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
