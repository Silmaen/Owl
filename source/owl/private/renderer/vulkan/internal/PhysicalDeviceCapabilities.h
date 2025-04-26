/**
 * @file PhysicalDeviceCapabilities.h
 * @author Silmaen
 * @date 11/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace owl::renderer::vulkan::internal {
/**
 * @brief Class for managing Vulkan physical device capabilities.
 */
class PhysicalDeviceCapabilities final {
public:
	/**
	 * @brief Default Constructor.
	 * @param[in] iDev The physical device to analyse.
	 */
	explicit PhysicalDeviceCapabilities(const VkPhysicalDevice& iDev = nullptr);
	/**
	 * @brief Copy Constructor.
	 */
	PhysicalDeviceCapabilities(const PhysicalDeviceCapabilities&) = default;
	/**
	 * @brief Move Constructor.
	 */
	PhysicalDeviceCapabilities(PhysicalDeviceCapabilities&&) = default;
	/**
	 * @brief Copy assignation.
	 */
	auto operator=(const PhysicalDeviceCapabilities&) -> PhysicalDeviceCapabilities& = default;
	/**
	 * @brief Move assignation.
	 */
	auto operator=(PhysicalDeviceCapabilities&&) -> PhysicalDeviceCapabilities& = default;

	/**
	 * @brief Destructor.
	 */
	~PhysicalDeviceCapabilities();
	/**
	 * @brief compute the device's score.
	 * @return The device's score.
	 */
	[[nodiscard]] auto getScore() const -> uint32_t;

	/**
	 * @brief Check if a layer is available on this device.
	 * @param[in] iLayer The layer name to test.
	 * @return True if available
	 */
	[[nodiscard]] auto hasLayer(const std::string& iLayer) const -> bool;
	/**
	 * @brief Check if an extension is available on this device.
	 * @param[in] iExtension The extension name to test.
	 * @return True if available
	 */
	[[nodiscard]] auto hasExtension(const std::string& iExtension) const -> bool;
	/**
	 * @brief Check if some layers are available on this device.
	 * @param[in] iLayers The layer's names to test.
	 * @return True if all available
	 */
	[[nodiscard]] auto hasLayers(const std::vector<std::string>& iLayers) const -> bool;
	/**
	 * @brief Check if some extensions are available on this device.
	 * @param[in] iExtensions The extension's names to test.
	 * @return True if all available
	 */
	[[nodiscard]] auto hasExtensions(const std::vector<std::string>& iExtensions) const -> bool;

	/**
	 * @brief Get a simple string describing the device.
	 * @return The device name and few details.
	 */
	[[nodiscard]] auto getSimpleName() const -> std::string;
	/**
	 * @brief Get a detailed string describing the device.
	 * @return The device name and some details.
	 */
	[[nodiscard]] auto getDetailedName() const -> std::string;
	/**
	 * @brief Get a string describing the device capabilities.
	 * @return The device name and some details.
	 */
	[[nodiscard]] auto getCapabilityString() const -> std::string;

	/**
	 * @brief Force to check for surface changes.
	 */
	void updateSurfaceInformation();

	[[nodiscard]] auto getTotalMemory() const -> size_t;

	/// The physical device.
	VkPhysicalDevice device = nullptr;
	/// Available layers.
	std::vector<VkLayerProperties> supportedLayers;
	/// Available extensions.
	std::vector<VkExtensionProperties> supportedExtensions;
	/// The physical device properties.
	VkPhysicalDeviceProperties properties{};
	/// Features available on the selected physical device (for e.g. checking if a feature is available).
	VkPhysicalDeviceFeatures features{};
	/// Available memory (type) properties for the physical device
	VkPhysicalDeviceMemoryProperties memoryProperties{};
	/// List of queue families.
	std::vector<VkQueueFamilyProperties> queueFamilies;
	/// Capability of the physical surface
	VkSurfaceCapabilitiesKHR surfaceCapabilities{};
	/// List of supported surface format.
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	/// List of supported presentation modes.
	std::vector<VkPresentModeKHR> presentModes;

	/// Index of the graphic queue.
	uint32_t graphicQueueIndex = std::numeric_limits<uint32_t>::max();
	/// Index of the present queue.
	uint32_t presentQueueIndex = std::numeric_limits<uint32_t>::max();
};

/**
 * @brief Enumerate the physical devices.
 * @param[in] iInstance The vulkan instance.
 * @return List of physical devices sorted by decreasing score.
 */
auto enumerateDevices(const VkInstance& iInstance) -> std::vector<PhysicalDeviceCapabilities>;
}// namespace owl::renderer::vulkan::internal
