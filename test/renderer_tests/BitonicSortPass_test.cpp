/**
 * @file BitonicSortPass_test.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/gpu/RenderCommand.h>
#include <renderer/utils/BitonicSortPass.h>

TEST(BitonicSortPass, initShutdownOnNullBackend) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::BitonicSortPass pass;
	pass.init();
	EXPECT_EQ(pass.getItemCount(), 0u);
	EXPECT_NE(pass.getBuffer(), nullptr);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(BitonicSortPass, sortWithoutInitIsSafe) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::BitonicSortPass pass;
	const owl::renderer::utils::BitonicSortPass::Item item{.key = 1.0f, .value = 0u};
	pass.sort(std::span<const owl::renderer::utils::BitonicSortPass::Item>{&item, 1});
	EXPECT_EQ(pass.getItemCount(), 0u);
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(BitonicSortPass, sortClampsAtCapacity) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::BitonicSortPass pass;
	pass.init();
	std::vector<owl::renderer::utils::BitonicSortPass::Item> items;
	items.resize(owl::renderer::utils::BitonicSortPass::kSortSize + 17,
				 owl::renderer::utils::BitonicSortPass::Item{.key = 1.0f, .value = 0u});
	pass.sort(items);
	EXPECT_EQ(pass.getItemCount(), owl::renderer::utils::BitonicSortPass::kSortSize);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}

TEST(BitonicSortPass, sortRecordsItemCount) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::BitonicSortPass pass;
	pass.init();
	std::vector<owl::renderer::utils::BitonicSortPass::Item> items;
	for (uint32_t i = 0; i < 42; ++i) items.push_back({.key = static_cast<float>(42 - i), .value = i});
	pass.sort(items);
	EXPECT_EQ(pass.getItemCount(), 42u);
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
