/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Framebuffer.h"

#include "GraphContext.h"
#include "app/Application.h"
#include "internal/Descriptors.h"
#include "internal/FrameProfiler.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"

#include <cstring>

namespace owl::renderer::gpu::vulkan {

namespace {
constexpr VkPipelineStageFlags g_attachmentStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
													VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
													VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
constexpr VkAccessFlags g_attachmentAccess =
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
constexpr VkAccessFlags g_attachmentWrites =
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

// Handles of a framebuffer size-dependent objects, destroyed together.
struct SizedObjects {
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkImageView> views;
	std::vector<VkSampler> samplers;
	std::vector<VkDescriptorSet> sets;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<internal::AllocatedImage> images;
	VkSwapchainKHR swapChain = nullptr;
};

void destroySized(SizedObjects& ioObjects) {
	auto* const device = internal::VulkanCore::get().getLogicalDevice();
	if (device == nullptr)
		return;
	for (auto* const framebuffer: ioObjects.framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
	if (!ioObjects.sets.empty()) {
		const auto& pool = internal::Descriptors::get().getSingleImageDescriptorPool();
		vkFreeDescriptorSets(device, pool, static_cast<uint32_t>(ioObjects.sets.size()), ioObjects.sets.data());
	}
	for (auto* const layout: ioObjects.setLayouts) vkDestroyDescriptorSetLayout(device, layout, nullptr);
	for (auto* const sampler: ioObjects.samplers) vkDestroySampler(device, sampler, nullptr);
	for (auto* const view: ioObjects.views) vkDestroyImageView(device, view, nullptr);
	for (auto& image: ioObjects.images) internal::MemoryAllocator::get().destroyImage(image);
	if (ioObjects.swapChain != nullptr)
		vkDestroySwapchainKHR(device, ioObjects.swapChain, nullptr);
	ioObjects = {};
}
}// namespace

Framebuffer::Framebuffer(FramebufferSpecification iSpec) : m_specs{std::move(iSpec)} {
	if (m_specs.samples > 1 && !isMainTarget()) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): only FrameBuffer for swapchain supports multiple sample.",
					   m_specs.debugName)
		return;
	}
	if (m_specs.samples < 1) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): sample must be at least one.", m_specs.debugName)
		return;
	}
	if (isMainTarget() && m_specs.attachments[0].format != AttachmentSpecification::Format::Surface) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): format of swap chain's first attachment is not 'Surface'.",
					   m_specs.debugName)
		return;
	}
	if (isMainTarget()) {
		OWL_CORE_INFO("Vulkan Framebuffer ({}): creation for swapchain use.", m_specs.debugName)
	} else {
		OWL_CORE_INFO("Vulkan Framebuffer ({}): creation for simple frame buffer use.", m_specs.debugName)
	}
	createRenderPass();
	invalidate();
}

Framebuffer::~Framebuffer() {
	deepCleanup();
	OWL_CORE_TRACE("Vulkan Framebuffer ({}): destroyed.", m_specs.debugName)
}

void Framebuffer::invalidate() {
	if (isMainTarget()) {
		const auto& vkc = internal::VulkanCore::get();
		internal::FrameProfiler::get().deviceWaitIdle(vkc.getLogicalDevice());
	}
	cleanup();
	createImages();
	createImageViews();
	createFrameBuffer();
	if (isMainTarget()) {
		releaseSyncObjects();
		createSyncObjects();
	} else {
		createDescriptorSets();
	}
	m_passSerial = 0;
	m_pickSerial.fill(0);
}

void Framebuffer::bind() {
	if (m_framebuffers.empty())
		return;
	internal::VulkanHandler::get().bindFramebuffer(this);
}

void Framebuffer::nextSubpass() {
	auto& vkh = internal::VulkanHandler::get();
	++m_currentSubPass;
	if (m_currentSubPass >= getSubpassCount()) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): subpass index out of range.", m_specs.debugName)
		--m_currentSubPass;
		return;
	}
	vkh.nextSubpass(true);
}

