/**
 * @file StorageBuffer.cpp
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "StorageBuffer.h"

#include "internal/FrameProfiler.h"
#include "internal/RendererDescriptors.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"

#include <cstring>

namespace owl::renderer::gpu::vulkan {

StorageBuffer::StorageBuffer(const uint32_t iSize, const uint32_t iBinding) : m_size{iSize}, m_binding{iBinding} {
	if (iSize == 0)
		return;
	if (internal::VulkanHandler::get().getState() != internal::VulkanHandler::State::Running) {
		OWL_CORE_WARN("Vulkan storage buffer: Trying to create SSBO before VulkanHandler is running.")
		return;
	}
	m_buffer = internal::createBuffer(iSize,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
											  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
									  internal::MemoryUsage::Readback, "storageBuffer");
}

StorageBuffer::~StorageBuffer() {
	if (internal::VulkanHandler::get().getState() == internal::VulkanHandler::State::Running) {
		if (m_buffer.buffer != nullptr)
			internal::RendererDescriptors::unbindStorageBuffer(m_buffer.buffer);
		internal::freeBuffer(m_buffer);
	}
	m_buffer = {};
}

void StorageBuffer::setData(const void* iData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_buffer.buffer == nullptr || iSize == 0 || iData == nullptr)
		return;
	if (iOffset + iSize > m_size) {
		OWL_CORE_WARN("Vulkan storage buffer: setData out of range (offset {} + size {} > capacity {}).", iOffset,
					  iSize, m_size)
		return;
	}
	internal::writeMapped(m_buffer, iData, iSize, iOffset);
}

void StorageBuffer::getData(void* oData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_buffer.mapped == nullptr || iSize == 0 || oData == nullptr)
		return;
	if (iOffset + iSize > m_size) {
		OWL_CORE_WARN("Vulkan storage buffer: getData out of range (offset {} + size {} > capacity {}).", iOffset,
					  iSize, m_size)
		return;
	}
	const auto& vkc = internal::VulkanCore::get();
	internal::FrameProfiler::get().deviceWaitIdle(vkc.getLogicalDevice());

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(oData, static_cast<const uint8_t*>(m_buffer.mapped) + iOffset, iSize);
	OWL_DIAG_POP
}

void StorageBuffer::bind() {
	if (m_buffer.buffer == nullptr)
		return;
	auto* const active = internal::RendererDescriptors::getActive();
	if (active == nullptr)
		return;
	active->bindStorageBuffer(m_binding, m_buffer.buffer, static_cast<VkDeviceSize>(m_size));
}

void StorageBuffer::bind(const uint32_t iBinding) {
	if (m_buffer.buffer == nullptr)
		return;
	auto* const active = internal::RendererDescriptors::getActive();
	if (active == nullptr)
		return;
	active->bindStorageBuffer(iBinding, m_buffer.buffer, static_cast<VkDeviceSize>(m_size));
}

}// namespace owl::renderer::gpu::vulkan
