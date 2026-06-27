/**
 * @file DescriptorRing.cpp
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "DescriptorRing.h"

#include "VulkanCore.h"
#include "utils.h"

#include <limits>

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
	for (auto* const pool: m_pools) {
		if (pool != nullptr)
			vkDestroyDescriptorPool(core.getLogicalDevice(), pool, nullptr);
	}
	reset();
}

void DescriptorRing::reset() {
	m_pools.clear();
	m_entries.clear();
	m_pendingThisBatch.clear();
	m_setsInCurrentPool = 0;
	m_current = nullptr;
}

auto DescriptorRing::allocate() -> size_t {
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();
	if (m_pools.empty() || m_setsInCurrentPool >= k_setsPerPool) {
		std::vector<VkDescriptorPoolSize> sizes = m_perSetSizes;
		for (auto& size: sizes) size.descriptorCount *= k_setsPerPool;
		const VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = {},
												  .maxSets = k_setsPerPool,
												  .poolSizeCount = static_cast<uint32_t>(sizes.size()),
												  .pPoolSizes = sizes.data()};
		VkDescriptorPool pool = nullptr;
		if (const auto result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool); result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan DescriptorRing: failed to create descriptor pool ({}).", resultString(result))
			return std::numeric_limits<size_t>::max();
		}
		m_pools.push_back(pool);
		m_setsInCurrentPool = 0;
	}
	VkDescriptorSet set = nullptr;
	const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												.pNext = nullptr,
												.descriptorPool = m_pools.back(),
												.descriptorSetCount = 1,
												.pSetLayouts = &m_layout};
	if (const auto result = vkAllocateDescriptorSets(device, &allocInfo, &set); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan DescriptorRing: failed to allocate descriptor set ({}).", resultString(result))
		return std::numeric_limits<size_t>::max();
	}
	++m_setsInCurrentPool;
	m_entries.push_back({.set = set, .fence = nullptr, .busy = false});
	return m_entries.size() - 1;
}

auto DescriptorRing::acquire() -> VkDescriptorSet {
	if (m_layout == nullptr)
		return nullptr;
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();
	size_t index = std::numeric_limits<size_t>::max();
	for (size_t i = 0; i < m_entries.size(); ++i) {
		auto& entry = m_entries[i];
		if (entry.busy) {
			if (entry.fence == nullptr)
				continue;
			if (vkGetFenceStatus(device, entry.fence) != VK_SUCCESS)
				continue;
		}
		index = i;
		break;
	}
	if (index == std::numeric_limits<size_t>::max())
		index = allocate();
	if (index == std::numeric_limits<size_t>::max())
		return nullptr;
	auto& entry = m_entries[index];
	entry.busy = true;
	entry.fence = nullptr;
	m_pendingThisBatch.push_back(index);
	m_current = entry.set;
	return entry.set;
}

void DescriptorRing::onSubmit(VkFence iFence) {
	for (const auto index: m_pendingThisBatch) {
		if (index < m_entries.size())
			m_entries[index].fence = iFence;
	}
	m_pendingThisBatch.clear();
}

}// namespace owl::renderer::gpu::vulkan::internal
