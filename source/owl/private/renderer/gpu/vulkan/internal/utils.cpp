/**
 * @file utils.cpp
 * @author Silmaen
 * @date 19/03/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "utils.h"

#include "VulkanCore.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace owl::renderer::gpu::vulkan::internal {

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

auto createBuffer(const VkDeviceSize iSize, const VkBufferUsageFlags iUsage, const MemoryUsage iMemory,
				  const std::string_view iName) -> AllocatedBuffer {
	return MemoryAllocator::get().createBuffer(iSize, iUsage, iMemory, iName);
}

void freeBuffer(AllocatedBuffer& ioBuffer) { MemoryAllocator::get().destroyBuffer(ioBuffer); }

void writeMapped(const AllocatedBuffer& iBuffer, const void* iData, const size_t iSize, const size_t iOffset) {
	if (iBuffer.mapped == nullptr || iData == nullptr || iSize == 0 || iOffset + iSize > iBuffer.size)
		return;

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(static_cast<uint8_t*>(iBuffer.mapped) + iOffset, iData, iSize);
	OWL_DIAG_POP
}

void uploadToDeviceBuffer(VkBuffer iDestination, const void* iData, const VkDeviceSize iSize) {
	if (iDestination == nullptr || iData == nullptr || iSize == 0)
		return;
	auto staging = createBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUsage::Upload, "staging");
	if (staging.buffer == nullptr)
		return;
	writeMapped(staging, iData, iSize);
	copyBuffer(staging.buffer, iDestination, iSize);
	freeBuffer(staging);
}

namespace {
constexpr auto layoutToAccFlag(const VkImageLayout& iLayout) -> VkAccessFlags {
	if (iLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		return VK_ACCESS_NONE;
	if (iLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		return VK_ACCESS_NONE;
	if (iLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	if (iLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		return VK_ACCESS_TRANSFER_READ_BIT;
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

void transitionImageLayout(const VkImage& iImage, const VkImageLayout iOldLayout, const VkImageLayout iNewLayout,
						   const uint32_t iLevelCount) {
	const auto& core = VulkanCore::get();
	const auto& commandBuffer = core.beginSingleTimeCommands();
	transitionImageLayout(commandBuffer, iImage, iOldLayout, iNewLayout, iLevelCount);
	core.endSingleTimeCommands(commandBuffer);
}

void transitionImageLayout(const VkCommandBuffer& iCmd, const VkImage& iImage, const VkImageLayout iOldLayout,
						   const VkImageLayout iNewLayout, const uint32_t iLevelCount) {
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
															.levelCount = iLevelCount,
															.baseArrayLayer = 0,
															.layerCount = 1}};
	vkCmdPipelineBarrier(iCmd, layoutToStgFlag(iOldLayout), layoutToStgFlag(iNewLayout), 0, 0, nullptr, 0, nullptr, 1,
						 &barrier);
}

void generateMipmaps(const VkImage& iImage, const math::vec2ui& iSize, const uint32_t iLevelCount) {
	const auto& core = VulkanCore::get();
	const auto& commandBuffer = core.beginSingleTimeCommands();
	const auto levelBarrier = [&](const uint32_t iLevel, const VkImageLayout iOld, const VkImageLayout iNew) -> void {
		const VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
										   .pNext = nullptr,
										   .srcAccessMask = layoutToAccFlag(iOld),
										   .dstAccessMask = layoutToAccFlag(iNew),
										   .oldLayout = iOld,
										   .newLayout = iNew,
										   .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										   .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
										   .image = iImage,
										   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
																.baseMipLevel = iLevel,
																.levelCount = 1,
																.baseArrayLayer = 0,
																.layerCount = 1}};
		vkCmdPipelineBarrier(commandBuffer, layoutToStgFlag(iOld), layoutToStgFlag(iNew), 0, 0, nullptr, 0, nullptr, 1,
							 &barrier);
	};
	auto width = static_cast<int32_t>(iSize.x());
	auto height = static_cast<int32_t>(iSize.y());
	for (uint32_t level = 1; level < iLevelCount; ++level) {
		levelBarrier(level - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		const int32_t nextWidth = std::max(width / 2, 1);
		const int32_t nextHeight = std::max(height / 2, 1);
		const VkImageBlit blit{.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												  .mipLevel = level - 1,
												  .baseArrayLayer = 0,
												  .layerCount = 1},
							   .srcOffsets = {{0, 0, 0}, {width, height, 1}},
							   .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												  .mipLevel = level,
												  .baseArrayLayer = 0,
												  .layerCount = 1},
							   .dstOffsets = {{0, 0, 0}, {nextWidth, nextHeight, 1}}};
		vkCmdBlitImage(commandBuffer, iImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, iImage,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		levelBarrier(level - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		width = nextWidth;
		height = nextHeight;
	}
	levelBarrier(iLevelCount - 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	core.endSingleTimeCommands(commandBuffer);
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


}// namespace owl::renderer::gpu::vulkan::internal
