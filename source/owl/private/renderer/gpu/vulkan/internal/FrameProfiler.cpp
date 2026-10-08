/**
 * @file FrameProfiler.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "FrameProfiler.h"

#include "VulkanCore.h"
#include "utils.h"

#include <cstdint>
#include <limits>

namespace owl::renderer::gpu::vulkan::internal {

auto FrameProfiler::isSupported() const -> bool {
	const auto& core = VulkanCore::get();
	if (core.getLogicalDevice() == nullptr)
		return false;
	return core.getGraphicQueueTimestampBits() > 0 && core.getTimestampPeriod() > 0.f;
}

void FrameProfiler::setEnabled(const bool iEnabled) {
	if (iEnabled == m_enabled)
		return;
	if (!iEnabled) {
		m_enabled = false;
		return;
	}
	if (!isSupported()) {
		OWL_CORE_WARN("Vulkan: GPU timestamps not supported by the graphics queue.")
		return;
	}
	if (m_pool == VK_NULL_HANDLE && !createPool())
		return;
	m_slotFrame.fill(0);
	m_slotUsed.fill(0);
	m_openOneShots.clear();
	m_completed.clear();
	m_frameId = 0;
	m_slot = 0;
	m_enabled = true;
}

auto FrameProfiler::createPool() -> bool {
	const auto& core = VulkanCore::get();
	const VkQueryPoolCreateInfo info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
									 .pNext = nullptr,
									 .flags = {},
									 .queryType = VK_QUERY_TYPE_TIMESTAMP,
									 .queryCount = static_cast<uint32_t>(g_slotCount) * g_queriesPerSlot,
									 .pipelineStatistics = {}};
	if (const VkResult result = vkCreateQueryPool(core.getLogicalDevice(), &info, nullptr, &m_pool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create the timestamp query pool ({}).", resultString(result))
		m_pool = VK_NULL_HANDLE;
		return false;
	}
	core.setObjectName(VK_OBJECT_TYPE_QUERY_POOL, reinterpret_cast<uint64_t>(m_pool), "FrameProfiler timestamps");
	m_periodNs = static_cast<double>(core.getTimestampPeriod());
	const uint32_t bits = core.getGraphicQueueTimestampBits();
	m_validMask = bits >= 64 ? ~uint64_t{0} : (uint64_t{1} << bits) - 1;
	m_readback.resize(static_cast<size_t>(g_queriesPerSlot) * 2);
	return true;
}

void FrameProfiler::onBeginFrame() {
	if (!m_enabled)
		return;
	++m_frameId;
	m_slot = static_cast<size_t>(m_frameId % g_slotCount);
	if (m_slotFrame[m_slot] != 0)
		harvest(m_slot);
	m_slotFrame[m_slot] = m_frameId;
	m_slotUsed[m_slot] = 0;
}

auto FrameProfiler::writeBegin(VkCommandBuffer iCommandBuffer) -> std::optional<uint32_t> {
	if (!m_enabled || m_frameId == 0 || iCommandBuffer == nullptr)
		return std::nullopt;
	if (m_slotUsed[m_slot] + 2 > g_queriesPerSlot)
		return std::nullopt;
	const uint32_t query = static_cast<uint32_t>(m_slot) * g_queriesPerSlot + m_slotUsed[m_slot];
	m_slotUsed[m_slot] += 2;
	vkCmdResetQueryPool(iCommandBuffer, m_pool, query, 2);
	vkCmdWriteTimestamp(iCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_pool, query);
	return query;
}

void FrameProfiler::writeEnd(VkCommandBuffer iCommandBuffer, const uint32_t iBeginQuery) const {
	if (m_pool == VK_NULL_HANDLE || iCommandBuffer == nullptr)
		return;
	vkCmdWriteTimestamp(iCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_pool, iBeginQuery + 1);
}

void FrameProfiler::beginOneShot(VkCommandBuffer iCommandBuffer) {
	if (const auto query = writeBegin(iCommandBuffer); query.has_value())
		m_openOneShots.emplace_back(iCommandBuffer, *query);
}

void FrameProfiler::endOneShot(VkCommandBuffer iCommandBuffer) {
	const auto it = std::ranges::find_if(
			m_openOneShots, [iCommandBuffer](const auto& iEntry) -> bool { return iEntry.first == iCommandBuffer; });
	if (it == m_openOneShots.end())
		return;
	writeEnd(iCommandBuffer, it->second);
	m_openOneShots.erase(it);
}

void FrameProfiler::harvest(const size_t iSlot) {
	const uint32_t used = m_slotUsed[iSlot];
	GpuFrameTiming timing{.frameId = m_slotFrame[iSlot], .busyMs = 0.0, .spanMs = 0.0, .intervalCount = 0};
	if (used > 0) {
		const auto& core = VulkanCore::get();
		const uint32_t first = static_cast<uint32_t>(iSlot) * g_queriesPerSlot;
		const VkResult result = vkGetQueryPoolResults(
				core.getLogicalDevice(), m_pool, first, used, sizeof(uint64_t) * 2 * used, m_readback.data(),
				sizeof(uint64_t) * 2, VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		if (result == VK_SUCCESS || result == VK_NOT_READY) {
			uint64_t firstTick = std::numeric_limits<uint64_t>::max();
			uint64_t lastTick = 0;
			uint64_t busyTicks = 0;
			for (uint32_t i = 0; i + 1 < used; i += 2) {
				const uint64_t beginTick = m_readback[2 * static_cast<size_t>(i)] & m_validMask;
				const uint64_t endTick = m_readback[2 * static_cast<size_t>(i) + 2] & m_validMask;
				const bool available = m_readback[2 * static_cast<size_t>(i) + 1] != 0 &&
									   m_readback[2 * static_cast<size_t>(i) + 3] != 0;
				if (!available || endTick < beginTick)
					continue;
				busyTicks += endTick - beginTick;
				firstTick = std::min(firstTick, beginTick);
				lastTick = std::max(lastTick, endTick);
				++timing.intervalCount;
			}
			if (timing.intervalCount > 0) {
				timing.busyMs = static_cast<double>(busyTicks) * m_periodNs * 1e-6;
				timing.spanMs = static_cast<double>(lastTick - firstTick) * m_periodNs * 1e-6;
			}
		} else {
			OWL_CORE_WARN("Vulkan: failed to read the timestamp queries ({}).", resultString(result))
		}
	}
	m_completed.push_back(timing);
	m_slotFrame[iSlot] = 0;
	m_slotUsed[iSlot] = 0;
}

auto FrameProfiler::popTimings() -> std::vector<GpuFrameTiming> {
	std::vector<GpuFrameTiming> out;
	out.swap(m_completed);
	return out;
}

auto FrameProfiler::deviceWaitIdle(VkDevice iDevice) -> VkResult {
	++m_counters.deviceWaitIdles;
	return vkDeviceWaitIdle(iDevice);
}

void FrameProfiler::release() {
	m_enabled = false;
	m_openOneShots.clear();
	if (m_pool == VK_NULL_HANDLE)
		return;
	const auto& core = VulkanCore::get();
	if (core.getLogicalDevice() != nullptr)
		vkDestroyQueryPool(core.getLogicalDevice(), m_pool, nullptr);
	m_pool = VK_NULL_HANDLE;
}

auto FrameProfiler::get() -> FrameProfiler& {
	static FrameProfiler instance;
	return instance;
}

}// namespace owl::renderer::gpu::vulkan::internal
