/**
 * @file DescriptorRing.h
 * @author Silmaen
 * @date 26/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  A fence-recycled pool of descriptor sets sharing one layout.
 *
 *  Each draw acquires a distinct set, so a set bound to an in-flight command
 *  buffer is never rewritten — the root cause of the `UPDATE_AFTER_BIND`
 *  validation cascade (a single shared per-frame set rewritten between draws).
 *  On batch submit every set acquired since the previous submit is tagged with
 *  that batch's fence; a set is reused only once its fence has signalled. Sets
 *  are never freed individually — the ring grows to the high-water mark and
 *  whole pools are released together. Recycling keys on the submit fence (not
 *  the frame index) so sets stay correct even when several framebuffers submit
 *  independently within one displayed frame.
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
	 *  Get an idle set for the current draw; it becomes the current set.
	 * @return The acquired descriptor set, or null when uninitialised / allocation failed.
	 */
	auto acquire() -> VkDescriptorSet;

	/**
	 * @brief
	 *  Stable pointer to the most recently acquired set — the bind target for the current draw.
	 * @return Pointer to the current set handle (the handle is null until the first acquire).
	 */
	auto currentPtr() -> VkDescriptorSet* { return &m_current; }

	/**
	 * @brief
	 *  Tag every set acquired since the previous submit with the batch fence.
	 * @param[in] iFence The fence the just-submitted command buffer signals.
	 */
	void onSubmit(VkFence iFence);

private:
	/**
	 * @brief
	 *  One pooled descriptor set plus the fence of its last submission.
	 */
	struct Entry {
		VkDescriptorSet set = nullptr;///< The descriptor set handle.
		VkFence fence = nullptr;///< Fence of the batch last using it (meaningful only while busy).
		bool busy = false;///< True while claimed this batch or in flight; false when reusable.
	};

	/**
	 * @brief
	 *  Allocate a fresh idle set, creating a new pool block when the current one is full.
	 * @return Index of the new entry in m_entries, or SIZE_MAX on failure.
	 */
	auto allocate() -> size_t;

	/// The shared layout for every set in the ring (not owned).
	VkDescriptorSetLayout m_layout = nullptr;
	/// Per-set descriptor budget used to size each pool block.
	std::vector<VkDescriptorPoolSize> m_perSetSizes;
	/// Owned descriptor pools, grown on demand.
	std::vector<VkDescriptorPool> m_pools;
	/// Sets already allocated from the last pool block.
	uint32_t m_setsInCurrentPool = 0;
	/// Every allocated set with its in-flight fence tag.
	std::vector<Entry> m_entries;
	/// Entry indices acquired since the last onSubmit (tagged together on submit).
	std::vector<size_t> m_pendingThisBatch;
	/// Set returned by the last acquire (the one to bind).
	VkDescriptorSet m_current = nullptr;
};

}// namespace owl::renderer::gpu::vulkan::internal
