/**
 * @file Framebuffer.cpp
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "Framebuffer.h"

#include "GraphContext.h"
#include "core/Application.h"
#include "internal/Descriptors.h"
#include "internal/VulkanCore.h"
#include "internal/VulkanHandler.h"
#include "internal/utils.h"

namespace owl::renderer::vulkan {

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
	// Render Pass creation.
	createRenderPass();
	invalidate();
	m_samples.resize(m_specs.samples);
	createCommandBuffers();
	createSyncObjects();
}

Framebuffer::~Framebuffer() {
	deepCleanup();
	OWL_CORE_TRACE("Vulkan Framebuffer ({}): destroyed.", m_specs.debugName)
}

void Framebuffer::invalidate() {
	const auto& vkc = internal::VulkanCore::get();
	vkDeviceWaitIdle(vkc.getLogicalDevice());
	cleanup();

	createImages();
	createImageViews();
	createFrameBuffer();
	if (!isMainTarget())
		createDescriptorSets();
}

void Framebuffer::bind() {
	if (m_framebuffers.empty())
		return;
	//const auto& core = internal::VulkanCore::get();
	auto& vkh = internal::VulkanHandler::get();
	resetBatch();
	//vkWaitForFences(core.getLogicalDevice(), 1, getCurrentFence(), VK_TRUE, UINT64_MAX);
	if (!isMainTarget()) {
		for (auto& img: m_images) {
			internal::transitionImageLayout(img.image, VK_IMAGE_LAYOUT_UNDEFINED,
											VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}
	}
	vkh.bindFramebuffer(this);
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
	if (!isMainTarget()) {
		for (auto& img: m_images) {
			internal::transitionImageLayout(img.image, VK_IMAGE_LAYOUT_UNDEFINED,
											VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}
	vkh.unbindFramebuffer();
}

void Framebuffer::resize(const math::vec2ui iSize) {
	if (m_specs.size == iSize)// nothing to do.
		return;
	m_specs.size = iSize;
	invalidate();
}

void Framebuffer::deepCleanup() {
	const auto& vkc = internal::VulkanCore::get();
	for (auto& [imgAvail, renderFinish, fence, cmd]: m_samples) {
		if (renderFinish != nullptr) {
			vkDestroySemaphore(vkc.getLogicalDevice(), renderFinish, nullptr);
			renderFinish = nullptr;
		}
		if (imgAvail != nullptr)
			vkDestroySemaphore(vkc.getLogicalDevice(), imgAvail, nullptr);
		imgAvail = nullptr;
		if (fence != nullptr)
			vkDestroyFence(vkc.getLogicalDevice(), fence, nullptr);
		fence = nullptr;
	}
	m_samples.clear();
	cleanup();
	// Cleanup render pass
	if (m_renderPass != nullptr) {
		vkDestroyRenderPass(vkc.getLogicalDevice(), m_renderPass, nullptr);
		m_renderPass = nullptr;
	}
}

void Framebuffer::cleanup() {
	const auto& vkc = internal::VulkanCore::get();
	// Cleanup framebuffer
	for (const auto& framebuffer: m_framebuffers) {
		if (framebuffer != nullptr)
			vkDestroyFramebuffer(vkc.getLogicalDevice(), framebuffer, nullptr);
	}
	m_framebuffers.clear();
	const auto& pool = internal::Descriptors::get().getSingleImageDescriptorPool();
	vkDeviceWaitIdle(vkc.getLogicalDevice());
	for (auto& [image, memory, view, sampler, dSet, dSetLayout]: m_images) {
		if (dSet != nullptr)
			vkFreeDescriptorSets(vkc.getLogicalDevice(), pool, 1, &dSet);
		if (dSetLayout != nullptr)
			vkDestroyDescriptorSetLayout(vkc.getLogicalDevice(), dSetLayout, nullptr);
		if (sampler != nullptr)
			vkDestroySampler(vkc.getLogicalDevice(), sampler, nullptr);
		if (view != nullptr)
			vkDestroyImageView(vkc.getLogicalDevice(), view, nullptr);
		if (memory != nullptr) {
			vkFreeMemory(vkc.getLogicalDevice(), memory, nullptr);
			if (image != nullptr)
				vkDestroyImage(vkc.getLogicalDevice(), image, nullptr);
		}
	}
	m_images.clear();
	if (m_swapChain != nullptr) {
		vkDestroySwapchainKHR(vkc.getLogicalDevice(), m_swapChain, nullptr);
		m_swapChain = nullptr;
	}
}

auto Framebuffer::readPixel(const uint32_t iAttachmentIndex, const int iX, const int iY) -> int {
	const auto& vkc = internal::VulkanCore::get();
	const auto& [format, tiling] = m_specs.attachments[iAttachmentIndex];
	const uint32_t imgIndex = attToImgIdx(iAttachmentIndex);
	VkBuffer stagingBuffer = nullptr;
	{
		const VkBufferCreateInfo bufferCi{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = {},
										  .size = internal::attachmentFormatToSize(format),
										  .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										  .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
										  .queueFamilyIndexCount = 0,
										  .pQueueFamilyIndices = nullptr};
		if (const VkResult result = vkCreateBuffer(vkc.getLogicalDevice(), &bufferCi, nullptr, &stagingBuffer);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to create staging buffer ({}).", m_specs.debugName,
						   internal::resultString(result))
			return 0;
		}
	}
	VkDeviceMemory stagingBufferMemory = nullptr;
	{
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(vkc.getLogicalDevice(), stagingBuffer, &memRequirements);
		const VkMemoryAllocateInfo allocInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = memRequirements.size,
				.memoryTypeIndex = vkc.findMemoryTypeIndex(memRequirements.memoryTypeBits,
														   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
																   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
		if (const VkResult result = vkAllocateMemory(vkc.getLogicalDevice(), &allocInfo, nullptr, &stagingBufferMemory);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to allocate staging buffer memory ({}).", m_specs.debugName,
						   internal::resultString(result))
			vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
			return 0;
		}
		if (const VkResult result = vkBindBufferMemory(vkc.getLogicalDevice(), stagingBuffer, stagingBufferMemory, 0);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to bind staging buffer memory ({}).", m_specs.debugName,
						   internal::resultString(result))
			vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
			vkFreeMemory(vkc.getLogicalDevice(), stagingBufferMemory, nullptr);
			return 0;
		}
	}
	internal::transitionImageLayout(m_images[imgIndex].image, VK_IMAGE_LAYOUT_UNDEFINED,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	internal::copyImageToBuffer(m_images[imgIndex].image, stagingBuffer, {1, 1}, {iX, iY});
	internal::transitionImageLayout(m_images[imgIndex].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
												   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	void* data = nullptr;
	if (const VkResult result = vkMapMemory(vkc.getLogicalDevice(), stagingBufferMemory, 0,
											internal::attachmentFormatToSize(format), 0, &data);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to map memory ({}).", m_specs.debugName,
					   internal::resultString(result))
		vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
		vkFreeMemory(vkc.getLogicalDevice(), stagingBufferMemory, nullptr);
		return 0;
	}
	int32_t pixel = 0;
	memcpy(&pixel, data, internal::attachmentFormatToSize(format));
	vkDestroyBuffer(vkc.getLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(vkc.getLogicalDevice(), stagingBufferMemory, nullptr);
	return pixel;
}

void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const int iValue) {
	if (m_specs.attachments[iAttachmentIndex].format != AttachmentSpecification::Format::RedInteger) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Try to int-clear non integer attachment.", m_specs.debugName)
		return;
	}
	const auto& core = internal::VulkanCore::get();
	auto* const cmd = core.beginSingleTimeCommands();
	const uint32_t imgIndex = attToImgIdx(iAttachmentIndex);
	VkClearColorValue val;
	val.int32[0] = iValue;
	constexpr VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
											.baseMipLevel = 0,
											.levelCount = 1,
											.baseArrayLayer = 0,
											.layerCount = 1};
	internal::transitionImageLayout(cmd, m_images[imgIndex].image, VK_IMAGE_LAYOUT_UNDEFINED,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdClearColorImage(cmd, m_images[imgIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &val, 1, &range);

	internal::transitionImageLayout(cmd, m_images[imgIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
												   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	core.endSingleTimeCommands(cmd);
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG16("-Wunsafe-buffer-usage")
void Framebuffer::clearAttachment(const uint32_t iAttachmentIndex, const math::vec4 iColorValue) {
	if (m_specs.attachments[iAttachmentIndex].format == AttachmentSpecification::Format::RedInteger) {
		OWL_CORE_WARN("Vulkan Framebuffer ({}): Try to color-clear non color attachment.", m_specs.debugName)
		return;
	}
	const auto& core = internal::VulkanCore::get();
	auto* const cmd = core.beginSingleTimeCommands();
	const uint32_t imgIndex = attToImgIdx(iAttachmentIndex);
	VkClearColorValue val;
	val.float32[0] = iColorValue.r();
	val.float32[1] = iColorValue.g();
	val.float32[2] = iColorValue.b();
	val.float32[3] = iColorValue.a();
	constexpr VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
											.baseMipLevel = 0,
											.levelCount = 1,
											.baseArrayLayer = 0,
											.layerCount = 1};
	internal::transitionImageLayout(m_images[imgIndex].image, VK_IMAGE_LAYOUT_UNDEFINED,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdClearColorImage(cmd, m_images[imgIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &val, 1, &range);
	core.endSingleTimeCommands(cmd);
	internal::transitionImageLayout(m_images[imgIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
												   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}
OWL_DIAG_POP

void Framebuffer::createImages() {
	const auto& vkc = internal::VulkanCore::get();
	m_swapChainImageCount = 0;
	if (isMainTarget()) {
		auto* const gc = dynamic_cast<GraphContext*>(core::Application::get().getWindow().getGraphContext());
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
			m_images[ii].image = temp[ii];
			internal::transitionImageLayout(temp[ii], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		}
		OWL_CORE_INFO("Vulkan Framebuffer ({}): Created {} images for swapchain", m_specs.debugName,
					  m_swapChainImageCount)
	} else
		m_images.resize(m_specs.attachments.size());
	// Create remaining Memory images
	for (uint32_t i = m_swapChainImageCount; i < m_images.size(); ++i) {
		const uint32_t attIndex = imgIdxToAtt(i);
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
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
				.queueFamilyIndexCount = 1,
				.pQueueFamilyIndices = nullptr,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
		if (const VkResult result = vkCreateImage(vkc.getLogicalDevice(), &imageInfo, nullptr, &m_images[i].image);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to create image ({}).", m_specs.debugName,
						   internal::resultString(result))
			return;
		}
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(vkc.getLogicalDevice(), m_images[i].image, &memRequirements);
		const VkMemoryAllocateInfo allocInfo{
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = nullptr,
				.allocationSize = memRequirements.size,
				.memoryTypeIndex =
						vkc.findMemoryTypeIndex(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
		if (const VkResult result =
					vkAllocateMemory(vkc.getLogicalDevice(), &allocInfo, nullptr, &m_images[i].imageMemory);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to allocate image memory ({}).", m_specs.debugName,
						   internal::resultString(result))
			return;
		}
		if (const VkResult result =
					vkBindImageMemory(vkc.getLogicalDevice(), m_images[i].image, m_images[i].imageMemory, 0);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan Framebuffer ({}): failed to bind image memory ({}).", m_specs.debugName,
						   internal::resultString(result))
			return;
		}
		internal::transitionImageLayout(m_images[i].image, VK_IMAGE_LAYOUT_UNDEFINED,
										isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
													   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
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
	const auto& vkc = internal::VulkanCore::get();
	std::vector<VkAttachmentReference> attRefs;
	uniq<VkAttachmentReference> depthRefs = nullptr;
	std::vector<VkAttachmentDescription> attDesc;

	uint32_t i = 0;
	for (const auto& [format, tiling]: m_specs.attachments) {
		if (format == AttachmentSpecification::Format::Depth24Stencil8) {
			depthRefs = mkUniq<VkAttachmentReference>(
					VkAttachmentReference{.attachment = i, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
		} else {
			attRefs.push_back({.attachment = i, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
		}
		attDesc.push_back({.flags = {},
						   .format = internal::attachmentFormatToVulkan(format),
						   .samples = VK_SAMPLE_COUNT_1_BIT,
						   .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
						   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
						   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
						   .initialLayout = isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
														   : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						   .finalLayout = isMainTarget() ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
														 : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
		++i;
	}
	constexpr VkAttachmentReference simpleAttachmentReference{.attachment = 0,
															  .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	std::vector subpasses = {// SubPass 0: the scene
							 VkSubpassDescription{.flags = {},
												  .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
												  .inputAttachmentCount = 0,
												  .pInputAttachments = nullptr,
												  .colorAttachmentCount = static_cast<uint32_t>(attRefs.size()),
												  .pColorAttachments = attRefs.data(),
												  .pResolveAttachments = nullptr,
												  .pDepthStencilAttachment = depthRefs.get(),
												  .preserveAttachmentCount = 0,
												  .pPreserveAttachments = nullptr}};
	//if (isMainTarget()) {
	// SubPass 1: the GUI
	subpasses.emplace_back(VkSubpassDescription{.flags = {},
												.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
												.inputAttachmentCount = 0,
												.pInputAttachments = nullptr,
												.colorAttachmentCount = 1,
												.pColorAttachments = &simpleAttachmentReference,
												.pResolveAttachments = nullptr,
												.pDepthStencilAttachment = nullptr,
												.preserveAttachmentCount = 0,
												.pPreserveAttachments = nullptr});
	//}
	std::vector dependencies = {VkSubpassDependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
													.dstSubpass = 0,
													.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
													.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
													.srcAccessMask = 0,
													.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
													.dependencyFlags = {}}};
	//if (isMainTarget()) {
	dependencies.emplace_back(VkSubpassDependency{.srcSubpass = 0,
												  .dstSubpass = 1,
												  .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
												  .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
												  .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
												  .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
												  .dependencyFlags = {}});
	//}
	m_SubPassCount = static_cast<uint32_t>(subpasses.size());
	const VkRenderPassCreateInfo renderPassInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
												.pNext = nullptr,
												.flags = {},
												.attachmentCount = static_cast<uint32_t>(attDesc.size()),
												.pAttachments = attDesc.data(),
												.subpassCount = static_cast<uint32_t>(subpasses.size()),
												.pSubpasses = subpasses.data(),
												.dependencyCount = static_cast<uint32_t>(dependencies.size()),
												.pDependencies = dependencies.data()};
	if (const VkResult result = vkCreateRenderPass(vkc.getLogicalDevice(), &renderPassInfo, nullptr, &m_renderPass);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create render pass ({})", m_specs.debugName,
					   internal::resultString(result))
	}
}

auto Framebuffer::isMainTarget() const -> bool { return m_specs.swapChainTarget; }

auto Framebuffer::hasBeenCalled() const -> bool { return m_called || m_specs.swapChainTarget; }

auto Framebuffer::getSubpassCount() const -> uint32_t {
	return m_SubPassCount;
	//return isMainTarget() ? 2 : 1;
}

void Framebuffer::nextFrame() { m_currentFrame = (m_currentFrame + 1) % m_specs.samples; }

void Framebuffer::createCommandBuffers() {
	const auto& core = internal::VulkanCore::get();
	for (auto& [imgAvail, renderFinish, fence, cmd]: m_samples) cmd = core.createCommandBuffer();
}

void Framebuffer::createSyncObjects() {
	constexpr VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = {}};
	constexpr VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = VK_FENCE_CREATE_SIGNALED_BIT};

	const auto& core = internal::VulkanCore::get();
	uint32_t idx = 0;
	for (auto& [imgAvail, renderFinish, fence, cmd]: m_samples) {
		if (isMainTarget()) {
			if (const VkResult result = vkCreateSemaphore(core.getLogicalDevice(), &semaphoreInfo, nullptr, &imgAvail);
				result != VK_SUCCESS) {
				OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create image available semaphore {} ({}).",
							   m_specs.debugName, idx, internal::resultString(result))
			}
		}
		if (const VkResult result = vkCreateSemaphore(core.getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinish);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create render finish semaphore {} ({}).",
						   m_specs.debugName, idx, internal::resultString(result))
		}
		if (const VkResult result = vkCreateFence(core.getLogicalDevice(), &fenceInfo, nullptr, &fence);
			result != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan framebuffer ({}): failed to create synchronization in flight fence {} ({}).",
						   m_specs.debugName, idx, internal::resultString(result))
		}
		idx++;
	}
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
										  .compareEnable = VK_TRUE,
										  .compareOp = VK_COMPARE_OP_LESS,
										  .minLod = -1000,
										  .maxLod = 1000,
										  .borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
										  .unnormalizedCoordinates = VK_FALSE};
	static constexpr VkDescriptorSetLayoutBinding samplerLayoutBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr};
	constexpr VkDescriptorSetLayoutCreateInfo layoutCi{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
													   .pNext = nullptr,
													   .flags = {},
													   .bindingCount = 1,
													   .pBindings = &samplerLayoutBinding};
	for (auto& img: m_images) {
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
			OWL_CORE_ERROR("Vulkan Texture Descriptor: failed to allocate descriptor sets ({})",
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
									   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
									   .pImageInfo = &info,
									   .pBufferInfo = nullptr,
									   .pTexelBufferView = nullptr};
		vkUpdateDescriptorSets(core.getLogicalDevice(), 1, &wrt, 0, nullptr);
	}
}


}// namespace owl::renderer::vulkan
