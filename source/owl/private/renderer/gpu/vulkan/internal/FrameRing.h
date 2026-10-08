/**
 * @file FrameRing.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "MemoryAllocator.h"
#include "VulkanCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  A region of a frame ring block, valid until the frame that allocated it is complete on the GPU.
 */
struct RingSlice {
	/// Buffer holding the region.
	VkBuffer buffer = nullptr;
	/// Offset of the region in the buffer.
	VkDeviceSize offset = 0;
	/// Size of the region in bytes.
	VkDeviceSize size = 0;
	/// Host address of the region (the blocks stay mapped).
	void* data = nullptr;
};

/**
 * @brief
 *  Per-frame linear allocator of host-visible memory: uniforms, streamed storage buffers and staging data.
 *
 * Each frame in flight owns its blocks; they are rewound when the frame slot is reused, that is once the GPU is done
 * with the frame that filled them. A frame that overflows its blocks gets one more, and the blocks of that slot are
 * merged into a single larger one at its next reuse.
 */
class FrameRing final {
public:
	FrameRing() = default;
	FrameRing(const FrameRing&) = delete;
	FrameRing(FrameRing&&) = delete;
	auto operator=(const FrameRing&) -> FrameRing& = delete;
	auto operator=(FrameRing&&) -> FrameRing& = delete;
	~FrameRing() = default;

	/**
	 * @brief
	 *  Rewind the blocks of a frame slot; its previous frame must be complete on the GPU.
	 * @param[in] iSlot Frame slot.
	 */
	void beginFrame(uint32_t iSlot);

	/**
	 * @brief
	 *  Allocate a region in the blocks of the current frame slot.
	 * @param[in] iSize Size in bytes.
	 * @param[in] iAlignment Alignment of the region offset (power of two).
	 * @return The region, empty on failure (logged).
	 */
	[[nodiscard]] auto allocate(VkDeviceSize iSize, VkDeviceSize iAlignment) -> RingSlice;

	/**
	 * @brief
	 *  Destroy every block (the device must be idle).
	 */
	void release();

	/**
	 * @brief
	 *  Bytes allocated in the current frame slot since its last rewind.
	 * @return The byte count.
	 */
	[[nodiscard]] auto getUsedBytes() const -> VkDeviceSize;

private:
	/// One mapped buffer of a slot.
	struct Block {
		AllocatedBuffer buffer;///< The mapped buffer.
		VkDeviceSize used = 0;///< Bytes allocated in it this frame.
	};
	/// Blocks of one frame slot.
	struct Slot {
		std::vector<Block> blocks;///< Blocks, filled in order.
		size_t active = 0;///< Index of the block being filled.
	};
	/// Size of a slot's first block.
	static constexpr VkDeviceSize g_initialBlockSize = VkDeviceSize{4} << 20u;
	/// Blocks of every frame slot.
	std::array<Slot, g_maxFrameInFlight> m_slots{};
	/// Slot being filled.
	uint32_t m_slot = 0;

	[[nodiscard]] static auto createBlock(VkDeviceSize iSize) -> Block;
};

}// namespace owl::renderer::gpu::vulkan::internal
