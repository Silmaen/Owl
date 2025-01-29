/**
 * @file utils.cpp
 * @author Silmaen
 * @date 19/03/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "utils.h"

#include "VulkanCore.h"

namespace owl::renderer::vulkan::internal {

auto attachmentFormatToVulkan(const AttachmentSpecification::Format& iFormat) -> VkFormat {
	switch (iFormat) {
		case AttachmentSpecification::Format::None:
			return VK_FORMAT_UNDEFINED;
		case AttachmentSpecification::Format::Depth24Stencil8:
			return VK_FORMAT_D24_UNORM_S8_UINT;
		case AttachmentSpecification::Format::Rgba8:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case AttachmentSpecification::Format::RedInteger:
			return VK_FORMAT_R32_SINT;
		case AttachmentSpecification::Format::Surface:
			return VulkanCore::get().getSurfaceFormat().format;
	}
	return VK_FORMAT_UNDEFINED;
}

auto attachmentFormatToAspect(const AttachmentSpecification::Format& iFormat) -> VkImageAspectFlags {
	switch (iFormat) {
		case AttachmentSpecification::Format::None:
		case AttachmentSpecification::Format::Rgba8:
		case AttachmentSpecification::Format::RedInteger:
		case AttachmentSpecification::Format::Surface:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		case AttachmentSpecification::Format::Depth24Stencil8:
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	return VK_IMAGE_ASPECT_COLOR_BIT;
}

auto attachmentFormatToSize(const AttachmentSpecification::Format& iFormat) -> uint32_t {
	switch (iFormat) {
		case AttachmentSpecification::Format::Depth24Stencil8:
		case AttachmentSpecification::Format::Rgba8:
		case AttachmentSpecification::Format::RedInteger:
		case AttachmentSpecification::Format::Surface:
			return 4;
		case AttachmentSpecification::Format::None:
			return 1;
	}
	return 1;
}

auto attachmentTilingToVulkan(const AttachmentSpecification::Tiling& iTiling) -> VkImageTiling {
	switch (iTiling) {
		case AttachmentSpecification::Tiling::Linear:
			return VK_IMAGE_TILING_LINEAR;
		case AttachmentSpecification::Tiling::Optimal:
			return VK_IMAGE_TILING_OPTIMAL;
	}
	return VK_IMAGE_TILING_OPTIMAL;
}


void copyBuffer(const VkBuffer& iSrcBuffer, const VkBuffer& iDstBuffer, const VkDeviceSize iSize) {
	const auto& core = VulkanCore::get();
	const VkCommandBuffer& commandBuffer = core.beginSingleTimeCommands();
	VkBufferCopy copyRegion{};
	copyRegion.size = iSize;
	vkCmdCopyBuffer(commandBuffer, iSrcBuffer, iDstBuffer, 1, &copyRegion);
	core.endSingleTimeCommands(commandBuffer);
}

void createBuffer(const VkDeviceSize iSize, const VkBufferUsageFlags iUsage, const VkMemoryPropertyFlags iProperties,
				  VkBuffer& iBuffer, VkDeviceMemory& iBufferMemory) {
	const auto& core = VulkanCore::get();
	const VkBufferCreateInfo bufferInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
										.pNext = nullptr,
										.flags = {},
										.size = iSize,
										.usage = iUsage,
										.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
										.queueFamilyIndexCount = 0,
										.pQueueFamilyIndices = nullptr};

	if (const VkResult result = vkCreateBuffer(core.getLogicalDevice(), &bufferInfo, nullptr, &iBuffer);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan buffer: failed to create vertex buffer ({}).", resultString(result))
		return;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(core.getLogicalDevice(), iBuffer, &memRequirements);

	const VkMemoryAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .allocationSize = memRequirements.size,
										 .memoryTypeIndex =
												 core.findMemoryTypeIndex(memRequirements.memoryTypeBits, iProperties)};

	if (const VkResult result = vkAllocateMemory(core.getLogicalDevice(), &allocInfo, nullptr, &iBufferMemory);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan vertex buffer: failed to allocate memory buffer ({}).", resultString(result))
		return;
	}

	if (const VkResult result = vkBindBufferMemory(core.getLogicalDevice(), iBuffer, iBufferMemory, 0);
		result != VK_SUCCESS)
		OWL_CORE_ERROR("Vulkan vertex buffer: failed to bind memory buffer ({}).", resultString(result))
}

void freeBuffer(const VkDevice& iDevice, const VkBuffer& iBuffer, const VkDeviceMemory& iBufferMemory) {
	vkFreeMemory(iDevice, iBufferMemory, nullptr);
	vkDestroyBuffer(iDevice, iBuffer, nullptr);
}

namespace {
constexpr auto layoutToAccFlag(const VkImageLayout& iLayout) -> VkAccessFlags {
	if (iLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		return VK_ACCESS_NONE;
	if (iLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		return VK_ACCESS_NONE;
	if (iLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		return VK_ACCESS_SHADER_READ_BIT;
	return VK_ACCESS_NONE;
}

constexpr auto layoutToStgFlag(const VkImageLayout& iLayout) -> VkPipelineStageFlags {
	if (iLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || iLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		return VK_PIPELINE_STAGE_TRANSFER_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	return VK_PIPELINE_STAGE_NONE;
}
}// namespace

void transitionImageLayout(const VkImage& iImage, const VkImageLayout iOldLayout, const VkImageLayout iNewLayout) {
	const auto& core = VulkanCore::get();
	const auto& commandBuffer = core.beginSingleTimeCommands();
	transitionImageLayout(commandBuffer, iImage, iOldLayout, iNewLayout);
	core.endSingleTimeCommands(commandBuffer);
}

void transitionImageLayout(const VkCommandBuffer& iCmd, const VkImage& iImage, const VkImageLayout iOldLayout,
						   const VkImageLayout iNewLayout) {
	const VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
									   .pNext = nullptr,
									   .srcAccessMask = layoutToAccFlag(iOldLayout),
									   .dstAccessMask = layoutToAccFlag(iNewLayout),
									   .oldLayout = iOldLayout,
									   .newLayout = iNewLayout,
									   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									   .image = iImage,
									   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
															.baseMipLevel = 0,
															.levelCount = 1,
															.baseArrayLayer = 0,
															.layerCount = 1}};
	vkCmdPipelineBarrier(iCmd, layoutToStgFlag(iOldLayout), layoutToStgFlag(iNewLayout), 0, 0, nullptr, 0, nullptr, 1,
						 &barrier);
}

void copyBufferToImage(const VkBuffer& iBuffer, const VkImage& iImage, const math::vec2ui& iSize,
					   const math::vec2i& iOffset) {
	const auto& core = VulkanCore::get();
	const auto& commandBuffer = core.beginSingleTimeCommands();
	const VkBufferImageCopy region{.bufferOffset = 0,
								   .bufferRowLength = 0,
								   .bufferImageHeight = 0,
								   .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
														.mipLevel = 0,
														.baseArrayLayer = 0,
														.layerCount = 1},
								   .imageOffset = {iOffset.x(), iOffset.y(), 0},
								   .imageExtent = {iSize.x(), iSize.y(), 1}};
	vkCmdCopyBufferToImage(commandBuffer, iBuffer, iImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	core.endSingleTimeCommands(commandBuffer);
}

void copyImageToBuffer(const VkImage& iImage, const VkBuffer& iBuffer, const math::vec2ui& iSize,
					   const math::vec2i& iOffset) {
	const auto& core = VulkanCore::get();
	const auto& commandBuffer = core.beginSingleTimeCommands();
	const VkBufferImageCopy region{.bufferOffset = 0,
								   .bufferRowLength = 0,
								   .bufferImageHeight = 0,
								   .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
														.mipLevel = 0,
														.baseArrayLayer = 0,
														.layerCount = 1},
								   .imageOffset = {iOffset.x(), iOffset.y(), 0},
								   .imageExtent = {iSize.x(), iSize.y(), 1}};
	vkCmdCopyImageToBuffer(commandBuffer, iImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, iBuffer, 1, &region);
	core.endSingleTimeCommands(commandBuffer);
}


}// namespace owl::renderer::vulkan::internal
