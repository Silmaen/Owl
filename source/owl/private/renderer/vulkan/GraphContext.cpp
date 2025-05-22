/**
 * @file GraphContext.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "GraphContext.h"

#include "internal/VulkanHandler.h"

namespace owl::renderer::vulkan {

GraphContext::GraphContext(GLFWwindow* ioWindow) : mp_wnd(ioWindow) {}

GraphContext::~GraphContext() = default;

void GraphContext::swapBuffers() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.swapFrame();
}

void GraphContext::init() {}

auto GraphContext::getVersion() const -> Version {
	const int version = internal::VulkanHandler::get().getVersion();
	return {.major=static_cast<int32_t>(VK_API_VERSION_MAJOR(version)), .minor=static_cast<int32_t>(VK_API_VERSION_MAJOR(version))};
}

auto GraphContext::createSurface(const VkInstance& iInstance) -> VkResult {
	return glfwCreateWindowSurface(iInstance, mp_wnd, nullptr, &m_surface);
}


void GraphContext::destroySurface(const VkInstance& iInstance) {
	vkDestroySurfaceKHR(iInstance, m_surface, nullptr);
	m_surface = nullptr;
}

void GraphContext::waitIdle() {
	OWL_CORE_TRACE("GraphContext Wait for Idle.")
	const auto& vkh = internal::VulkanCore::get();
	vkDeviceWaitIdle(vkh.getLogicalDevice());
}

}// namespace owl::renderer::vulkan
