/**
 * @file DescriptorRing.h
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "VulkanCore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  Descriptor sets of one layout, allocated from pools owned by each frame in flight.
 *
 *  Each draw acquires a distinct set, so a set referenced by a recorded command is never rewritten — the root cause
 *  of the `UPDATE_AFTER_BIND` validation cascade (a single shared set rewritten between draws). The pools of a frame
 *  slot are reset in one call (`vkResetDescriptorPool`) the first time the slot is used by a new frame, once the GPU
 *  is done with the frame that filled them; they grow to the high-water mark of a frame and are never freed one set at
 *  a time.
 */
class DescriptorRing final {
public:
	DescriptorRing() = default;

	~DescriptorRing() = default;

	DescriptorRing(const DescriptorRing&) = delete;

	DescriptorRing(DescriptorRing&&) = delete;

	auto operator=(const DescriptorRing&) -> DescriptorRing& = delete;

	auto operator=(DescriptorRing&&) -> DescriptorRing& = delete;

	/**
	 * @brief
	 *  Configure the ring for a given layout and per-set descriptor budget.
	 * @param[in] iLayout The descriptor set layout every acquired set uses (owned by the caller).
	 * @param[in] iPerSetSizes Descriptor counts consumed by a single set (scaled per pool block).
	 */
	void init(VkDescriptorSetLayout iLayout, std::vector<VkDescriptorPoolSize> iPerSetSizes);

	/**
	 * @brief
	 *  Destroy every owned pool (the device must still be valid).
	 */
	void release();

	/**
	 * @brief
	 *  Drop every handle without any Vulkan call (for teardown when the device is already gone).
	 */
	void reset();

	/**
	 * @brief
	 *  Get a fresh set for the current draw of a frame; it becomes the current set.
	 * @param[in] iSlot Frame slot recording the draw.
	 * @param[in] iSerial Serial of the frame recording the draw (a new serial resets the slot's pools).
	 * @return The acquired descriptor set, or null when uninitialised / allocation failed.
	 */
	auto acquire(uint32_t iSlot, uint64_t iSerial) -> VkDescriptorSet;

	/**
	 * @brief
	 *  Stable pointer to the most recently acquired set — the bind target for the current draw.
	 * @return Pointer to the current set handle (the handle is null until the first acquire).
	 */
	auto currentPtr() -> VkDescriptorSet* { return &m_current; }

private:
	/// Pools of one frame slot.
	struct Slot {
		std::vector<VkDescriptorPool> pools;///< Owned pools, grown on demand.
		size_t active = 0;///< Pool being allocated from.
		uint32_t setsInActive = 0;///< Sets already taken from the active pool.
		uint64_t serial = 0;///< Frame serial the pools were last reset for.
	};

	/**
	 * @brief
	 *  Create one pool block.
	 * @return The pool, or null on failure (logged).
	 */
	[[nodiscard]] auto createPool() const -> VkDescriptorPool;

	/// The shared layout for every set in the ring (not owned).
	VkDescriptorSetLayout m_layout = nullptr;
	/// Per-set descriptor budget used to size each pool block.
	std::vector<VkDescriptorPoolSize> m_perSetSizes;
	/// Pools of every frame slot.
	std::array<Slot, g_maxFrameInFlight> m_slots{};
	/// Set returned by the last acquire (the one to bind).
	VkDescriptorSet m_current = nullptr;
};

}// namespace owl::renderer::gpu::vulkan::internal
