/**
 * @file utils.h
 * @author Silmaen
 * @date 06/02/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include "renderer/gpu/Framebuffer.h"
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan::internal {

/**
 * @brief
 *  Convert a `VkResult` into its identifier as a string (for logs and assertions).
 * @param[in] iResult The Vulkan result code.
 * @return The matching `VK_*` identifier, or `"VK_RESULT_unknown"` for unknown codes.
 */
static constexpr auto resultString(const VkResult iResult) -> std::string_view {
	switch (iResult) {
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_NOT_READY:
			return "VK_NOT_READY";
		case VK_TIMEOUT:
			return "VK_TIMEOUT";
		case VK_EVENT_SET:
			return "VK_EVENT_SET";
		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";
		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL:
			return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_UNKNOWN:
			return "VK_ERROR_UNKNOWN";
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:
			return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case VK_ERROR_FRAGMENTATION:
			return "VK_ERROR_FRAGMENTATION";
		case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
			return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case VK_PIPELINE_COMPILE_REQUIRED:
			return "VK_PIPELINE_COMPILE_REQUIRED";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV:
			return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
			return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
			return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
			return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
			return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
			return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
		case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
			return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
		case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
			return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		case VK_ERROR_NOT_PERMITTED_KHR:
			return "VK_ERROR_NOT_PERMITTED_KHR";
		case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
			return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		case VK_THREAD_IDLE_KHR:
			return "VK_THREAD_IDLE_KHR";
		case VK_THREAD_DONE_KHR:
			return "VK_THREAD_DONE_KHR";
		case VK_OPERATION_DEFERRED_KHR:
			return "VK_OPERATION_DEFERRED_KHR";
		case VK_OPERATION_NOT_DEFERRED_KHR:
			return "VK_OPERATION_NOT_DEFERRED_KHR";
		case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
			return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
		case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
			return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
		case VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT:
			return "VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT";
		case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
			return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
		case VK_PIPELINE_BINARY_MISSING_KHR:
			return "VK_PIPELINE_BINARY_MISSING_KHR";
		case VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT:
			return "VK_ERROR_PRESENT_TIMING_QUEUE_FULL_EXT";
		case VK_RESULT_MAX_ENUM:
			return "VK_RESULT_MAX_ENUM";
	}
	return "VK_RESULT_unknown";
}

/**
 * @brief
 *  Translate an engine attachment format to its Vulkan counterpart.
 * @param[in] iFormat The image/buffer format.
 * @return The Vulkan format.
 */
auto attachmentFormatToVulkan(const AttachmentSpecification::Format& iFormat) -> VkFormat;

/**
 * @brief
 *  Get the Vulkan image-aspect flags suited to the given attachment format.
 * @param[in] iFormat The image/buffer format.
 * @return The aspect flags (color/depth/stencil).
 */
auto attachmentFormatToAspect(const AttachmentSpecification::Format& iFormat) -> VkImageAspectFlags;

/**
 * @brief
 *  Get the per-pixel byte size of the given attachment format.
 * @param[in] iFormat The image/buffer format.
 * @return Size in bytes.
 */
auto attachmentFormatToSize(const AttachmentSpecification::Format& iFormat) -> uint32_t;

/**
 * @brief
 *  Translate an engine attachment tiling mode to the Vulkan counterpart.
 * @param[in] iTiling The engine tiling enum.
 * @return The Vulkan tiling mode.
 */
auto attachmentTilingToVulkan(const AttachmentSpecification::Tiling& iTiling) -> VkImageTiling;

/**
 * @brief
 *  Copy `iSize` bytes between two device buffers using a transient command buffer.
 * @param[in] iSrcBuffer Source buffer.
 * @param[in] iDstBuffer Destination buffer.
 * @param[in] iSize Number of bytes to copy.
 */
void copyBuffer(const VkBuffer& iSrcBuffer, const VkBuffer& iDstBuffer, VkDeviceSize iSize);

/**
 * @brief
 *  Allocate a Vulkan buffer with the requested usage and memory properties.
 * @param[in] iSize Buffer size in bytes.
 * @param[in] iUsage Buffer usage flags.
 * @param[in] iProperties Required memory property flags (host-visible, device-local, ...).
 * @param[out] iBuffer Out: created buffer handle.
 * @param[out] iBufferMemory Out: bound memory allocation handle.
 */
void createBuffer(VkDeviceSize iSize, VkBufferUsageFlags iUsage, VkMemoryPropertyFlags iProperties, VkBuffer& iBuffer,
				  VkDeviceMemory& iBufferMemory);

/**
 * @brief
 *  Free buffer.
 * @param[in] iDevice The Vulkan device handle.
 * @param[in] iBuffer The buffer.
 * @param[in] iBufferMemory The buffer memory handle.
 */
void freeBuffer(const VkDevice& iDevice, const VkBuffer& iBuffer, const VkDeviceMemory& iBufferMemory);

/**
 * @brief
 *  Transition image layout.
 * @param[in] iImage The image handle.
 * @param[in] iOldLayout Previous image layout.
 * @param[in] iNewLayout New image layout to transition to.
 */
void transitionImageLayout(const VkImage& iImage, VkImageLayout iOldLayout, VkImageLayout iNewLayout);

/**
 * @brief
 *  Transition image layout.
 * @param[in] iCmd The command buffer.
 * @param[in] iImage The image handle.
 * @param[in] iOldLayout Previous image layout.
 * @param[in] iNewLayout New image layout to transition to.
 */
void transitionImageLayout(const VkCommandBuffer& iCmd, const VkImage& iImage, VkImageLayout iOldLayout,
						   VkImageLayout iNewLayout);

/**
 * @brief
 *  Copy buffer to image.
 * @param[in] iBuffer The buffer.
 * @param[in] iImage The image handle.
 * @param[in] iSize Target size.
 * @param[in] iOffset Offset in elements/bytes.
 */
void copyBufferToImage(const VkBuffer& iBuffer, const VkImage& iImage, const math::vec2ui& iSize,
					   const math::vec2i& iOffset = {0, 0});

/**
 * @brief
 *  Copy image to buffer.
 * @param[in] iImage The image handle.
 * @param[in] iBuffer The buffer.
 * @param[in] iSize Target size.
 * @param[in] iOffset Offset in elements/bytes.
 */
void copyImageToBuffer(const VkImage& iImage, const VkBuffer& iBuffer, const math::vec2ui& iSize,
					   const math::vec2i& iOffset = {0, 0});

/**
 * @brief
 *  Convert a 2D unsigned size into a Vulkan `VkExtent2D`.
 * @param[in] iSize Width / height in pixels.
 * @return The matching `VkExtent2D`.
 */
static constexpr auto toExtent(const math::vec2ui& iSize) -> VkExtent2D { return {iSize.x(), iSize.y()}; }

/**
 * @brief
 *  Convert a Vulkan `VkExtent2D` to a 2D unsigned size.
 * @param[in] iSize The Vulkan extent.
 * @return Width / height as `math::vec2ui`.
 */
static constexpr auto toSize(const VkExtent2D& iSize) -> math::vec2ui { return {iSize.width, iSize.height}; }
}// namespace owl::renderer::gpu::vulkan::internal
