/**
 * @file GpuProfiler.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "GpuProfiler.h"

#ifdef OWL_PROFILER_TRACY
#include "VulkanCore.h"
#include "core/external/tracy.h"

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wold-style-cast")
OWL_DIAG_DISABLE_CLANG("-Wcast-align")
OWL_DIAG_DISABLE_CLANG("-Wsign-conversion")
OWL_DIAG_DISABLE_CLANG("-Wshorten-64-to-32")
OWL_DIAG_DISABLE_CLANG("-Wimplicit-int-conversion")
OWL_DIAG_DISABLE_CLANG("-Wzero-as-null-pointer-constant")
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
OWL_DIAG_DISABLE_CLANG("-Wreserved-macro-identifier")
OWL_DIAG_DISABLE_CLANG("-Wundef")
OWL_DIAG_DISABLE_CLANG("-Wmissing-field-initializers")
OWL_DIAG_DISABLE_CLANG("-Wunused-member-function")
#include <tracy/TracyVulkan.hpp>
OWL_DIAG_POP

#include <cstdint>
#include <optional>
#endif

namespace owl::renderer::gpu::vulkan::internal {

#ifdef OWL_PROFILER_TRACY
namespace {
constexpr tracy::SourceLocationData g_batchLocation{"Vulkan batch", "GpuProfiler::beginBatch", __FILE__, __LINE__, 0};
tracy::VkCtx* g_context = nullptr;
std::optional<tracy::VkCtxScope> g_batchZone;
}// namespace

void GpuProfiler::init() {
	if (g_context != nullptr)
		return;
	const auto& core = VulkanCore::get();
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(core.getPhysicalDevice(), &properties);
	if (properties.limits.timestampComputeAndGraphics == VK_FALSE) {
		OWL_CORE_WARN("Vulkan: No timestamp support on the graphic queue, Tracy GPU zones disabled.")
		return;
	}
	VkCommandBuffer commandBuffer = core.createCommandBuffer();
	if (commandBuffer == nullptr)
		return;
	g_context = tracy::CreateVkContext(core.getPhysicalDevice(), core.getLogicalDevice(), core.getGraphicQueue(),
									   commandBuffer, nullptr, nullptr);
	constexpr std::string_view name{"Vulkan"};
	g_context->Name(name.data(), static_cast<uint16_t>(name.size()));
}

void GpuProfiler::release() {
	if (g_context == nullptr)
		return;
	g_batchZone.reset();
	vkDeviceWaitIdle(VulkanCore::get().getLogicalDevice());
	tracy::DestroyVkContext(g_context);
	g_context = nullptr;
}

void GpuProfiler::beginBatch(VkCommandBuffer iCommandBuffer) {
	if (g_context == nullptr || g_batchZone.has_value())
		return;
	g_context->Collect(iCommandBuffer);
	g_batchZone.emplace(g_context, &g_batchLocation, iCommandBuffer, true);
}

void GpuProfiler::endBatch() { g_batchZone.reset(); }
#else
void GpuProfiler::init() {}

void GpuProfiler::release() {}

void GpuProfiler::beginBatch([[maybe_unused]] VkCommandBuffer iCommandBuffer) {}

void GpuProfiler::endBatch() {}
#endif

}// namespace owl::renderer::gpu::vulkan::internal
