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
#include "core/utils/StringUtils.h"
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
		const auto* gc = dynamic_cast<GraphContext*>(core::Application::get().getWindow().getGraphContext());
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
							   [&](const auto& iExtension) { return this->hasExtension(iExtension); });
}

auto PhysicalDeviceCapabilities::getScore() const -> uint32_t {
	if (features.geometryShader == 0u)
		return 0;
	if (surfaceFormats.empty() || presentModes.empty())
		return 0;
	uint32_t score = 0;
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM ||
		properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER)
		return 0;
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		score += 1000;// favor graphic card over CPU-integrated GPU
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
		properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
		score += 100;// favor graphic card over CPU
	score += properties.limits.maxImageDimension2D;
	score += getTotalMemory() / 1024 / 1024 / 1024;
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
						  [](const PhysicalDeviceCapabilities& iFirst, const PhysicalDeviceCapabilities& iSecond) {
							  return iFirst.getScore() > iSecond.getScore();
						  });
	OWL_CORE_TRACE("Vulkan: Found {} physical devices.", resultVec.size())
	for (const auto& device: resultVec) {
		OWL_CORE_TRACE("{}", device.getDetailedName())
		OWL_CORE_TRACE("{}", device.getCapabilityString())
	}
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
namespace {
std::string vkDeviceTypeToString(const VkPhysicalDeviceType& iType) {
	switch (iType) {
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			return "Other";
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			return "Integrated GPU";
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			return "Discrete GPU";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			return "Virtual GPU";
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			return "CPU";
		case VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM:
			return "Unknown";
	}
	return "Unknown";
}
}// namespace
auto PhysicalDeviceCapabilities::getSimpleName() const -> std::string {

	return std::format("{} ({})", properties.deviceName, vkDeviceTypeToString(properties.deviceType));
}
auto PhysicalDeviceCapabilities::getDetailedName() const -> std::string {
	std::string result;
	std::format_to(std::back_inserter(result), "{}\n", getSimpleName());
	std::format_to(std::back_inserter(result), "  - Score: {}", getScore());
	return result;
}
auto PhysicalDeviceCapabilities::getCapabilityString() const -> std::string {
	std::string result;
	std::format_to(std::back_inserter(result), "Device Capabilities:\n");
	std::format_to(std::back_inserter(result), "--------------------\n");
	std::format_to(std::back_inserter(result), " Memory : {}\n", core::utils::sizeToString(getTotalMemory()));
	return result;
}

[[nodiscard]] auto PhysicalDeviceCapabilities::getTotalMemory() const -> size_t {
	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG("-Wunsafe-buffer-usage")
	size_t total = 0;
	for (uint32_t i = 0; i < memoryProperties.memoryHeapCount; ++i) { total += memoryProperties.memoryHeaps[i].size; }
	OWL_DIAG_PUSH
	return total;
}

}// namespace owl::renderer::vulkan::internal