void Framebuffer::unbind() {
	auto& vkh = internal::VulkanHandler::get();
	vkh.unbindFramebuffer();
	if (isMainTarget())
		return;
	const bool needed = std::ranges::any_of(m_images, [](const Image& iImage) -> bool {
		return iImage.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	});
	if (needed)
		vkh.recordTransfer([this](VkCommandBuffer iCmd) -> void { prepareForSampling(iCmd); });
}

void Framebuffer::prepareForRendering(VkCommandBuffer iCmd) {
	if (isMainTarget())
		return;
	for (auto& img: m_images) {
		if (img.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || img.layout == VK_IMAGE_LAYOUT_UNDEFINED)
			continue;
		internal::imageBarrier(iCmd, img.image, VK_IMAGE_ASPECT_COLOR_BIT, img.layout,
							   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
							   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
									   VK_PIPELINE_STAGE_TRANSFER_BIT,
							   0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							   VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		img.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
}

void Framebuffer::prepareForSampling(VkCommandBuffer iCmd) {
	if (isMainTarget())
		return;
	for (auto& img: m_images) {
		if (img.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
			continue;
		internal::imageBarrier(iCmd, img.image, VK_IMAGE_ASPECT_COLOR_BIT, img.layout,
							   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
							   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							   VK_ACCESS_SHADER_READ_BIT);
		img.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
}

auto Framebuffer::acquirePassRenderPass(const uint64_t iFrameSerial) -> VkRenderPass {
	const bool first = m_passSerial != iFrameSerial;
	m_passSerial = iFrameSerial;
	return first ? m_firstRenderPass : m_renderPass;
}

void Framebuffer::resize(const math::vec2ui iSize) {
	if (m_specs.size == iSize)// nothing to do.
		return;
	m_specs.size = iSize;
	invalidate();
}

void Framebuffer::deepCleanup() {
	cleanup();
	releaseSyncObjects();
	const auto& vkc = internal::VulkanCore::get();
	if (vkc.getLogicalDevice() == nullptr)
		return;
	if (m_pickBuffer.buffer != nullptr)
		internal::releaseBuffer(m_pickBuffer);
	internal::VulkanHandler::get().deferRelease(
			[device = vkc.getLogicalDevice(), first = m_firstRenderPass, load = m_renderPass]() -> void {
				if (first != nullptr)
					vkDestroyRenderPass(device, first, nullptr);
				if (load != nullptr)
					vkDestroyRenderPass(device, load, nullptr);
			});
	m_firstRenderPass = nullptr;
	m_renderPass = nullptr;
}

void Framebuffer::cleanup() {
	SizedObjects objects;
	objects.framebuffers = std::move(m_framebuffers);
	m_framebuffers.clear();
	for (const Image& img: m_images) {
		if (img.descriptorSet != nullptr)
			objects.sets.push_back(img.descriptorSet);
		if (img.descriptorSetLayout != nullptr)
			objects.setLayouts.push_back(img.descriptorSetLayout);
		if (img.imageSampler != nullptr)
			objects.samplers.push_back(img.imageSampler);
		if (img.imageView != nullptr)
			objects.views.push_back(img.imageView);
		if (img.imageMemory != nullptr)
			objects.images.push_back({.image = img.image, .allocation = img.imageMemory});
	}
	m_images.clear();
	objects.swapChain = m_swapChain;
	m_swapChain = nullptr;
	// The swapchain is destroyed right away: the device is idle and a new one is created on the same surface.
	if (isMainTarget()) {
		destroySized(objects);
		return;
	}
	internal::VulkanHandler::get().deferRelease([objects]() mutable -> void { destroySized(objects); });
}

auto Framebuffer::readPixel(const uint32_t iAttachmentIndex, const int iX, const int iY) -> int {
	if (iAttachmentIndex >= m_specs.attachments.size() ||
		m_specs.attachments[iAttachmentIndex].format == AttachmentSpecification::Format::Depth24Stencil8 ||
		(isMainTarget() && iAttachmentIndex == 0)) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Cannot read a pixel of attachment {}.", m_specs.debugName,
					  iAttachmentIndex)
		return 0;
	}
	if (iX < 0 || iY < 0 || std::cmp_greater_equal(iX, m_specs.size.x()) ||
		std::cmp_greater_equal(iY, m_specs.size.y()))
		return m_lastPick;
	constexpr VkDeviceSize pixelSize = 4;
	if (m_pickBuffer.buffer == nullptr) {
		m_pickBuffer =
				internal::createBuffer(pixelSize * internal::g_maxFrameInFlight, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									   internal::MemoryUsage::Readback, "fb.pick:" + m_specs.debugName);
		if (m_pickBuffer.mapped == nullptr)
			return 0;
	}
	auto& vkh = internal::VulkanHandler::get();
	const uint32_t slot = vkh.isRecording() ? vkh.getCurrentFrameIndex() : 0;
	const auto* const slots = static_cast<const uint8_t*>(m_pickBuffer.mapped);
	auto& image = m_images[attToImgIdx(iAttachmentIndex)];
	const auto record = [&image, iX, iY, slot, this](VkCommandBuffer iCmd) -> void {
		const VkImageLayout layout = image.layout;
		internal::transitionImageLayout(iCmd, image.image, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		internal::copyImageToBuffer(iCmd, image.image, m_pickBuffer.buffer, {1, 1}, {iX, iY}, pixelSize * slot);
		internal::transitionImageLayout(iCmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);
	};
	if (!vkh.isRecording()) {
		vkh.submitNow(record);
		std::memcpy(&m_lastPick, slots, sizeof(m_lastPick));
		return m_lastPick;
	}
	// The pick of the previous frame of this slot is complete: its fence was waited when the frame started.
	if (m_pickSerial[slot] != 0) {
		std::memcpy(&m_lastPick, slots + pixelSize * slot, sizeof(m_lastPick));
		m_pickSerial[slot] = 0;
	}
	vkh.recordTransfer(record);
	m_pickSerial[slot] = vkh.getFrameSerial();
	return m_lastPick;
}

auto Framebuffer::readColorAttachment(const uint32_t iAttachmentIndex) -> std::vector<uint8_t> {
	if (iAttachmentIndex >= m_specs.attachments.size() || isMainTarget()) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): No offscreen attachment {} to read back.", m_specs.debugName,
					  iAttachmentIndex)
		return {};
	}
	const auto format = m_specs.attachments[iAttachmentIndex].format;
	if (format != AttachmentSpecification::Format::Surface && format != AttachmentSpecification::Format::Rgba8) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Attachment {} is not an 8-bit colour attachment.", m_specs.debugName,
					  iAttachmentIndex)
		return {};
	}
	const VkDeviceSize size = static_cast<VkDeviceSize>(m_specs.size.surface()) * 4;
	auto staging = internal::createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, internal::MemoryUsage::Readback,
										  "fb.readback");
	if (staging.mapped == nullptr) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): Failed to create the read-back buffer.", m_specs.debugName)
		return {};
	}
	auto& image = m_images[attToImgIdx(iAttachmentIndex)];
	internal::VulkanHandler::get().submitNow([&image, &staging, this](VkCommandBuffer iCmd) -> void {
		const VkImageLayout layout = image.layout;
		internal::transitionImageLayout(iCmd, image.image, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		internal::copyImageToBuffer(iCmd, image.image, staging.buffer, m_specs.size);
		internal::transitionImageLayout(iCmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);
	});
	std::vector<uint8_t> pixels(static_cast<size_t>(size));

	OWL_DIAG_PUSH
	OWL_DIAG_DISABLE_CLANG20("-Wunsafe-buffer-usage-in-libc-call")
	memcpy(pixels.data(), staging.mapped, pixels.size());
	OWL_DIAG_POP

	internal::freeBuffer(staging);
	if (const VkFormat vkFormat = internal::attachmentFormatToVulkan(format);
		vkFormat == VK_FORMAT_B8G8R8A8_UNORM || vkFormat == VK_FORMAT_B8G8R8A8_SRGB) {
		for (size_t i = 0; i + 3 < pixels.size(); i += 4) std::swap(pixels[i], pixels[i + 2]);
	}
	return pixels;
}

