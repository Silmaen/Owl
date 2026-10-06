/**
 * @file GpuProfiler.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan::internal {
/**
 * @brief
 *  GPU zones of the Vulkan backend on the Tracy timeline (`OWL_PROFILER=tracy`), no-ops otherwise.
 *
 * One zone per batch (one command buffer submit), opened right after `vkBeginCommandBuffer` and closed before
 * `vkEndCommandBuffer`; the timestamp queries of the previous batches are collected at the start of each batch,
 * outside the render pass.
 */
class GpuProfiler final {
public:
	GpuProfiler() = delete;

	/**
	 * @brief
	 *  Create the Tracy Vulkan context once the device, the graphic queue and the command pool exist.
	 */
	static void init();

	/**
	 * @brief
	 *  Destroy the Tracy Vulkan context (its query pool) while the device is still valid.
	 */
	static void release();

	/**
	 * @brief
	 *  Collect the finished queries and open the batch GPU zone.
	 * @param[in] iCommandBuffer The batch command buffer, recording and outside any render pass.
	 */
	static void beginBatch(VkCommandBuffer iCommandBuffer);

	/**
	 * @brief
	 *  Close the batch GPU zone, outside the render pass and before the command buffer ends.
	 */
	static void endBatch();
};
}// namespace owl::renderer::gpu::vulkan::internal
