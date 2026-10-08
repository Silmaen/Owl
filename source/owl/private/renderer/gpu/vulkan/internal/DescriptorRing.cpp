/**
 * @file DescriptorRing.cpp
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DescriptorRing.h"

#include "utils.h"

namespace owl::renderer::gpu::vulkan::internal {

namespace {
constexpr uint32_t k_setsPerPool = 64;
}// namespace

void DescriptorRing::init(VkDescriptorSetLayout iLayout, std::vector<VkDescriptorPoolSize> iPerSetSizes) {
	release();
	m_layout = iLayout;
	m_perSetSizes = std::move(iPerSetSizes);
}

void DescriptorRing::release() {
	const auto& core = VulkanCore::get();
	for (const auto& slot: m_slots) {
		for (auto* const pool: slot.pools) {
			if (pool != nullptr)
				vkDestroyDescriptorPool(core.getLogicalDevice(), pool, nullptr);
		}
	}
	reset();
}

void DescriptorRing::reset() {
	m_slots = {};
	m_current = nullptr;
}

auto DescriptorRing::createPool() const -> VkDescriptorPool {
	std::vector<VkDescriptorPoolSize> sizes = m_perSetSizes;
	for (auto& size: sizes) size.descriptorCount *= k_setsPerPool;
	const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = {},
											  .maxSets = k_setsPerPool,
											  .poolSizeCount = static_cast<uint32_t>(sizes.size()),
											  .pPoolSizes = sizes.data()};
	VkDescriptorPool pool = nullptr;
	if (const auto result = vkCreateDescriptorPool(VulkanCore::get().getLogicalDevice(), &poolInfo, nullptr, &pool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan DescriptorRing: failed to create descriptor pool ({}).", resultString(result))
		return nullptr;
	}
	return pool;
}

auto DescriptorRing::acquire(const uint32_t iSlot, const uint64_t iSerial) -> VkDescriptorSet {
	if (m_layout == nullptr)
		return nullptr;
	auto* const device = VulkanCore::get().getLogicalDevice();
	auto& slot = m_slots[iSlot % g_maxFrameInFlight];
	if (slot.serial != iSerial) {
		for (auto* const pool: slot.pools) vkResetDescriptorPool(device, pool, 0);
		slot.active = 0;
		slot.setsInActive = 0;
		slot.serial = iSerial;
	}
	if (slot.setsInActive >= k_setsPerPool) {
		++slot.active;
		slot.setsInActive = 0;
	}
	if (slot.active >= slot.pools.size()) {
		auto* const pool = createPool();
		if (pool == nullptr)
			return nullptr;
		slot.pools.push_back(pool);
		slot.active = slot.pools.size() - 1;
	}
	VkDescriptorSet set = nullptr;
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = slot.pools[slot.active],
												.descriptorSetCount = 1,
												.pSetLayouts = &m_layout};
	if (const auto result = vkAllocateDescriptorSets(device, &allocInfo, &set); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan DescriptorRing: failed to allocate descriptor set ({}).", resultString(result))
		return nullptr;
	}
	++slot.setsInActive;
	m_current = set;
	return set;
}

}// namespace owl::renderer::gpu::vulkan::internal
