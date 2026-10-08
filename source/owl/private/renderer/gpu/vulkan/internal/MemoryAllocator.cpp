/**
 * @file MemoryAllocator.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#define VMA_IMPLEMENTATION
#include "MemoryAllocator.h"

#include "VulkanCore.h"
#include "utils.h"

#include <bit>
#include <string>

namespace owl::renderer::gpu::vulkan::internal {

namespace {
auto toAllocationInfo(const MemoryUsage iMemory) -> VmaAllocationCreateInfo {
	VmaAllocationCreateInfo info{};
	switch (iMemory) {
		case MemoryUsage::Device:
			info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			break;
		case MemoryUsage::Upload:
			info.usage = VMA_MEMORY_USAGE_AUTO;
			info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
		case MemoryUsage::Readback:
			info.usage = VMA_MEMORY_USAGE_AUTO;
			info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			break;
	}
	return info;
}
}// namespace

MemoryAllocator::~MemoryAllocator() = default;

auto MemoryAllocator::get() -> MemoryAllocator& {
	static MemoryAllocator allocator;
	return allocator;
}

auto MemoryAllocator::init(VkInstance iInstance, VkPhysicalDevice iPhysicalDevice, VkDevice iDevice) -> bool {
	release();
	const VmaAllocatorCreateInfo info{.flags = {},
									  .physicalDevice = iPhysicalDevice,
									  .device = iDevice,
									  .preferredLargeHeapBlockSize = 0,
									  .pAllocationCallbacks = nullptr,
									  .pDeviceMemoryCallbacks = nullptr,
									  .pHeapSizeLimit = nullptr,
									  .pVulkanFunctions = nullptr,
									  .instance = iInstance,
									  .vulkanApiVersion = VK_API_VERSION_1_3,
									  .pTypeExternalMemoryHandleTypes = nullptr};
	if (const VkResult result = vmaCreateAllocator(&info, &m_allocator); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create the memory allocator ({}).", resultString(result))
		m_allocator = nullptr;
		return false;
	}
	return true;
}

void MemoryAllocator::release() {
	if (m_allocator == nullptr)
		return;
	vmaDestroyAllocator(m_allocator);
	m_allocator = nullptr;
}

auto MemoryAllocator::createBuffer(const VkDeviceSize iSize, const VkBufferUsageFlags iUsage, const MemoryUsage iMemory,
								   const std::string_view iName) const -> AllocatedBuffer {
	AllocatedBuffer out;
	if (m_allocator == nullptr || iSize == 0) {
		OWL_CORE_ERROR("Vulkan: cannot create buffer {} (allocator not ready or empty size).", iName)
		return out;
	}
	const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
										.pNext = nullptr,
										.flags = {},
										.size = iSize,
										.usage = iUsage,
										.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
										.queueFamilyIndexCount = 0,
										.pQueueFamilyIndices = nullptr};
	const VmaAllocationCreateInfo allocInfo = toAllocationInfo(iMemory);
	VmaAllocationInfo result{};
	if (const VkResult res =
				vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &out.buffer, &out.allocation, &result);
		res != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create buffer {} of {} bytes ({}).", iName, iSize, resultString(res))
		return {};
	}
	out.mapped = result.pMappedData;
	out.size = iSize;
	if (!iName.empty()) {
		vmaSetAllocationName(m_allocator, out.allocation, std::string{iName}.c_str());
		VulkanCore::get().setObjectName(VK_OBJECT_TYPE_BUFFER, std::bit_cast<uint64_t>(out.buffer), iName);
	}
	return out;
}

void MemoryAllocator::destroyBuffer(AllocatedBuffer& ioBuffer) const {
	if (m_allocator != nullptr && ioBuffer.buffer != nullptr)
		vmaDestroyBuffer(m_allocator, ioBuffer.buffer, ioBuffer.allocation);
	ioBuffer = {};
}

auto MemoryAllocator::createImage(const VkImageCreateInfo& iInfo, const std::string_view iName) const
		-> AllocatedImage {
	AllocatedImage out;
	if (m_allocator == nullptr) {
		OWL_CORE_ERROR("Vulkan: cannot create image {} (allocator not ready).", iName)
		return out;
	}
	const VmaAllocationCreateInfo allocInfo = toAllocationInfo(MemoryUsage::Device);
	if (const VkResult res = vmaCreateImage(m_allocator, &iInfo, &allocInfo, &out.image, &out.allocation, nullptr);
		res != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create image {} ({}).", iName, resultString(res))
		return {};
	}
	if (!iName.empty()) {
		vmaSetAllocationName(m_allocator, out.allocation, std::string{iName}.c_str());
		VulkanCore::get().setObjectName(VK_OBJECT_TYPE_IMAGE, std::bit_cast<uint64_t>(out.image), iName);
	}
	return out;
}

void MemoryAllocator::destroyImage(AllocatedImage& ioImage) const {
	if (m_allocator != nullptr && ioImage.image != nullptr)
		vmaDestroyImage(m_allocator, ioImage.image, ioImage.allocation);
	ioImage = {};
}

auto MemoryAllocator::getBlockCount() const -> uint32_t {
	if (m_allocator == nullptr)
		return 0;
	VmaTotalStatistics stats{};
	vmaCalculateStatistics(m_allocator, &stats);
	return stats.total.statistics.blockCount;
}

}// namespace owl::renderer::gpu::vulkan::internal
