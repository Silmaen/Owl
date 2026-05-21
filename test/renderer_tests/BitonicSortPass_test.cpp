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

TEST(BitonicSortPass, ssboMirrorsUploadedItemsOnNullBackend) {
	owl::core::Log::init(owl::core::Log::Level::Off);
	owl::renderer::gpu::RenderCommand::create(owl::renderer::gpu::RenderAPI::Type::Null);
	owl::renderer::utils::BitonicSortPass pass;
	pass.init();
	std::vector<owl::renderer::utils::BitonicSortPass::Item> items;
	for (uint32_t i = 0; i < 8; ++i) items.push_back({.key = static_cast<float>(8 - i), .value = i});
	pass.sort(items);

	// The Null backend's StorageBuffer is a host-side mirror — it stores
	// whatever `setData` last uploaded. The sort dispatch is a no-op on
	// Null, so the readback returns the uploaded (unsorted) bytes verbatim.
	// This proves the readback API contract works end-to-end (upload →
	// dispatch → readback) on the headless backend and gives the
	// real-GPU integration tests a known-good oracle to compare against.
	std::vector<owl::renderer::utils::BitonicSortPass::Item> roundtrip(items.size());
	pass.getBuffer()->getData(
			roundtrip.data(),
			static_cast<std::uint32_t>(roundtrip.size() * sizeof(owl::renderer::utils::BitonicSortPass::Item)), 0);
	for (std::size_t i = 0; i < items.size(); ++i) {
		EXPECT_FLOAT_EQ(roundtrip[i].key, items[i].key);
		EXPECT_EQ(roundtrip[i].value, items[i].value);
	}
	pass.shutdown();
	owl::renderer::gpu::RenderCommand::invalidate();
	owl::core::Log::invalidate();
}
