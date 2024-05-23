/**
 * @file VulkanCore.cpp
 * @author Silmaen
 * @date 11/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "VulkanCore.h"

#include "VulkanHandler.h"
#include "core/Application.h"
#include "renderer/vulkan/GraphContext.h"
#include "utils.h"

namespace owl::renderer::vulkan::internal {

namespace {

constexpr VkApplicationInfo g_appInfo = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
										 .pNext = nullptr,
										 .pApplicationName = "Owl",
										 .applicationVersion = {},
										 .pEngineName = "OwlEngine",
										 .engineVersion = {},
										 .apiVersion = VK_API_VERSION_1_3};

VkBool32 debugUtilsMessageCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT iMessageSeverity,
								   VkDebugUtilsMessageTypeFlagsEXT,
								   const VkDebugUtilsMessengerCallbackDataEXT *iPCallbackData, void *) {
	auto bufferName = VulkanHandler::get().getCurrentFrameBufferName();
	auto frameId = core::Application::get().getTimeStep().getFrameNumber();
	if (iMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
		OWL_CORE_TRACE("Vulkan fb({} {}): [{}][{}]: {}", frameId, bufferName, iPCallbackData->messageIdNumber,
					   iPCallbackData->pMessageIdName, iPCallbackData->pMessage)
	} else if (iMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		OWL_CORE_INFO("Vulkan fb({} {}): [{}][{}]: {}", frameId, bufferName, iPCallbackData->messageIdNumber,
					  iPCallbackData->pMessageIdName, iPCallbackData->pMessage)
	} else if (iMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		OWL_CORE_WARN("Vulkan fb({} {}): [{}][{}]: {}", frameId, bufferName, iPCallbackData->messageIdNumber,
					  iPCallbackData->pMessageIdName, iPCallbackData->pMessage)
	} else if (iMessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		OWL_CORE_ERROR("Vulkan fb({} {}): [{}][{}]: {}", frameId, bufferName, iPCallbackData->messageIdNumber,
					   iPCallbackData->pMessageIdName, iPCallbackData->pMessage)
	}
	// The return value of this callback controls whether the Vulkan call that caused the validation message will
	// be aborted or not. We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
	// If you instead want to have calls abort, pass in VK_TRUE and the function will
	// return VK_ERROR_VALIDATION_FAILED_EXT
	return false;
}

VkDebugUtilsMessengerCreateInfoEXT s_debugUtilsMessagerCi{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = {},
		.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
					   VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
		.pfnUserCallback = debugUtilsMessageCallback,
		.pUserData = nullptr};

PFN_vkCreateDebugUtilsMessengerEXT g_vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT g_vkDestroyDebugUtilsMessengerEXT;

}// namespace

// ============= VulkanCore =====================

VulkanCore::VulkanCore() = default;

VulkanCore::~VulkanCore() = default;

void VulkanCore::init(const VulkanConfiguraton &iConfiguration) {
	m_config = iConfiguration;
	if (!m_instanceInfo.hasMinimalVersion(1, 3)) {
		OWL_CORE_ERROR("Vulkan: cannot initialize du to insuficient instance version. Require 1.3")
		m_state = State::Error;
		return;
	}
	createInstance();
	if (m_state == State::Error)
		return;
	if (m_hasValidation) {
		setupDebugging();
		if (m_state == State::Error)
			return;
	}
	const auto gc = dynamic_cast<GraphContext *>(core::Application::get().getWindow().getGraphContext());
	if (const VkResult result = gc->createSurface(m_instance); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create window surface ({})", resultString(result))
		m_state = State::Error;
		return;
	}
	selectPhysicalDevice();
	if (m_state == State::Error)
		return;
	createLogicalDevice();
	createQueues();
	createCommandPool();
	if (isHealthy())
		m_state = State::Initialized;
}

void VulkanCore::release() {
	vkDeviceWaitIdle(m_logicalDevice);
	if (m_commandPool != nullptr) {
		vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
		OWL_CORE_TRACE("Vulkan: commandPool destroyed.")
	}
	if (m_logicalDevice != nullptr) {
		vkDestroyDevice(m_logicalDevice, nullptr);
		m_logicalDevice = nullptr;
		OWL_CORE_TRACE("Vulkan: logicalDevice destroyed.")
	}
	{
		const auto gc = dynamic_cast<vulkan::GraphContext *>(core::Application::get().getWindow().getGraphContext());
		gc->destroySurface(m_instance);
		OWL_CORE_TRACE("Vulkan: Surface destroyed.")
	}
	if (m_debugUtilsMessenger != nullptr) {
		g_vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugUtilsMessenger, nullptr);
		m_debugUtilsMessenger = nullptr;
		OWL_CORE_TRACE("Vulkan: debugUtilsMessenger destroyed.")
	}
	vkDestroyInstance(m_instance, nullptr);
	m_instance = nullptr;
	OWL_CORE_TRACE("Vulkan: instance destroyed.")
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void VulkanCore::createInstance() {
	// first check for requested Layers
	std::vector<std::string> requestedLayers;
	if (m_config.activeValidation) {
		if (!m_instanceInfo.hasLayer("VK_LAYER_KHRONOS_validation")) {
			OWL_CORE_WARN("Vulkan: Missing validation layers, go one without it.")
		} else {
			requestedLayers.emplace_back("VK_LAYER_KHRONOS_validation");
			m_hasValidation = true;
		}
	}
	if (!m_instanceInfo.hasLayers(requestedLayers)) {
		OWL_CORE_ERROR("Vulkan: Missing mandatory instance layers.")
		m_state = State::Error;
		return;
	}
	// second check for requested Extensions.
	std::vector<std::string> requestedExtensions;
	{
		std::set<const char *> uniqueExtensions;
		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		uniqueExtensions.insert(glfwExtensions, glfwExtensions + glfwExtensionCount);
		uniqueExtensions.emplace(VK_KHR_SURFACE_EXTENSION_NAME);
		if (m_hasValidation) {
			uniqueExtensions.emplace(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		requestedExtensions.assign(uniqueExtensions.begin(), uniqueExtensions.end());
	}
	if (!m_instanceInfo.hasExtensions(requestedExtensions)) {
		OWL_CORE_ERROR("Vulkan: Missing mandatory instance extensions.")
		m_state = State::Error;
		return;
	}
	// now creation.
	std::vector<const char *> layers;
	layers.reserve(requestedLayers.size());
	for (const auto &layer: requestedLayers) layers.emplace_back(layer.c_str());
	std::vector<const char *> extensions;
	extensions.reserve(requestedExtensions.size());
	for (const auto &extension: requestedExtensions) extensions.emplace_back(extension.c_str());

	VkInstanceCreateInfo instanceCi{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
									.pNext = m_hasValidation ? &s_debugUtilsMessagerCi : nullptr,
									.flags = {},
									.pApplicationInfo = &g_appInfo,
									.enabledLayerCount = static_cast<uint32_t>(layers.size()),
									.ppEnabledLayerNames = layers.data(),
									.enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
									.ppEnabledExtensionNames = extensions.data()};
	VkResult result = vkCreateInstance(&instanceCi, nullptr, &m_instance);
	bool second = false;
	if (result == VK_ERROR_LAYER_NOT_PRESENT) {
		OWL_CORE_ERROR("Vulkan: Unable to create instance because a layer can't be loaded, Retry without layers.")
		second = true;
		instanceCi.ppEnabledLayerNames = nullptr;
		instanceCi.enabledLayerCount = 0;
		result = vkCreateInstance(&instanceCi, nullptr, &m_instance);
	}
	if (result != VK_SUCCESS) {
		if (second)
			OWL_CORE_ERROR("Vulkan: Definitively unable to create instance ({}).", resultString(result))
		else
			OWL_CORE_ERROR("Vulkan: Unable to create instance ({}).", resultString(result))
		m_state = State::Error;
	}
}
OWL_DIAG_POP

bool VulkanCore::isHealthy() const {
	return m_instance != nullptr && m_physicalDevice != nullptr && m_logicalDevice != nullptr &&
		   m_graphicQueue != nullptr && m_presentQueue != nullptr;
}

void VulkanCore::selectPhysicalDevice() {
	const auto devices = enumerateDevices(m_instance);
	if (devices.empty()) {
		OWL_CORE_ERROR("Vulkan: No suitable physical device found.")
		m_state = State::Error;
		return;
	}
	m_phyProps = devices.front();
	if (m_phyProps.getScore() == 0) {
		OWL_CORE_ERROR("Vulkan: No suitable physical device found, scores 0.")
		m_state = State::Error;
		return;
	}
	m_physicalDevice = m_phyProps.device;
}

void VulkanCore::createLogicalDevice() {
	std::vector<VkDeviceQueueCreateInfo> deviceQueuesCi;
	float queuePriority = 1.0f;
	for (const uint32_t queue: std::set{m_phyProps.graphicQueueIndex, m_phyProps.presentQueueIndex}) {
		VkDeviceQueueCreateInfo deviceQueueCi{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = {},
											  .queueFamilyIndex = queue,
											  .queueCount = 1,
											  .pQueuePriorities = &queuePriority};
		deviceQueuesCi.push_back(deviceQueueCi);
	}
	const std::vector layerNames = {"VK_LAYER_KHRONOS_validation"};
	const std::vector extensionNames = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	constexpr VkPhysicalDeviceFeatures features = {.robustBufferAccess = VK_FALSE,
												   .fullDrawIndexUint32 = VK_FALSE,
												   .imageCubeArray = VK_FALSE,
												   .independentBlend = VK_TRUE,
												   .geometryShader = VK_FALSE,
												   .tessellationShader = VK_FALSE,
												   .sampleRateShading = VK_FALSE,
												   .dualSrcBlend = VK_FALSE,
												   .logicOp = VK_FALSE,
												   .multiDrawIndirect = VK_FALSE,
												   .drawIndirectFirstInstance = VK_FALSE,
												   .depthClamp = VK_FALSE,
												   .depthBiasClamp = VK_FALSE,
												   .fillModeNonSolid = VK_FALSE,
												   .depthBounds = VK_FALSE,
												   .wideLines = VK_FALSE,
												   .largePoints = VK_FALSE,
												   .alphaToOne = VK_FALSE,
												   .multiViewport = VK_FALSE,
												   .samplerAnisotropy = VK_TRUE,
												   .textureCompressionETC2 = VK_FALSE,
												   .textureCompressionASTC_LDR = VK_FALSE,
												   .textureCompressionBC = VK_FALSE,
												   .occlusionQueryPrecise = VK_FALSE,
												   .pipelineStatisticsQuery = VK_FALSE,
												   .vertexPipelineStoresAndAtomics = VK_FALSE,
												   .fragmentStoresAndAtomics = VK_FALSE,
												   .shaderTessellationAndGeometryPointSize = VK_FALSE,
												   .shaderImageGatherExtended = VK_FALSE,
												   .shaderStorageImageExtendedFormats = VK_FALSE,
												   .shaderStorageImageMultisample = VK_FALSE,
												   .shaderStorageImageReadWithoutFormat = VK_FALSE,
												   .shaderStorageImageWriteWithoutFormat = VK_FALSE,
												   .shaderUniformBufferArrayDynamicIndexing = VK_FALSE,
												   .shaderSampledImageArrayDynamicIndexing = VK_FALSE,
												   .shaderStorageBufferArrayDynamicIndexing = VK_FALSE,
												   .shaderStorageImageArrayDynamicIndexing = VK_FALSE,
												   .shaderClipDistance = VK_FALSE,
												   .shaderCullDistance = VK_FALSE,
												   .shaderFloat64 = VK_FALSE,
												   .shaderInt64 = VK_FALSE,
												   .shaderInt16 = VK_FALSE,
												   .shaderResourceResidency = VK_FALSE,
												   .shaderResourceMinLod = VK_FALSE,
												   .sparseBinding = VK_FALSE,
												   .sparseResidencyBuffer = VK_FALSE,
												   .sparseResidencyImage2D = VK_FALSE,
												   .sparseResidencyImage3D = VK_FALSE,
												   .sparseResidency2Samples = VK_FALSE,
												   .sparseResidency4Samples = VK_FALSE,
												   .sparseResidency8Samples = VK_FALSE,
												   .sparseResidency16Samples = VK_FALSE,
												   .sparseResidencyAliased = VK_FALSE,
												   .variableMultisampleRate = VK_FALSE,
												   .inheritedQueries = VK_FALSE};
	const VkDeviceCreateInfo deviceCi{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
									  .pNext = nullptr,
									  .flags = {},
									  .queueCreateInfoCount = static_cast<uint32_t>(deviceQueuesCi.size()),
									  .pQueueCreateInfos = deviceQueuesCi.data(),
									  .enabledLayerCount = m_hasValidation
																   ? static_cast<uint32_t>(extensionNames.size())
																   : static_cast<uint32_t>(extensionNames.size()) - 1,
									  .ppEnabledLayerNames = layerNames.data(),
									  .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
									  .ppEnabledExtensionNames = extensionNames.data(),
									  .pEnabledFeatures = &features};
	if (const VkResult result = vkCreateDevice(m_physicalDevice, &deviceCi, nullptr, &m_logicalDevice);
		result != VK_SUCCESS) {
		m_state = State::Error;
		OWL_CORE_ERROR("Vulkan: Error while creating the logical device ({})", resultString(result))
	}
}

void VulkanCore::setupDebugging() {
	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG17("-Wcast-function-type-strict")
	g_vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
	g_vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
	OWL_DIAG_POP
	if (const VkResult result =
				g_vkCreateDebugUtilsMessengerEXT(m_instance, &s_debugUtilsMessagerCi, nullptr, &m_debugUtilsMessenger);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: Error while setup debugging ({})", resultString(result))
		m_state = State::Error;
	}
}

void VulkanCore::createQueues() {
	vkGetDeviceQueue(m_logicalDevice, m_phyProps.graphicQueueIndex, 0, &m_graphicQueue);
	vkGetDeviceQueue(m_logicalDevice, m_phyProps.presentQueueIndex, 0, &m_presentQueue);
}

VkExtent2D VulkanCore::getCurrentExtent() const {
	VkExtent2D extent;
	if (m_phyProps.surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = m_phyProps.surfaceCapabilities.currentExtent;
	} else {
		auto sizes = core::Application::get().getWindow().getSize();
		extent.width = std::clamp(sizes.width(), m_phyProps.surfaceCapabilities.minImageExtent.width,
								  m_phyProps.surfaceCapabilities.maxImageExtent.width);
		extent.height = std::clamp(sizes.height(), m_phyProps.surfaceCapabilities.minImageExtent.height,
								   m_phyProps.surfaceCapabilities.maxImageExtent.height);
	}
	return extent;
}

math::FrameSize VulkanCore::getCurrentSize() const {
	math::FrameSize extent;
	if (m_phyProps.surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		extent = toFrameSize(m_phyProps.surfaceCapabilities.currentExtent);
	} else {
		auto sizes = core::Application::get().getWindow().getSize();
		extent.width() = std::clamp(sizes.width(), m_phyProps.surfaceCapabilities.minImageExtent.width,
									m_phyProps.surfaceCapabilities.maxImageExtent.width);
		extent.height() = std::clamp(sizes.height(), m_phyProps.surfaceCapabilities.minImageExtent.height,
									 m_phyProps.surfaceCapabilities.maxImageExtent.height);
	}
	return extent;
}

VkSurfaceFormatKHR VulkanCore::getSurfaceFormat() const {
	VkSurfaceFormatKHR surfaceFormat = m_phyProps.surfaceFormats.front();
	for (const auto &availableFormat: m_phyProps.surfaceFormats) {
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
			availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = availableFormat;
		}
	}
	return surfaceFormat;
}

VkPresentModeKHR VulkanCore::getPresentMode() const {
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto &availablePresentMode: m_phyProps.presentModes) {
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = availablePresentMode;
		}
	}
	return presentMode;
}

uint32_t VulkanCore::getImagecount() const {
	uint32_t imageCount = m_phyProps.surfaceCapabilities.minImageCount + 1;
	if (m_phyProps.surfaceCapabilities.maxImageCount > 0 && imageCount > m_phyProps.surfaceCapabilities.maxImageCount) {
		imageCount = m_phyProps.surfaceCapabilities.maxImageCount;
	}
	return imageCount;
}

VkSurfaceTransformFlagBitsKHR VulkanCore::getCurrentTransform() const {
	return m_phyProps.surfaceCapabilities.currentTransform;
}

std::vector<uint32_t> VulkanCore::getQueueIndicies() const {
	return {m_phyProps.graphicQueueIndex, m_phyProps.presentQueueIndex};
}

void VulkanCore::updateSurfaceInformations() { m_phyProps.updateSurfaceInformations(); }

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
uint32_t VulkanCore::findMemoryTypeIndex(const uint32_t iTypeFilter, const VkMemoryPropertyFlags iMemProperties) const {
	for (uint32_t i = 0; i < m_phyProps.memoryProperties.memoryTypeCount; i++) {
		if ((iTypeFilter & (1 << i)) &&
			(m_phyProps.memoryProperties.memoryTypes[i].propertyFlags & iMemProperties) == iMemProperties) {
			return i;
		}
	}
	OWL_CORE_ERROR("Vulkan PhysicalDevice: failed to find suitable memory type!")
	return std::numeric_limits<uint32_t>::max();
}
OWL_DIAG_POP

float VulkanCore::getMaxSamplerAnisotropy() const { return m_phyProps.properties.limits.maxSamplerAnisotropy; }

VkCommandBuffer VulkanCore::beginSingleTimeCommands() const {
	const auto &core = VulkanCore::get();
	const VkCommandBufferAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												.pNext = nullptr,
												.commandPool = m_commandPool,
												.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												.commandBufferCount = 1};

	VkCommandBuffer commandBuffer;
	if (const VkResult result = vkAllocateCommandBuffers(core.getLogicalDevice(), &allocInfo, &commandBuffer);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create command buffer for buffer copy.")
		return nullptr;
	}

	constexpr VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
												 .pNext = nullptr,
												 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
												 .pInheritanceInfo = nullptr};
	if (const VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to begin command buffer for buffer copy.")
		return nullptr;
	}
	return commandBuffer;
}

void VulkanCore::endSingleTimeCommands(VkCommandBuffer iCommandBuffer) const {
	const auto &core = VulkanCore::get();
	if (const VkResult result = vkEndCommandBuffer(iCommandBuffer); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to end command buffer for buffer copy.")
		return;
	}

	const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								  .pNext = nullptr,
								  .waitSemaphoreCount = 0,
								  .pWaitSemaphores = nullptr,
								  .pWaitDstStageMask = nullptr,
								  .commandBufferCount = 1,
								  .pCommandBuffers = &iCommandBuffer,
								  .signalSemaphoreCount = 0,
								  .pSignalSemaphores = nullptr};

	if (const VkResult result = vkQueueSubmit(core.getGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to submit to queue for buffer copy.")
		return;
	}
	if (const VkResult result = vkQueueWaitIdle(core.getGraphicQueue()); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to wait for idle queue for buffer copy.")
		return;
	}

	vkFreeCommandBuffers(core.getLogicalDevice(), m_commandPool, 1, &iCommandBuffer);
}


[[nodiscard]] VkCommandBuffer VulkanCore::createCommandBuffer() const {
	VkCommandBuffer cmd;
	const VkCommandBufferAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
												.pNext = nullptr,
												.commandPool = m_commandPool,
												.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
												.commandBufferCount = 1};
	if (const VkResult result = vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, &cmd); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Core: failed to allocate command buffers ({}).", resultString(result))
	}
	return cmd;
}

void VulkanCore::createCommandPool() {
	const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
										   .pNext = nullptr,
										   .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
										   .queueFamilyIndex = getGraphQueueFamilyIndex()};

	if (const VkResult result = vkCreateCommandPool(getLogicalDevice(), &poolInfo, nullptr, &m_commandPool);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Core: failed to create command pool ({}).", resultString(result))
		m_state = State::Error;
	}
}

// ============= VulkanCore =====================

// ============= InstanceInformations =====================

InstanceInformations::InstanceInformations() {
	vkEnumerateInstanceVersion(&version);
	OWL_CORE_INFO("Vulkan: Found API Version: {}.{}.", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version))
	// Get the list of available layers
	{
		supportedLayers.clear();
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> layers(layerCount);
		if (const VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
			result == VK_SUCCESS) {
			for (const auto &[layerName, specVersion, implementationVersion, description]: layers) {
				supportedLayers.emplace_back(layerName);
				OWL_CORE_TRACE("Vulkan: Instance layer: {} version {} // {}", layerName, specVersion, description)
			}
		} else {
			OWL_CORE_WARN("Vulkan: unable to enumerate instance layers ({}).", resultString(result))
		}
	}
	// Get extensions supported by the instance and store for later use
	{
		supportedExtensions.clear();
		uint32_t extCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
		if (extCount > 0) {
			std::vector<VkExtensionProperties> extensions(extCount);
			if (const VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front());
				result == VK_SUCCESS) {
				for (const auto &[extensionName, specVersion]: extensions) {
					supportedExtensions.emplace_back(extensionName);
					OWL_CORE_TRACE("Vulkan: Supported instance extension: {} version: {}", extensionName, specVersion)
				}
			} else {
				OWL_CORE_WARN("Vulkan: unable to enumerate instance extensions ({}).", resultString(result))
			}
		}
	}
}

bool InstanceInformations::hasMinimalVersion(const uint8_t iMajor, const uint8_t iMinor, const uint8_t iPatch) const {
	return VK_API_VERSION_MAJOR(version) > iMajor ||
		   (VK_API_VERSION_MAJOR(version) == iMajor && VK_API_VERSION_MINOR(version) > iMinor) ||
		   (VK_API_VERSION_MAJOR(version) == iMajor && VK_API_VERSION_MINOR(version) == iMinor &&
			VK_API_VERSION_PATCH(version) >= iPatch);
}

bool InstanceInformations::hasLayer(const std::string &iLayer) const {
	return std::ranges::find(supportedLayers.begin(), supportedLayers.end(), iLayer) != supportedLayers.end();
}

bool InstanceInformations::hasExtension(const std::string &iExtension) const {
	return std::ranges::find(supportedExtensions.begin(), supportedExtensions.end(), iExtension) !=
		   supportedExtensions.end();
}

bool InstanceInformations::hasLayers(const std::vector<std::string> &iLayers) const {
	return std::ranges::all_of(iLayers.begin(), iLayers.end(),
							   [&](const auto &iLayer) { return this->hasLayer(iLayer); });
}

bool InstanceInformations::hasExtensions(const std::vector<std::string> &iExtensions) const {
	return std::ranges::all_of(iExtensions.begin(), iExtensions.end(),
							   [&](const auto &iExtension) { return this->hasExtension(iExtension); });
}

// ============= InstanceInformations =====================

}// namespace owl::renderer::vulkan::internal