void Framebuffer::recordClear(const uint32_t iAttachmentIndex, const VkClearColorValue& iValue) {
	auto& vkh = internal::VulkanHandler::get();
	if (vkh.getCurrentFramebuffer() == this) {
		vkh.beginBatch();
		if (vkh.inBatch && m_currentSubPass != 0) {
			vkh.endBatch();
			vkh.beginBatch();
		}
	}
	if (vkh.inBatch && vkh.getCurrentFramebuffer() == this) {
		const VkClearAttachment attachment{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										   .colorAttachment = colorAttachmentIndex(iAttachmentIndex),
										   .clearValue = {.color = iValue}};
		const VkClearRect rect{.rect = {.offset = {0, 0}, .extent = internal::toExtent(m_specs.size)},
							   .baseArrayLayer = 0,
							   .layerCount = 1};
		vkCmdClearAttachments(vkh.getRenderPassCommandBuffer(), 1, &attachment, 1, &rect);
		return;
	}
	if (isMainTarget()) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): The swapchain is cleared in its render pass only.", m_specs.debugName)
		return;
	}
	auto& image = m_images[attToImgIdx(iAttachmentIndex)];
	vkh.recordTransfer([&image, &iValue](VkCommandBuffer iCmd) -> void {
		constexpr VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
												.baseMipLevel = 0,
												.levelCount = 1,
												.baseArrayLayer = 0,
												.layerCount = 1};
		const VkImageLayout layout =
				image.layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : image.layout;
		internal::transitionImageLayout(iCmd, image.image, image.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkCmdClearColorImage(iCmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &iValue, 1, &range);
		internal::transitionImageLayout(iCmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);
		image.layout = layout;
	});
}

