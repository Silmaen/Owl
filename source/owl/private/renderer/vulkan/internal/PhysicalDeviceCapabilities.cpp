/**
 * @file PhysicalDeviceCapabilities.cpp
 * @author Silmaen
 * @date 11/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "PhysicalDeviceCapabilities.h"

#include "core/Application.h"
#include "renderer/vulkan/GraphContext.h"
#include "utils.h"

#include <vulkan/vulkan.h>

namespace owl::renderer::vulkan::internal {

PhysicalDeviceCapabilities::PhysicalDeviceCapabilities(const VkPhysicalDevice& iDev) : device(iDev) {
	if (device == nullptr)
		return;
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);
	vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);
	{
		uint32_t count = 0;
		vkEnumerateDeviceLayerProperties(device, &count, nullptr);
		supportedLayers.resize(count);
		vkEnumerateDeviceLayerProperties(device, &count, supportedLayers.data());
	}
	{
		uint32_t count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
		supportedExtensions.resize(count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &count, supportedExtensions.data());
	}
	{
		uint32_t count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
		queueFamilies.resize(count);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queueFamilies.data());
	}
	if (hasExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
		updateSurfaceInformation();
		uint32_t index = 0;
		auto* const gc = dynamic_cast<vulkan::GraphContext*>(core::Application::get().getWindow().getGraphContext());
		for (const auto& qFam: queueFamilies) {
			if ((qFam.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) {
				graphicQueueIndex = index;
			}
			VkBool32 support = 0;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, index, gc->getSurface(), &support);
			if (support != 0u) {
				presentQueueIndex = index;
			}
			index++;
		}
	}
}

PhysicalDeviceCapabilities::~PhysicalDeviceCapabilities() = default;

auto PhysicalDeviceCapabilities::hasLayer(const std::string& iLayer) const -> bool {
	return std::ranges::find_if(supportedLayers.begin(), supportedLayers.end(),
								[&iLayer](const VkLayerProperties& iLayerProp) {
									return iLayerProp.layerName == iLayer;
								}) != supportedLayers.end();
}

auto PhysicalDeviceCapabilities::hasExtension(const std::string& iExtension) const -> bool {
	return std::ranges::find_if(supportedExtensions.begin(), supportedExtensions.end(),
								[&iExtension](const VkExtensionProperties& iExtensionProp) {
									return iExtensionProp.extensionName == iExtension;
								}) != supportedExtensions.end();
}

auto PhysicalDeviceCapabilities::hasLayers(const std::vector<std::string>& iLayers) const -> bool {
	return std::ranges::all_of(iLayers.begin(), iLayers.end(),
							   [&](const auto& iLayer) { return this->hasLayer(iLayer); });
}

auto PhysicalDeviceCapabilities::hasExtensions(const std::vector<std::string>& iExtensions) const -> bool {
	return std::ranges::all_of(iExtensions.begin(), iExtensions.end(),
							   [&](const auto& extension) { return this->hasExtension(extension); });
}

auto PhysicalDeviceCapabilities::getScore() const -> uint32_t {
	if (features.geometryShader == 0u)
		return 0;
	if (surfaceFormats.empty() || presentModes.empty())
		return 0;
	uint32_t score = 0;
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;// favor graphic card over CPU-integrated GPU
	score += properties.limits.maxImageDimension2D;
	// checking the queue families
	if (graphicQueueIndex == std::numeric_limits<uint32_t>::max() || (features.samplerAnisotropy == 0u))
		score *= 0;
	return score;
}

auto enumerateDevices(const VkInstance& iInstance) -> std::vector<PhysicalDeviceCapabilities> {
	std::vector<PhysicalDeviceCapabilities> resultVec;
	uint32_t deviceCount = 0;
	{
		if (const VkResult result = vkEnumeratePhysicalDevices(iInstance, &deviceCount, nullptr);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan: Error while enumerating physical devices ({}).", resultString(result))
			return {};
		}
		if (deviceCount == 0) {
			OWL_CORE_ERROR("Vulkan: Cannot find any GPUs with vulkan support.")
			return {};
		}
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	if (const VkResult result = vkEnumeratePhysicalDevices(iInstance, &deviceCount, devices.data());
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: Error while enumerating physical devices ({}).", resultString(result))
		return {};
	}
	resultVec.reserve(devices.size());
	for (const auto& device: devices) { resultVec.emplace_back(device); }
	// sort by decreasing score...
	if (!resultVec.empty())
		std::ranges::sort(resultVec.begin(), resultVec.cend(),
						  [](const PhysicalDeviceCapabilities& a, const PhysicalDeviceCapabilities& b) {
							  return a.getScore() > b.getScore();
						  });
	return resultVec;
}

void PhysicalDeviceCapabilities::updateSurfaceInformation() {
	const auto* gc = dynamic_cast<GraphContext*>(core::Application::get().getWindow().getGraphContext());
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, gc->getSurface(), &surfaceCapabilities);
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, gc->getSurface(), &formatCount, nullptr);
	if (formatCount != 0) {
		surfaceFormats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, gc->getSurface(), &formatCount, surfaceFormats.data());
	}
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, gc->getSurface(), &presentModeCount, nullptr);
	if (presentModeCount != 0) {
		presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, gc->getSurface(), &presentModeCount, presentModes.data());
	}
}
}// namespace owl::renderer::vulkan::internal
