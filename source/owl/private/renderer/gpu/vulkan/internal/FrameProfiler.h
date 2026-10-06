/**
 * @file FrameProfiler.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  GPU timestamps and synchronisation counters of the Vulkan backend.
 *
 * Every timed command buffer gets a begin and an end timestamp (`vkCmdWriteTimestamp`) in a ring of query slots,
 * one slot per frame. A slot is read back when the ring comes round to it again, four frames later, without waiting:
 * a query the GPU has not written yet is dropped from the frame. The counters (submissions, queue and device drains)
 * are always on; the timestamps only between `setEnabled(true)` and `setEnabled(false)`.
 */
class FrameProfiler final {
public:
	FrameProfiler(const FrameProfiler&) = delete;

	FrameProfiler(FrameProfiler&&) = delete;

	auto operator=(const FrameProfiler&) -> FrameProfiler& = delete;

	auto operator=(FrameProfiler&&) -> FrameProfiler& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~FrameProfiler() = default;

	/**
	 * @brief
	 *  Singleton access.
	 * @return The profiler.
	 */
	static auto get() -> FrameProfiler& {
		static FrameProfiler instance;
		return instance;
	}

	/**
	 * @brief
	 *  Check whether the graphics queue supports timestamps.
	 * @return True when the device can time command buffers.
	 */
	[[nodiscard]] auto isSupported() const -> bool;

	/**
	 * @brief
	 *  Start or stop the timestamps; the query pool is created on first use.
	 * @param[in] iEnabled True to time the next frames.
	 */
	void setEnabled(bool iEnabled);

	/**
	 * @brief
	 *  Check whether the timestamps are recorded.
	 * @return True while timing.
	 */
	[[nodiscard]] auto isEnabled() const -> bool { return m_enabled; }

	/**
	 * @brief
	 *  Open a new frame: read back the slot it reuses and start filling it.
	 */
	void onBeginFrame();

	/**
	 * @brief
	 *  Reset two queries and write the begin timestamp. Must be recorded outside a render pass.
	 * @param[in] iCommandBuffer The recording command buffer.
	 * @return The index of the begin query, or nothing when timing is off or the slot is full.
	 */
	auto writeBegin(VkCommandBuffer iCommandBuffer) -> std::optional<uint32_t>;

	/**
	 * @brief
	 *  Write the end timestamp matching a `writeBegin`.
	 * @param[in] iCommandBuffer The recording command buffer.
	 * @param[in] iBeginQuery The index returned by `writeBegin`.
	 */
	void writeEnd(VkCommandBuffer iCommandBuffer, uint32_t iBeginQuery) const;

	/**
	 * @brief
	 *  Time a one-shot command buffer (begin timestamp, remembered until `endOneShot`).
	 * @param[in] iCommandBuffer The one-shot command buffer, just begun.
	 */
	void beginOneShot(VkCommandBuffer iCommandBuffer);

	/**
	 * @brief
	 *  Close the timing of a one-shot command buffer opened by `beginOneShot`.
	 * @param[in] iCommandBuffer The one-shot command buffer, before `vkEndCommandBuffer`.
	 */
	void endOneShot(VkCommandBuffer iCommandBuffer);

	/**
	 * @brief
	 *  Hand over the frames read back since the last call.
	 * @return The completed timings, oldest first.
	 */
	auto popTimings() -> std::vector<GpuFrameTiming>;

	/**
	 * @brief
	 *  Get the number of the frame being recorded.
	 * @return The frame number, 0 when timing is off.
	 */
	[[nodiscard]] auto getFrameId() const -> uint64_t { return m_enabled ? m_frameId : 0; }

	/**
	 * @brief
	 *  Count one queue submission.
	 */
	void countSubmit() { ++m_counters.submits; }

	/**
	 * @brief
	 *  Wait for a queue to drain and count it.
	 * @param[in] iQueue The queue to wait for.
	 * @return The Vulkan result.
	 */
	auto queueWaitIdle(VkQueue iQueue) -> VkResult;

	/**
	 * @brief
	 *  Wait for the device to drain and count it.
	 * @param[in] iDevice The device to wait for.
	 * @return The Vulkan result.
	 */
	auto deviceWaitIdle(VkDevice iDevice) -> VkResult;

	/**
	 * @brief
	 *  Get the cumulative counters.
	 * @return The counters since start-up.
	 */
	[[nodiscard]] auto getCounters() const -> const RenderCounters& { return m_counters; }

	/**
	 * @brief
	 *  Destroy the query pool; must run before the logical device is destroyed.
	 */
	void release();

private:
	/**
	 * @brief
	 *  Default constructor.
	 */
	FrameProfiler() = default;

	/**
	 * @brief
	 *  Create the query pool and read the timestamp properties.
	 * @return True when the pool is usable.
	 */
	auto createPool() -> bool;

	/**
	 * @brief
	 *  Read back one slot and push its timing.
	 * @param[in] iSlot The slot index.
	 */
	void harvest(size_t iSlot);

	/// Frames kept in the query ring.
	static constexpr size_t g_slotCount = 4;
	/// Queries per frame slot (two per timed command buffer).
	static constexpr uint32_t g_queriesPerSlot = 512;

	/// Timestamp query pool (`g_slotCount * g_queriesPerSlot` queries).
	VkQueryPool m_pool{VK_NULL_HANDLE};
	/// True while timestamps are recorded.
	bool m_enabled{false};
	/// Nanoseconds per timestamp tick.
	double m_periodNs{1.0};
	/// Mask of the valid timestamp bits.
	uint64_t m_validMask{0};
	/// Number of the frame being recorded.
	uint64_t m_frameId{0};
	/// Slot of the frame being recorded.
	size_t m_slot{0};
	/// Frame number recorded in each slot (0 = empty).
	std::array<uint64_t, g_slotCount> m_slotFrame{};
	/// Queries used in each slot.
	std::array<uint32_t, g_slotCount> m_slotUsed{};
	/// Open one-shot command buffers and their begin query.
	std::vector<std::pair<VkCommandBuffer, uint32_t>> m_openOneShots;
	/// Frames read back and not handed over yet.
	std::vector<GpuFrameTiming> m_completed;
	/// Read-back buffer (value + availability per query).
	std::vector<uint64_t> m_readback;
	/// Cumulative counters.
	RenderCounters m_counters;
};

}// namespace owl::renderer::gpu::vulkan::internal
