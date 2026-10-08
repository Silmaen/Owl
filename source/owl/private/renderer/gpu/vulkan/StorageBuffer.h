/**
 * @file StorageBuffer.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "internal/FrameRing.h"
#include "internal/MemoryAllocator.h"
#include "renderer/gpu/StorageBuffer.h"
#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace owl::renderer::gpu::vulkan {

/**
 * @brief
 *  Vulkan-backed Shader Storage Buffer Object.
 *
 *  A buffer the CPU never writes (compute output) lives in one persistent, mapped `VkBuffer`. Once `setData` is
 *  called, the buffer is *streamed*: the CPU content is kept in a shadow copy and every frame that reads it gets its
 *  own copy in the frame ring, made at the first use after each write. Several writes in one frame thus give each
 *  draw the content it was recorded with, and no write ever touches memory a frame in flight still reads.
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
	 *  Region of a buffer the GPU reads in the frame being recorded.
	 */
	struct View {
		VkBuffer buffer = nullptr;///< Buffer handle.
		VkDeviceSize offset = 0;///< Offset of the region.
		VkDeviceSize range = 0;///< Size of the region.
		uint64_t version = 0;///< Changes whenever the region changes.
	};

	/**
	 * @brief
	 *  Get the region holding the buffer content for the frame being recorded, copying the CPU content into the
	 *  frame ring when it changed or was not copied in this frame yet.
	 * @return The region; empty when the buffer does not exist.
	 */
	[[nodiscard]] auto resolve() -> View;

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
	/// Persistently mapped buffer (GPU-written content, or streamed content outside a frame).
	internal::AllocatedBuffer m_buffer;
	/// CPU content of a streamed buffer.
	std::vector<uint8_t> m_shadow;
	/// Bytes of the shadow that hold data.
	uint32_t m_extent = 0;
	/// True once the CPU wrote the buffer.
	bool m_streamed = false;
	/// True when the shadow changed since the last copy.
	bool m_dirty = false;
	/// Frame-ring copy of the shadow.
	internal::RingSlice m_slice;
	/// Frame serial of `m_slice`.
	uint64_t m_sliceSerial = 0;
	/// Version of the region given by `resolve`.
	uint64_t m_version = 0;
	/// Buffer size in bytes.
	uint32_t m_size = 0;
	/// Shader binding slot.
	uint32_t m_binding = 0;
};

}// namespace owl::renderer::gpu::vulkan