void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const int iValue) {
	if (iAttachmentIndex >= m_specs.attachments.size() ||
		m_specs.attachments[iAttachmentIndex].format != AttachmentSpecification::Format::RedInteger) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Try to int-clear non integer attachment.", m_specs.debugName)
		return;
	}
	VkClearColorValue val{};
	val.int32[0] = iValue;
	recordClear(iAttachmentIndex, val);
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const math::vec4 iColorValue) {
	if (iAttachmentIndex >= m_specs.attachments.size() ||
		m_specs.attachments[iAttachmentIndex].format == AttachmentSpecification::Format::RedInteger ||
		m_specs.attachments[iAttachmentIndex].format == AttachmentSpecification::Format::Depth24Stencil8) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Try to color-clear non color attachment.", m_specs.debugName)
		return;
	}
	VkClearColorValue val{};
	val.float32[0] = iColorValue.r();
	val.float32[1] = iColorValue.g();
	val.float32[2] = iColorValue.b();
	val.float32[3] = iColorValue.a();
	recordClear(iAttachmentIndex, val);
}
OWL_DIAG_POP

void Framebuffer::createImages() {
	m_swapChainImageCount = 0;
	if (isMainTarget()) {
		const auto& vkc = internal::VulkanCore::get();
		auto* const gc = dynamic_cast<GraphContext*>(app::Application::get().getWindow().getGraphContext());
		m_swapChainImageCount = vkc.getImagecount();
		const auto queueFamilyIndices = vkc.getQueueIndices();
		const bool shares = queueFamilyIndices.size() > 1;
		const VkSurfaceFormatKHR surface = vkc.getSurfaceFormat();
		const VkSwapchainCreateInfoKHR createInfo{
				.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
				.pNext = nullptr,
				.flags = {},
				.surface = gc->getSurface(),
				.minImageCount = m_swapChainImageCount,
				.imageFormat = surface.format,
				.imageColorSpace = surface.colorSpace,
				.imageExtent = internal::toExtent(m_specs.size),
				.imageArrayLayers = 1,
				.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.imageSharingMode = shares ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = shares ? static_cast<uint32_t>(queueFamilyIndices.size()) : 0,
				.pQueueFamilyIndices = shares ? queueFamilyIndices.data() : nullptr,
				.preTransform = vkc.getCurrentTransform(),
				.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
				.presentMode = vkc.getPresentMode(),
				.clipped = VK_TRUE,
				.oldSwapchain = VK_NULL_HANDLE};
		if (const VkResult result = vkCreateSwapchainKHR(vkc.getLogicalDevice(), &createInfo, nullptr, &m_swapChain);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to create swap chain ({}).", m_specs.debugName,
						   internal::resultString(result))
			return;
		}

		vkGetSwapchainImagesKHR(vkc.getLogicalDevice(), m_swapChain, &m_swapChainImageCount, nullptr);
		m_images.resize(m_specs.attachments.size() - 1 + m_swapChainImageCount);
		std::vector<VkImage> temp(m_swapChainImageCount);
		vkGetSwapchainImagesKHR(vkc.getLogicalDevice(), m_swapChain, &m_swapChainImageCount, temp.data());
		for (uint32_t ii = 0; ii < m_swapChainImageCount; ++ii) {
			m_images[ii] = {};
			m_images[ii].image = temp[ii];
			m_images[ii].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
		OWL_CORE_INFO("Vulkan Framebuffer ({}): Created {} images for swapchain.", m_specs.debugName,
					  m_swapChainImageCount)
	} else
		m_images.resize(m_specs.attachments.size());
	// Create remaining Memory images
	std::vector<VkImage> colorImages;
	for (uint32_t i = m_swapChainImageCount; i < m_images.size(); ++i) {
		m_images[i] = {};
		m_images[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		const uint32_t attIndex = imgIdxToAtt(i);
		const bool isDepth = m_specs.attachments[attIndex].format == AttachmentSpecification::Format::Depth24Stencil8;
		const VkImageUsageFlags usage = isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
												: VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
														  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
														  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		const VkImageCreateInfo imageInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = nullptr,
				.flags = {},
				.imageType = VK_IMAGE_TYPE_2D,
				.format = internal::attachmentFormatToVulkan(m_specs.attachments[attIndex].format),
				.extent = {.width = m_specs.size.x(), .height = m_specs.size.y(), .depth = 1},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.tiling = internal::attachmentTilingToVulkan(m_specs.attachments[attIndex].tiling),
				.usage = usage,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = nullptr,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
		const auto allocated = internal::MemoryAllocator::get().createImage(imageInfo, "fb.image:" + m_specs.debugName);
		if (allocated.image == nullptr)
			return;
		m_images[i].image = allocated.image;
		m_images[i].imageMemory = allocated.allocation;
		if (!isDepth) {
			colorImages.push_back(allocated.image);
			m_images[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}
	// Colour images live in the attachment layout between passes: the render passes load them from it.
	internal::VulkanHandler::get().recordTransfer([&colorImages](VkCommandBuffer iCmd) -> void {
		for (auto* const image: colorImages)
			internal::transitionImageLayout(iCmd, image, VK_IMAGE_LAYOUT_UNDEFINED,
											VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	});
}

void Framebuffer::createImageViews() {
	const auto& vkc = internal::VulkanCore::get();
	for (uint32_t i = 0; i < m_images.size(); ++i) {
		const uint32_t attIndex = imgIdxToAtt(i);
		const VkImageViewCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = {},
				.image = m_images[i].image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = internal::attachmentFormatToVulkan(m_specs.attachments[attIndex].format),
				.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
							   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
				.subresourceRange = {.aspectMask =
											 internal::attachmentFormatToAspect(m_specs.attachments[attIndex].format),
									 .baseMipLevel = 0,
									 .levelCount = 1,
									 .baseArrayLayer = 0,
									 .layerCount = 1}};
		if (const VkResult result =
					vkCreateImageView(vkc.getLogicalDevice(), &createInfo, nullptr, &m_images[i].imageView);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): Error creating image views ({}).", m_specs.debugName,
						   internal::resultString(result))
		}
	}
}

void Framebuffer::createFrameBuffer() {
	const auto& vkc = internal::VulkanCore::get();
	m_framebuffers.resize(std::max(1u, m_swapChainImageCount));
	uint32_t idx = 0;
	for (auto& framebuffer: m_framebuffers) {
		std::vector<VkImageView> views = {m_images[idx].imageView};
		for (uint32_t iv = 1; iv < m_specs.attachments.size(); ++iv) {
			views.push_back(m_images[m_swapChainImageCount == 0 ? iv : m_swapChainImageCount - 1 + iv].imageView);
		}
		const VkFramebufferCreateInfo framebufferInfo{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
													  .pNext = nullptr,
													  .flags = {},
													  .renderPass = m_renderPass,
													  .attachmentCount = static_cast<uint32_t>(views.size()),
													  .pAttachments = views.data(),
													  .width = m_specs.size.x(),
													  .height = m_specs.size.y(),
													  .layers = 1};
		if (const VkResult result =
					vkCreateFramebuffer(vkc.getLogicalDevice(), &framebufferInfo, nullptr, &framebuffer);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): Error creating framebuffer {} ({}).", m_specs.debugName, idx,
						   internal::resultString(result))
		}
		++idx;
	}
}

