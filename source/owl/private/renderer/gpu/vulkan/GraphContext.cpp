/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GraphContext.h"

#include "internal/FrameProfiler.h"
#include "internal/VulkanHandler.h"

#ifdef OWL_PLATFORM_ANDROID
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#else
#include "core/external/glfw3.h"
#endif

#include <cstdint>

namespace owl::renderer::gpu::vulkan {

GraphContext::GraphContext(void* ioWindow) : mp_wnd(ioWindow) {}

GraphContext::~GraphContext() = default;

void GraphContext::swapBuffers() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.swapFrame();
}

void GraphContext::init() {}

auto GraphContext::getVersion() const -> Version {
	const uint32_t version = internal::VulkanCore::get().getApiVersion();
	return {.major = static_cast<int32_t>(VK_API_VERSION_MAJOR(version)),
			.minor = static_cast<int32_t>(VK_API_VERSION_MINOR(version))};
}

auto GraphContext::createSurface(const VkInstance& iInstance) -> VkResult {
#ifdef OWL_PLATFORM_ANDROID
	const VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
												   .pNext = nullptr,
												   .flags = 0,
												   .window = static_cast<ANativeWindow*>(mp_wnd)};
	return vkCreateAndroidSurfaceKHR(iInstance, &createInfo, nullptr, &m_surface);
#else
	return glfwCreateWindowSurface(iInstance, static_cast<GLFWwindow*>(mp_wnd), nullptr, &m_surface);
#endif
}

auto GraphContext::getRequiredInstanceExtensions() -> std::vector<const char*> {
#ifdef OWL_PLATFORM_ANDROID
	return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
#else
	uint32_t count = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);
	if (extensions == nullptr)
		return {VK_KHR_SURFACE_EXTENSION_NAME};
	std::vector<const char*> result(extensions, extensions + count);
	result.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	return result;
#endif
}

void GraphContext::destroySurface(const VkInstance& iInstance) {
	vkDestroySurfaceKHR(iInstance, m_surface, nullptr);
	m_surface = nullptr;
}

void GraphContext::waitIdle() {
	const auto& vkh = internal::VulkanCore::get();
	if (vkh.getLogicalDevice() == nullptr)
		return;
	OWL_CORE_TRACE("Vulkan: Waiting for the device to be idle.")
	internal::FrameProfiler::get().deviceWaitIdle(vkh.getLogicalDevice());
}

}// namespace owl::renderer::gpu::vulkan
