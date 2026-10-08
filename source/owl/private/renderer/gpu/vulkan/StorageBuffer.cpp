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

#include <algorithm>
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
	internal::RendererDescriptors::unbindStorageBuffer(this);
	if (internal::VulkanHandler::get().getState() == internal::VulkanHandler::State::Running)
		internal::releaseBuffer(m_buffer);
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
	if (m_shadow.size() < m_size)
		m_shadow.resize(m_size);

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(m_shadow.data() + iOffset, iData, iSize);
	OWL_DIAG_POP

	// A write from the start defines the content; a write further in extends it.
	m_extent = iOffset == 0 ? iSize : std::max(m_extent, iOffset + iSize);
	m_streamed = true;
	m_dirty = true;
}

auto StorageBuffer::resolve() -> View {
	if (m_buffer.buffer == nullptr)
		return {};
	if (!m_streamed)
		return {.buffer = m_buffer.buffer, .offset = 0, .range = m_size, .version = 0};
	auto& vkh = internal::VulkanHandler::get();
	const VkDeviceSize range = std::max<VkDeviceSize>(m_extent, 16);
	if (!vkh.isRecording()) {
		if (m_dirty) {
			internal::writeMapped(m_buffer, m_shadow.data(), m_extent);
			m_dirty = false;
			++m_version;
		}
		return {.buffer = m_buffer.buffer,
				.offset = 0,
				.range = std::min<VkDeviceSize>(range, m_size),
				.version = m_version};
	}
	if (m_dirty || m_sliceSerial != vkh.getFrameSerial() || m_slice.buffer == nullptr) {
		m_slice = vkh.allocateTransient(range);
		if (m_slice.data == nullptr)
			return {};

		OWL_DIAG_PUSH
		OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
		memcpy(m_slice.data, m_shadow.data(), std::min<size_t>(m_extent, m_shadow.size()));
		OWL_DIAG_POP

		m_sliceSerial = vkh.getFrameSerial();
		m_dirty = false;
		++m_version;
	}
	return {.buffer = m_slice.buffer, .offset = m_slice.offset, .range = m_slice.size, .version = m_version};
}

void StorageBuffer::getData(void* oData, const uint32_t iSize, const uint32_t iOffset) {
	if (m_buffer.mapped == nullptr || iSize == 0 || oData == nullptr)
		return;
	if (iOffset + iSize > m_size) {
		OWL_CORE_WARN("Vulkan storage buffer: getData out of range (offset {} + size {} > capacity {}).", iOffset,
					  iSize, m_size)
		return;
	}
	auto& vkh = internal::VulkanHandler::get();
	if (vkh.isRecording()) {
		vkh.flushFrame();
	} else {
		const auto& vkc = internal::VulkanCore::get();
		internal::FrameProfiler::get().deviceWaitIdle(vkc.getLogicalDevice());
	}
	const auto* source = static_cast<const uint8_t*>(m_buffer.mapped);
	size_t available = m_size;
	if (m_streamed && m_sliceSerial == vkh.getFrameSerial() && m_slice.data != nullptr) {
		source = static_cast<const uint8_t*>(m_slice.data);
		available = m_slice.size;
	} else if (m_streamed) {
		source = m_shadow.data();
		available = m_shadow.size();
	}
	if (iOffset + iSize > available) {
		OWL_CORE_WARN("Vulkan storage buffer: getData beyond the streamed content ({} bytes).", available)
		return;
	}

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(oData, source + iOffset, iSize);
	OWL_DIAG_POP
}

void StorageBuffer::bind() { bind(m_binding); }

void StorageBuffer::bind(const uint32_t iBinding) {
	if (m_buffer.buffer == nullptr)
		return;
	auto* const active = internal::RendererDescriptors::getActive();
	if (active == nullptr)
		return;
	active->bindStorageBuffer(iBinding, this);
}

}// namespace owl::renderer::gpu::vulkan
