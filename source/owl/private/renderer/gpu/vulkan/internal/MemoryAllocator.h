/**
 * @file MemoryAllocator.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/vma.h"

#include <cstdint>
#include <string_view>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  Where an allocation lives and how the CPU reaches it.
 */
enum struct MemoryUsage : uint8_t {
	/// Device-local, never mapped (vertex, index and image data uploaded once).
	Device,
	/// Host-visible and coherent, persistently mapped, written sequentially by the CPU (staging, streaming).
	Upload,
	/// Host-visible and coherent, persistently mapped, read and written at random by the CPU (read-back).
	Readback,
};

/**
 * @brief
 *  A buffer and the VMA sub-allocation behind it.
 */
struct AllocatedBuffer {
	/// Buffer handle.
	VkBuffer buffer = nullptr;
	/// VMA allocation backing the buffer.
	VmaAllocation allocation = nullptr;
	/// Persistent host mapping (`Upload` and `Readback` only).
	void* mapped = nullptr;
	/// Size in bytes.
	VkDeviceSize size = 0;
};

/**
 * @brief
 *  An image and the VMA sub-allocation behind it.
 */
struct AllocatedImage {
	/// Image handle.
	VkImage image = nullptr;
	/// VMA allocation backing the image.
	VmaAllocation allocation = nullptr;
};

/**
 * @brief
 *  Owner of the Vulkan Memory Allocator: every buffer and image of the backend is a sub-allocation of it.
 */
class MemoryAllocator final {
public:
	MemoryAllocator(const MemoryAllocator&) = delete;
	MemoryAllocator(MemoryAllocator&&) = delete;
	auto operator=(const MemoryAllocator&) -> MemoryAllocator& = delete;
	auto operator=(MemoryAllocator&&) -> MemoryAllocator& = delete;

	~MemoryAllocator();

	/**
	 * @brief
	 *  Singleton accessor.
	 * @return The allocator.
	 */
	static auto get() -> MemoryAllocator& {
		static MemoryAllocator allocator;
		return allocator;
	}

	/**
	 * @brief
	 *  Create the VMA allocator on the current logical device.
	 * @param[in] iInstance Vulkan instance.
	 * @param[in] iPhysicalDevice Physical device.
	 * @param[in] iDevice Logical device.
	 * @return True on success.
	 */
	auto init(VkInstance iInstance, VkPhysicalDevice iPhysicalDevice, VkDevice iDevice) -> bool;

	/**
	 * @brief
	 *  Destroy the VMA allocator (every allocation must be freed before).
	 */
	void release();

	/**
	 * @brief
	 *  Check the allocator exists.
	 * @return True once initialised.
	 */
	[[nodiscard]] auto isReady() const -> bool { return m_allocator != nullptr; }

	/**
	 * @brief
	 *  Create a buffer in a sub-allocation.
	 * @param[in] iSize Size in bytes.
	 * @param[in] iUsage Buffer usage flags.
	 * @param[in] iMemory Memory placement.
	 * @param[in] iName Debug name given to the buffer.
	 * @return The buffer, empty on failure (logged).
	 */
	[[nodiscard]] auto createBuffer(VkDeviceSize iSize, VkBufferUsageFlags iUsage, MemoryUsage iMemory,
									std::string_view iName) const -> AllocatedBuffer;

	/**
	 * @brief
	 *  Destroy a buffer and free its allocation now (the GPU must not use it any more).
	 * @param[in,out] ioBuffer The buffer, reset to empty.
	 */
	void destroyBuffer(AllocatedBuffer& ioBuffer) const;

	/**
	 * @brief
	 *  Create a device-local image in a sub-allocation.
	 * @param[in] iInfo Image description.
	 * @param[in] iName Debug name given to the image.
	 * @return The image, empty on failure (logged).
	 */
	[[nodiscard]] auto createImage(const VkImageCreateInfo& iInfo, std::string_view iName) const -> AllocatedImage;

	/**
	 * @brief
	 *  Destroy an image and free its allocation now (the GPU must not use it any more).
	 * @param[in,out] ioImage The image, reset to empty.
	 */
	void destroyImage(AllocatedImage& ioImage) const;

	/**
	 * @brief
	 *  Number of `vkAllocateMemory` blocks VMA holds.
	 * @return The block count.
	 */
	[[nodiscard]] auto getBlockCount() const -> uint32_t;

private:
	MemoryAllocator() = default;

	/// The VMA allocator.
	VmaAllocator m_allocator = nullptr;
};

}// namespace owl::renderer::gpu::vulkan::internal