void Framebuffer::createRenderPass() {
	m_clearValues.assign(m_specs.attachments.size(), VkClearValue{});
	for (size_t i = 0; i < m_specs.attachments.size(); ++i) {
		if (m_specs.attachments[i].format == AttachmentSpecification::Format::Depth24Stencil8)
			m_clearValues[i].depthStencil = {.depth = 1.0f, .stencil = 0};
	}
	m_renderPass = buildRenderPass(false);
	m_firstRenderPass = buildRenderPass(true);
}

auto Framebuffer::buildRenderPass(const bool iFirst) const -> VkRenderPass {
	const auto& vkc = internal::VulkanCore::get();
	std::vector<VkAttachmentReference> attRefs;
	uniq<VkAttachmentReference> depthRefs = nullptr;
	std::vector<VkAttachmentDescription> attDesc;
	uint32_t i = 0;
	for (const auto& [format, tiling]: m_specs.attachments) {
		if (format == AttachmentSpecification::Format::Depth24Stencil8) {
			depthRefs = mkUniq<VkAttachmentReference>(
					VkAttachmentReference{.attachment = i, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
			attDesc.push_back({.flags = {},
							   .format = internal::attachmentFormatToVulkan(format),
							   .samples = VK_SAMPLE_COUNT_1_BIT,
							   .loadOp = iFirst ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
							   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
							   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
							   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
							   .initialLayout = iFirst ? VK_IMAGE_LAYOUT_UNDEFINED
													   : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							   .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
		} else {
			attRefs.push_back({.attachment = i, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
			const bool presented = isMainTarget() && i == 0;
			// The swapchain is cleared by its first pass of a frame; off-screen colours keep their content.
			const bool cleared = iFirst && isMainTarget();
			VkImageLayout initial = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			if (presented)
				initial = iFirst ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			attDesc.push_back({.flags = {},
							   .format = internal::attachmentFormatToVulkan(format),
							   .samples = VK_SAMPLE_COUNT_1_BIT,
							   .loadOp = cleared ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
							   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
							   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
							   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
							   .initialLayout = initial,
							   .finalLayout = presented ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
														: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
		}
		++i;
	}
	constexpr VkAttachmentReference simpleAttachmentReference{.attachment = 0,
															  .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	const std::array subpasses = {VkSubpassDescription{.flags = {},
													   .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
													   .inputAttachmentCount = 0,
													   .pInputAttachments = nullptr,
													   .colorAttachmentCount = static_cast<uint32_t>(attRefs.size()),
													   .pColorAttachments = attRefs.data(),
													   .pResolveAttachments = nullptr,
													   .pDepthStencilAttachment = depthRefs.get(),
													   .preserveAttachmentCount = 0,
													   .pPreserveAttachments = nullptr},
								  VkSubpassDescription{.flags = {},
													   .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
													   .inputAttachmentCount = 0,
													   .pInputAttachments = nullptr,
													   .colorAttachmentCount = 1,
													   .pColorAttachments = &simpleAttachmentReference,
													   .pResolveAttachments = nullptr,
													   .pDepthStencilAttachment = nullptr,
													   .preserveAttachmentCount = 0,
													   .pPreserveAttachments = nullptr}};
	// Earlier passes on the same attachments (same frame command buffer) complete before this one loads them.
	const std::array dependencies = {VkSubpassDependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
														 .dstSubpass = 0,
														 .srcStageMask = g_attachmentStages,
														 .dstStageMask = g_attachmentStages,
														 .srcAccessMask = g_attachmentWrites,
														 .dstAccessMask = g_attachmentAccess,
														 .dependencyFlags = {}},
									 VkSubpassDependency{.srcSubpass = 0,
														 .dstSubpass = 1,
														 .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
														 .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
														 .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
																		  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
														 .dependencyFlags = {}}};
	const VkRenderPassCreateInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
												.pNext = nullptr,
												.flags = {},
												.attachmentCount = static_cast<uint32_t>(attDesc.size()),
												.pAttachments = attDesc.data(),
												.subpassCount = static_cast<uint32_t>(subpasses.size()),
												.pSubpasses = subpasses.data(),
												.dependencyCount = static_cast<uint32_t>(dependencies.size()),
												.pDependencies = dependencies.data()};
	VkRenderPass renderPass = nullptr;
	if (const VkResult result = vkCreateRenderPass(vkc.getLogicalDevice(), &renderPassInfo, nullptr, &renderPass);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create render pass ({}).", m_specs.debugName,
					   internal::resultString(result))
		return nullptr;
	}
	return renderPass;
}

auto Framebuffer::isMainTarget() const -> bool { return m_specs.swapChainTarget; }

auto Framebuffer::hasDepth() const -> bool {
	return std::ranges::any_of(m_specs.attachments, [](const AttachmentSpecification& iAttachment) -> bool {
		return iAttachment.format == AttachmentSpecification::Format::Depth24Stencil8;
	});
}

auto Framebuffer::colorAttachmentIndex(const uint32_t iAttachmentIndex) const -> uint32_t {
	uint32_t index = 0;
	for (uint32_t i = 0; i < iAttachmentIndex && i < m_specs.attachments.size(); ++i) {
		if (m_specs.attachments[i].format != AttachmentSpecification::Format::Depth24Stencil8)
			++index;
	}
	return index;
}

auto Framebuffer::getSubpassCount() const -> uint32_t { return 2; }

void Framebuffer::createSyncObjects() {
	constexpr VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = {}};
	const auto& core = internal::VulkanCore::get();
	m_renderFinished.assign(m_swapChainImageCount, nullptr);
	for (auto& semaphore: m_renderFinished) {
		if (const VkResult result = vkCreateSemaphore(core.getLogicalDevice(), &semaphoreInfo, nullptr, &semaphore);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create render finish semaphore ({}).", m_specs.debugName,
						   internal::resultString(result))
		}
	}
}

void Framebuffer::releaseSyncObjects() {
	const auto& core = internal::VulkanCore::get();
	if (core.getLogicalDevice() != nullptr) {
		for (auto* const semaphore: m_renderFinished) {
			if (semaphore != nullptr)
				vkDestroySemaphore(core.getLogicalDevice(), semaphore, nullptr);
		}
	}
	m_renderFinished.clear();
}

auto Framebuffer::attToImgIdx(const uint32_t iAttachmentIndex) const -> uint32_t {
	uint32_t imgIndex = iAttachmentIndex;
	if (m_swapChainImageCount > 0) {
		if (iAttachmentIndex == 0)
			imgIndex = m_currentImage;
		else
			imgIndex = m_swapChainImageCount - 1 + iAttachmentIndex;
	}
	return imgIndex;
}

auto Framebuffer::imgIdxToAtt(const uint32_t iImageIndex) const -> uint32_t {
	uint32_t attachmentIndex = iImageIndex;
	if (m_swapChainImageCount > 0) {
		if (iImageIndex < m_swapChainImageCount)
			return 0;
		attachmentIndex = iImageIndex - m_swapChainImageCount + 1;
	}
	return attachmentIndex;
}

auto Framebuffer::getColorAttachmentFormats() const -> std::vector<VkFormat> {
	std::vector<VkFormat> formats;
	for (const auto att: m_specs.attachments) {
		if (att.format != AttachmentSpecification::Format::Depth24Stencil8)
			formats.push_back(internal::attachmentFormatToVulkan(att.format));
	}
	return formats;
}

auto Framebuffer::getColorAttachmentRendererId(const uint32_t iIndex) const -> uint64_t {
	return reinterpret_cast<uint64_t>(m_images[imgIdxToAtt(iIndex)].descriptorSet);
}

void Framebuffer::createDescriptorSets() {
	const auto& pool = internal::Descriptors::get().getSingleImageDescriptorPool();
	const auto& core = internal::VulkanCore::get();
	const VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = {},
										  .magFilter = VK_FILTER_LINEAR,
										  .minFilter = VK_FILTER_LINEAR,
										  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
										  .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
										  .mipLodBias = {},
										  .anisotropyEnable = VK_TRUE,
										  .maxAnisotropy = core.getMaxSamplerAnisotropy(),
										  .compareEnable = VK_FALSE,
										  .compareOp = VK_COMPARE_OP_ALWAYS,
										  .minLod = -1000,
										  .maxLod = 1000,
										  .borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
										  .unnormalizedCoordinates = VK_FALSE};
	// ImGui >= 1.92.9 binds its own sampler: the texture id is a sampled-image set.
	static constexpr VkDescriptorSetLayoutBinding imageLayoutBinding{.binding = 0,
																	 .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
																	 .descriptorCount = 1,
																	 .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
																	 .pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutCreateInfo layoutCi{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													   .pNext = nullptr,
													   .flags = {},
													   .bindingCount = 1,
													   .pBindings = &imageLayoutBinding};
	for (uint32_t imgIdx = 0; imgIdx < m_images.size(); ++imgIdx) {
		if (m_specs.attachments[imgIdxToAtt(imgIdx)].format == AttachmentSpecification::Format::Depth24Stencil8)
			continue;
		auto& img = m_images[imgIdx];
		if (const VkResult result = vkCreateSampler(core.getLogicalDevice(), &samplerInfo, nullptr, &img.imageSampler);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Texture: Error creating texture sampler ({}).", internal::resultString(result))
		}
		if (const auto result =
					vkCreateDescriptorSetLayout(core.getLogicalDevice(), &layoutCi, nullptr, &img.descriptorSetLayout);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to create descriptor set layout ({}).",
						   internal::resultString(result))
		}
		const VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
													.pNext = nullptr,
													.descriptorPool = pool,
													.descriptorSetCount = 1,
													.pSetLayouts = &img.descriptorSetLayout};
		if (const auto result = vkAllocateDescriptorSets(core.getLogicalDevice(), &allocInfo, &img.descriptorSet);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to allocate descriptor sets ({}).",
						   internal::resultString(result))
		}
		const VkDescriptorImageInfo info{.sampler = img.imageSampler,
										 .imageView = img.imageView,
										 .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		const VkWriteDescriptorSet wrt{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
									   .pNext = nullptr,
									   .dstSet = img.descriptorSet,
									   .dstBinding = 0,
									   .dstArrayElement = 0,
									   .descriptorCount = 1,
									   .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
									   .pImageInfo = &info,
									   .pBufferInfo = nullptr,
									   .pTexelBufferView = nullptr};
		vkUpdateDescriptorSets(core.getLogicalDevice(), 1, &wrt, 0, nullptr);
	}
}


}// namespace owl::renderer::gpu::vulkan
