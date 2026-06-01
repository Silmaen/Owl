/**
 * @file StorageBuffer.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/StorageBuffer.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan {

/**
 * @brief
 *  Vulkan-backed Shader Storage Buffer Object — owns a `VkBuffer` with
 *  `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` and the descriptor wiring for the
 *  binding slot it was created with. Self-contained (does not go through
 *  the central `Descriptors` singleton) so each compute pass / instanced
 *  drawer can manage its own SSBOs without colliding with `Renderer2D`'s
 *  shared descriptor layout.
 */
class StorageBuffer final : public renderer::gpu::StorageBuffer {
public:
	StorageBuffer(const StorageBuffer&) = delete;

	StorageBuffer(StorageBuffer&&) = delete;

	auto operator=(const StorageBuffer&) -> StorageBuffer& = delete;

	auto operator=(StorageBuffer&&) -> StorageBuffer& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iSize Buffer size in bytes.
	 * @param[in] iBinding Shader binding slot.
	 */
	StorageBuffer(uint32_t iSize, uint32_t iBinding);

	/**
	 * @brief
	 *  Destructor.
	 */
	~StorageBuffer() override;

	/**
	 * @brief
	 *  Upload data from the CPU into the SSBO.
	 * @param[in] iData Source bytes.
	 * @param[in] iSize Byte count.
	 * @param[in] iOffset Destination offset in bytes.
	 */
	void setData(const void* iData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Read bytes back from the SSBO into a host buffer. Submits the current
	 *  command buffer and waits on it before mapping — the call is therefore
	 *  expensive (full GPU stall) and intended for test-time readback and
	 *  editor diagnostics only, not for the hot path.
	 * @param[out] oData Destination host buffer.
	 * @param[in] iSize Byte count to read.
	 * @param[in] iOffset Source offset in bytes inside the SSBO.
	 */
	void getData(void* oData, uint32_t iSize, uint32_t iOffset) override;

	/**
	 * @brief
	 *  Bind the SSBO. On Vulkan, descriptor sets are tied to the pipeline —
	 *  the actual binding happens when the pipeline is bound. This call
	 *  refreshes the descriptor write to point at the latest buffer state.
	 */
	void bind() override;

	/**
	 * @brief
	 *  Bind the SSBO at an explicit slot in the currently active descriptor
	 *  block. Used when the same `VkBuffer` participates in multiple renderers
	 *  at different slot indices.
	 * @param[in] iBinding Shader binding slot.
	 */
	void bind(uint32_t iBinding) override;

	/**
	 * @brief
	 *  Get the underlying VkBuffer handle (used by compute pipeline setup).
	 * @return The Vulkan buffer handle.
	 */
	[[nodiscard]] auto getHandle() const -> VkBuffer { return m_buffer; }

	/**
	 * @brief
	 *  Get the buffer size in bytes.
	 * @return Buffer size.
	 */
	[[nodiscard]] auto getSize() const -> uint32_t { return m_size; }

	/**
	 * @brief
	 *  Get the binding slot this SSBO was created for.
	 * @return Binding slot.
	 */
	[[nodiscard]] auto getBinding() const -> uint32_t { return m_binding; }

private:
	/// Underlying VkBuffer handle.
	VkBuffer m_buffer{nullptr};
	/// Backing device memory.
	VkDeviceMemory m_memory{nullptr};
	/// Buffer size in bytes.
	uint32_t m_size = 0;
	/// Shader binding slot.
	uint32_t m_binding = 0;
};

}// namespace owl::renderer::gpu::vulkan
