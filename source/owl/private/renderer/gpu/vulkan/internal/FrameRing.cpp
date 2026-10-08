/**
 * @file FrameRing.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "FrameRing.h"

#include <algorithm>

namespace owl::renderer::gpu::vulkan::internal {

namespace {
constexpr VkBufferUsageFlags g_ringUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
										   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
										   VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
										   VK_BUFFER_USAGE_TRANSFER_DST_BIT;

constexpr auto alignUp(const VkDeviceSize iValue, const VkDeviceSize iAlignment) -> VkDeviceSize {
	return (iValue + iAlignment - 1) & ~(iAlignment - 1);
}
}// namespace

auto FrameRing::createBlock(const VkDeviceSize iSize) -> Block {
	return {.buffer = MemoryAllocator::get().createBuffer(iSize, g_ringUsage, MemoryUsage::Upload, "frameRing"),
			.used = 0};
}

void FrameRing::beginFrame(const uint32_t iSlot) {
	m_slot = iSlot % g_maxFrameInFlight;
	auto& [blocks, active] = m_slots[m_slot];
	if (blocks.size() > 1) {
		VkDeviceSize total = 0;
		for (auto& block: blocks) {
			total += block.buffer.size;
			MemoryAllocator::get().destroyBuffer(block.buffer);
		}
		blocks.clear();
		blocks.push_back(createBlock(total));
	}
	for (auto& block: blocks) block.used = 0;
	active = 0;
}

auto FrameRing::allocate(const VkDeviceSize iSize, const VkDeviceSize iAlignment) -> RingSlice {
	if (iSize == 0)
		return {};
	auto& [blocks, active] = m_slots[m_slot];
	const VkDeviceSize alignment = std::max<VkDeviceSize>(iAlignment, 16);
	while (active < blocks.size()) {
		auto& block = blocks[active];
		if (const VkDeviceSize offset = alignUp(block.used, alignment);
			block.buffer.buffer != nullptr && offset + iSize <= block.buffer.size) {
			block.used = offset + iSize;
			return {.buffer = block.buffer.buffer,
					.offset = offset,
					.size = iSize,
					.data = static_cast<uint8_t*>(block.buffer.mapped) + offset};
		}
		++active;
	}
	const VkDeviceSize previous = blocks.empty() ? 0 : blocks.back().buffer.size;
	auto& block = blocks.emplace_back(createBlock(std::max({g_initialBlockSize, previous * 2, alignUp(iSize, 256)})));
	active = blocks.size() - 1;
	if (block.buffer.mapped == nullptr) {
		OWL_CORE_ERROR("Vulkan FrameRing: failed to grow the frame ring for {} bytes.", iSize)
		return {};
	}
	block.used = iSize;
	return {.buffer = block.buffer.buffer, .offset = 0, .size = iSize, .data = block.buffer.mapped};
}

void FrameRing::release() {
	for (auto& [blocks, active]: m_slots) {
		for (auto& block: blocks) MemoryAllocator::get().destroyBuffer(block.buffer);
		blocks.clear();
		active = 0;
	}
}

auto FrameRing::getUsedBytes() const -> VkDeviceSize {
	VkDeviceSize used = 0;
	for (const auto& block: m_slots[m_slot].blocks) used += block.used;
	return used;
}

}// namespace owl::renderer::gpu::vulkan::internal
