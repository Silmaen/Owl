/**
 * @file VulkanCore.h
 * @author Silmaen
 * @date 11/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PhysicalDeviceCapabilities.h"
#include "math/vectors.h"

/**
 * @brief Namespace for vulkan internal functions
 */
namespace owl::renderer::vulkan::internal {

constexpr uint32_t g_maxFrameInFlight = 2;

/**
 * @brief Simple struct to gather the vulkan configurations.
 */
struct VulkanConfiguration {
	/// If the validation layers should be activated.
	bool activeValidation = false;
	/// If the debug message should be activated.
	bool debugMessage = false;
};

/**
 * @brief Information about the vulkan's instance.
 */
struct InstanceInformations {
	InstanceInformations();

	[[nodiscard]] auto hasMinimalVersion(uint8_t iMajor, uint8_t iMinor, uint8_t iPatch = 0) const -> bool;

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

	uint32_t version = 0;
	std::vector<std::string> supportedExtensions;
	std::vector<std::string> supportedLayers;
};

/**
 * @brief Class gathering Vulkan core functions.
 */
class VulkanCore final {
public:
	VulkanCore(const VulkanCore&) = delete;

	VulkanCore(VulkanCore&&) = delete;

	auto operator=(const VulkanCore&) -> VulkanCore& = delete;

	auto operator=(VulkanCore&&) -> VulkanCore& = delete;

	/**
	 * @brief Destructor.
	 */
	~VulkanCore();

	/**
	 * @brief Singleton get instance.
	 * @return Intance of Vulkan core.
	 */
	static auto get() -> VulkanCore& {
		static VulkanCore instance;
		return instance;
	}

	/**
	 * @brief Initialize the vulkan core.
	 * @param[in] iConfiguration The Given configuratio.
	 */
	void init(const VulkanConfiguration& iConfiguration);

	/**
	 * @brief Release the vulkan core.
	 */
	void release();

	/**
	 * @brief Check if the currnt core is in good health.
	 * @return True if everything ok.
	 */
	[[nodiscard]] auto isHealthy() const -> bool;

	/**
	 * @brief Access to the vulkan instance.
	 * @return The vulkan instance.
	 */
	[[nodiscard]] auto getInstance() const -> VkInstance { return m_instance; }

	/**
	 * @brief Access to the physical device.
	 * @return The physical device.
	 */
	[[nodiscard]] auto getPhysicalDevice() const -> VkPhysicalDevice { return m_physicalDevice; }

	/**
	 * @brief Access to the logical device.
	 * @return The logical device.
	 */
	[[nodiscard]] auto getLogicalDevice() const -> VkDevice { return m_logicalDevice; }

	/**
	 * @brief The different core states
	 */
	enum struct State : uint8_t {
		/// Just created or reseted.
		Created,
		/// Initialized and ready.
		Initialized,
		/// Something is wrong.
		Error
	};

	/**
	 * @brief Access to the actual state of the core.
	 * @return The state of the core.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	/**
	 * @brief Get the graphic queue index.
	 * @return The graph queue index.
	 */
	[[nodiscard]] auto getGraphQueueFamilyIndex() const -> uint32_t { return m_phyProps->graphicQueueIndex; }

	/**
	 * @brief Access to the graphic queue.
	 * @return The graphic queue.
	 */
	[[nodiscard]] auto getGraphicQueue() const -> VkQueue { return m_graphicQueue; }
	/**
	 * @brief Access to the present queue.
	 * @return The present queue.
	 */
	[[nodiscard]] auto getPresentQueue() const -> VkQueue { return m_presentQueue; }
	/**
	 * @brief Get the current Extent.
	 * @return The current Extent.
	 */
	[[nodiscard]] auto getCurrentExtent() const -> VkExtent2D;

	/**
	 * @brief Get the current Size.
	 * @return The current size.
	 */
	[[nodiscard]] auto getCurrentSize() const -> math::vec2ui;

	/**
	 * @brief Get the surface format.
	 * @return The surface format.
	 */
	[[nodiscard]] auto getSurfaceFormat() const -> VkSurfaceFormatKHR;

	/**
	 * @brief Get the present mode.
	 * @return The present mode.
	 */
	[[nodiscard]] auto getPresentMode() const -> VkPresentModeKHR;

	/**
	 * @brief Get image count.
	 * @return The image count.
	 */
	[[nodiscard]] auto getImagecount() const -> uint32_t;

	/**
	 * @brief Get the current transform.
	 * @return The current transform.
	 */
	[[nodiscard]] auto getCurrentTransform() const -> VkSurfaceTransformFlagBitsKHR;

	/**
	 * @brief Get The queue indices.
	 * @return The queue indices.
	 */
	[[nodiscard]] auto getQueueIndices() const -> std::vector<uint32_t>;

	/**
	 * @brief Force to check for surface changes.
	 */
	void updateSurfaceInformation();

	/**
	 * @brief Search for a memory type index.
	 * @param[in] iTypeFilter Filter on type.
	 * @param[in] iMemProperties Memory properties.
	 * @return The memory type index.
	 */
	[[nodiscard]] auto findMemoryTypeIndex(uint32_t iTypeFilter, VkMemoryPropertyFlags iMemProperties) const
			-> uint32_t;

	[[nodiscard]] auto getMaxSamplerAnisotropy() const -> float;

	[[nodiscard]] auto beginSingleTimeCommands() const -> VkCommandBuffer;

	void endSingleTimeCommands(VkCommandBuffer iCommandBuffer) const;

	[[nodiscard]] auto createCommandBuffer() const -> VkCommandBuffer;

private:
	/**
	 * @brief Default Constructor.
	 */
	VulkanCore();

	/// Vulkan requested configuration.
	VulkanConfiguration m_config{};

	/// Vulkan instance.
	VkInstance m_instance = nullptr;
	/// Vulkan selected physical device.
	VkPhysicalDevice m_physicalDevice = nullptr;
	/// Vulkan logical device.
	VkDevice m_logicalDevice = nullptr;
	/// Debug messenger.
	VkDebugUtilsMessengerEXT m_debugUtilsMessenger{};

	/// Information about the instance.
	uniq<InstanceInformations> m_instanceInfo{nullptr};
	/// If  validation layer are enabled.
	bool m_hasValidation = false;
	/// If debug message are enabled.
	bool m_debugMessage = false;
	/// The internal state.
	State m_state = State::Created;
	/// Save of the current physical device properties.
	uniq<PhysicalDeviceCapabilities> m_phyProps;

	/// The graphic queue.
	VkQueue m_graphicQueue = nullptr;
	/// The present queue.
	VkQueue m_presentQueue = nullptr;

	/// The command pool.
	VkCommandPool m_commandPool{nullptr};

	void createInstance();

	void selectPhysicalDevice();

	void createLogicalDevice();

	void setupDebugging();

	void createQueues();

	void createCommandPool();
};
}// namespace owl::renderer::vulkan::internal
