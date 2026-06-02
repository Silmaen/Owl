/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "StorageBuffer.h"

#include "internal/RendererDescriptors.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"

namespace owl::renderer::gpu::vulkan {

StorageBuffer::StorageBuffer(const uint32_t iSize, const uint32_t iBinding) : m_size{iSize}, m_binding{iBinding} {
	if (iSize == 0)
		return;
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan storage buffer: Trying to create SSBO before VulkanHandler is running.")
		return;
	}
	internal::createBuffer(iSize,
						   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
								   VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_buffer,
						   m_memory);
}

StorageBuffer::~StorageBuffer() {
	if (internal::VulkanHandler::get().getState() == internal::VulkanHandler::State::Running) {
		const auto& vkc = internal::VulkanCore::get();
		if (m_buffer != nullptr)
			vkDestroyBuffer(vkc.getLogicalDevice(), m_buffer, nullptr);
		if (m_memory != nullptr)
			vkFreeMemory(vkc.getLogicalDevice(), m_memory, nullptr);
	}
	m_buffer = nullptr;
	m_memory = nullptr;
}

void StorageBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_buffer == nullptr || iSize == 0 || iData == nullptr)
		return;
	if (iOffset + iSize > m_size) {
		OWL_CORE_WARN("Vulkan storage buffer: setData out of range (offset {} + size {} > capacity {}).", iOffset,
					  iSize, m_size)
		return;
	}
	const auto& vkc = internal::VulkanCore::get();
	void* mapped = nullptr;
	vkMapMemory(vkc.getLogicalDevice(), m_memory, iOffset, iSize, 0, &mapped);

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(mapped, iData, iSize);
	OWL_DIAG_POP

	vkUnmapMemory(vkc.getLogicalDevice(), m_memory);
}

void StorageBuffer::getData(void* oData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_buffer == nullptr || iSize == 0 || oData == nullptr)
		return;
	if (iOffset + iSize > m_size) {
		OWL_CORE_WARN("Vulkan storage buffer: getData out of range (offset {} + size {} > capacity {}).", iOffset,
					  iSize, m_size)
		return;
	}
	const auto& vkc = internal::VulkanCore::get();
	vkDeviceWaitIdle(vkc.getLogicalDevice());

	void* mapped = nullptr;
	vkMapMemory(vkc.getLogicalDevice(), m_memory, iOffset, iSize, 0, &mapped);

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(oData, mapped, iSize);
	OWL_DIAG_POP

	vkUnmapMemory(vkc.getLogicalDevice(), m_memory);
}

void StorageBuffer::bind() {
	if (m_buffer == nullptr)
		return;
	auto* const active = internal::RendererDescriptors::getActive();
	if (active == nullptr)
		return;
	active->bindStorageBuffer(m_binding, m_buffer, static_cast<VkDeviceSize>(m_size));
}

void StorageBuffer::bind(const uint32_t iBinding) {
	if (m_buffer == nullptr)
		return;
	auto* const active = internal::RendererDescriptors::getActive();
	if (active == nullptr)
		return;
	active->bindStorageBuffer(iBinding, m_buffer, static_cast<VkDeviceSize>(m_size));
}

}// namespace owl::renderer::gpu::vulkan
