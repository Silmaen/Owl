/**
 * @file VulkanHandler.cpp
 * @author Silmaen
 * @date 30/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "VulkanHandler.h"

#include "Descriptors.h"
#include "FrameProfiler.h"
#include "GpuProfiler.h"
#include "RendererDescriptors.h"
#include "app/Application.h"
#include "renderer/gpu/GraphContext.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <bit>

namespace owl::renderer::gpu::vulkan::internal {

VulkanHandler::VulkanHandler() = default;

VulkanHandler::~VulkanHandler() = default;

void VulkanHandler::initVulkan() {
	{
		createCore();
		if (m_state != State::Uninitialized)
			return;
		OWL_CORE_TRACE("Vulkan: Core created.")
	}
	{
		createFrames();
		if (m_state != State::Uninitialized)
			return;
		createSwapChain();
		if (m_state != State::Uninitialized)
			return;
		OWL_CORE_TRACE("Vulkan: Swap Chain created.")
	}
	{
		auto& desc = Descriptors::get();
		desc.createDescriptors();
		if (m_state != State::Uninitialized)
			return;
		OWL_CORE_TRACE("Vulkan: Descriptor pool created.")
	}
	GpuProfiler::init();
	m_state = State::Running;
}

void VulkanHandler::release() {
	auto& core = VulkanCore::get();
	if (core.getInstance() == nullptr)
		return;// nothing can exist without instance.
	if (core.getLogicalDevice() != nullptr)
		vkDeviceWaitIdle(core.getLogicalDevice());
	m_releasing = true;
	m_completedSerial = m_frameSerial;
	runReleases();
	m_recording = false;
	inBatch = false;
	inFrame = false;
	GpuProfiler::release();

	for (auto&& [id, pipeLine]: m_pipeLines) {
		if (pipeLine.pipeLine != nullptr)
			vkDestroyPipeline(core.getLogicalDevice(), pipeLine.pipeLine, nullptr);
		if (pipeLine.layout != nullptr)
			vkDestroyPipelineLayout(core.getLogicalDevice(), pipeLine.layout, nullptr);
	}
	m_pipeLines.clear();
	m_pipelineCache.clear();

	if (m_imGuiRenderPass != nullptr) {
		vkDestroyRenderPass(core.getLogicalDevice(), m_imGuiRenderPass, nullptr);
		m_imGuiRenderPass = nullptr;
		OWL_CORE_TRACE("Vulkan: ImGui render pass destroyed.")
	}

	m_swapChain.reset();
	m_currentFramebuffer = nullptr;
	OWL_CORE_TRACE("Vulkan: swap destroyed.")
	auto& vkd = Descriptors::get();
	vkd.release();
	OWL_CORE_TRACE("Vulkan: Descriptors released.")
	// Destroy per-renderer descriptor blocks while the device is valid; their process-static storage else outlives it.
	gpu::RendererDescriptors::releaseAll();
	RendererDescriptors::releaseDefaults();
	OWL_CORE_TRACE("Vulkan: per-renderer descriptor blocks released.")
	runReleases();
	m_ring.release();
	releaseFrames();
	core.release();
	OWL_CORE_TRACE("Vulkan: core destroyed.")
	m_state = State::Uninitialized;
	m_releasing = false;
}

#if OWL_WITH_GUI
namespace {
void func(const VkResult iResult) {
	if (iResult != VK_SUCCESS)
		OWL_CORE_ERROR("Vulkan Imgui: Error detected: {}.", resultString(iResult))
}

constexpr auto toVkTopology(const gpu::PrimitiveTopology iTopology) -> VkPrimitiveTopology {
	switch (iTopology) {
		case gpu::PrimitiveTopology::Triangles:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case gpu::PrimitiveTopology::Lines:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	}
	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

constexpr auto toVkCullMode(const gpu::CullMode iMode) -> VkCullModeFlags {
	switch (iMode) {
		case gpu::CullMode::None:
			return VK_CULL_MODE_NONE;
		case gpu::CullMode::Back:
			return VK_CULL_MODE_BACK_BIT;
		case gpu::CullMode::Front:
			return VK_CULL_MODE_FRONT_BIT;
	}
	return VK_CULL_MODE_NONE;
}
}// namespace

auto VulkanHandler::toImGuiInfo(std::vector<VkFormat>& ioFormats) -> ImGui_ImplVulkan_InitInfo {
	const auto& core = VulkanCore::get();
	auto& vkd = Descriptors::get();
	vkd.createImguiDescriptorPool();
	ioFormats = m_swapChain->getColorAttachmentFormats();
	return {
			.ApiVersion = core.getApiVersion(),
			.Instance = core.getInstance(),
			.PhysicalDevice = core.getPhysicalDevice(),
			.Device = core.getLogicalDevice(),
			.QueueFamily = core.getGraphQueueFamilyIndex(),
			.Queue = core.getGraphicQueue(),
			.DescriptorPool = vkd.getImguiDescriptorPool(),
			.DescriptorPoolSize = 0,// Use the default descriptor pool
			.MinImageCount = std::max(2u, m_swapChain->getImageCount()),
			.ImageCount = std::max(2u, m_swapChain->getImageCount()),
			.PipelineCache = VK_NULL_HANDLE,
			.PipelineInfoMain =
					{.RenderPass = m_swapChain->getRenderPass(),
					 .Subpass = 1,
					 .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
					 .ExtraDynamicStates = {},
					 .PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
													 .pNext = nullptr,
													 .viewMask = 0,
													 .colorAttachmentCount = static_cast<uint32_t>(ioFormats.size()),
													 .pColorAttachmentFormats = ioFormats.data(),
													 .depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT,
													 .stencilAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT},
					 .SwapChainImageUsage = {}},
			.PipelineInfoForViewports = {},
			.UseDynamicRendering = false,
			.Allocator = nullptr,
			.CheckVkResultFn = func,
			.MinAllocationSize = 1048576u,
			.CustomShaderVertCreateInfo = {},
			.CustomShaderFragCreateInfo = {},
	};
}
#endif

void VulkanHandler::createCore() {
	auto& core = VulkanCore::get();
	core.init({.activeValidation = m_validation, .debugMessage = m_debugMessage});
	if (core.getState() == VulkanCore::State::Error)
		m_state = State::ErrorCreatingCore;
}

void VulkanHandler::createSwapChain() {
	const auto& core = VulkanCore::get();
	m_swapChain = mkUniq<Framebuffer>(FramebufferSpecification{
			.size = core.getCurrentSize(),
			.attachments =
					{
							{.format = AttachmentSpecification::Format::Surface,
							 .tiling = AttachmentSpecification::Tiling::Optimal},
							{.format = AttachmentSpecification::Format::RedInteger,
							 .tiling = AttachmentSpecification::Tiling::Optimal},
							// Matches the editor FB depth so 2D + voxel pipelines are RP-compatible (fixes 02684).
							{.format = AttachmentSpecification::Format::Depth24Stencil8,
							 .tiling = AttachmentSpecification::Tiling::Optimal},
					},
			.samples = 2,
			.swapChainTarget = true,
	});
	unbindFramebuffer();
}

auto VulkanHandler::getPipeline(const int32_t iId) const -> VulkanHandler::PipeLineData {
	if (!m_pipeLines.contains(iId))
		return {};
	return m_pipeLines.at(iId);
}

auto VulkanHandler::getCurrentCommandBuffer() -> VkCommandBuffer {
	if (m_state != State::Running)
		return VK_NULL_HANDLE;
	if (!m_recording && !startFrame())
		return VK_NULL_HANDLE;
	m_pendingCommands = true;
	return m_frames[m_frameSlot].commandBuffer;
}

auto VulkanHandler::getRenderPassCommandBuffer() const -> VkCommandBuffer {
	if (!inBatch || !m_recording)
		return VK_NULL_HANDLE;
	return m_frames[m_frameSlot].commandBuffer;
}

auto VulkanHandler::pushPipeline(const std::string& iPipeLineName,
								 std::vector<VkPipelineShaderStageCreateInfo>& iShaderStages,
								 VkPipelineVertexInputStateCreateInfo iVertexInputInfo,
								 const gpu::PipelineState& iState, VkDescriptorSetLayout iSetLayout) -> int32_t {
	const auto& core = VulkanCore::get();
	auto& vkd = Descriptors::get();
	PipeLineData pData;
	VkDescriptorSetLayout setLayoutHandle = iSetLayout != nullptr ? iSetLayout : *vkd.getDescriptorSetLayout();
	if (auto* const rd = RendererDescriptors::getActive(); iSetLayout == nullptr && rd != nullptr) {
		if (auto* const rdLayout = rd->getDescriptorSetLayout(); rdLayout != nullptr && *rdLayout != nullptr)
			setLayoutHandle = *rdLayout;
	}
	const auto* setLayout = &setLayoutHandle;

	auto hashCombine = [](size_t& ioSeed, const size_t iValue) -> void {
		ioSeed ^= iValue + 0x9e3779b97f4a7c15ULL + (ioSeed << 6) + (ioSeed >> 2);
	};
	size_t key = std::hash<std::string>{}(iPipeLineName);
	hashCombine(key, static_cast<size_t>(iState.topology));
	hashCombine(key, static_cast<size_t>(iState.cullMode));
	hashCombine(key, static_cast<size_t>(iState.blendMode));
	hashCombine(key, std::bit_cast<uint64_t>(*setLayout));
	hashCombine(key, std::bit_cast<uint64_t>(m_currentFramebuffer->getRenderPass()));
	for (uint32_t i = 0; i < iVertexInputInfo.vertexBindingDescriptionCount; ++i) {
		const auto& bind = iVertexInputInfo.pVertexBindingDescriptions[i];
		hashCombine(key, bind.binding);
		hashCombine(key, bind.stride);
		hashCombine(key, static_cast<size_t>(bind.inputRate));
	}
	for (uint32_t i = 0; i < iVertexInputInfo.vertexAttributeDescriptionCount; ++i) {
		const auto& attr = iVertexInputInfo.pVertexAttributeDescriptions[i];
		hashCombine(key, attr.location);
		hashCombine(key, attr.binding);
		hashCombine(key, static_cast<size_t>(attr.format));
		hashCombine(key, attr.offset);
	}
	if (const auto it = m_pipelineCache.find(key); it != m_pipelineCache.end()) {
		++m_pipeLines[it->second].refCount;
		return it->second;
	}
	pData.key = key;
	pData.refCount = 1;
	pData.setLayout = setLayoutHandle;

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
														.pNext = nullptr,
														.flags = {},
														.setLayoutCount = 1,
														.pSetLayouts = setLayout,
														.pushConstantRangeCount = 0,
														.pPushConstantRanges = nullptr};
	if (const VkResult result =

				vkCreatePipelineLayout(core.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pData.layout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: Shader: failed to create pipeline layout {} ({}).", iPipeLineName, resultString(result))
		m_state = State::ErrorCreatingPipelineLayout;
		return -1;
	}
	const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.topology = toVkTopology(iState.topology),
			.primitiveRestartEnable = VK_FALSE};
	constexpr VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr};
	const VkCullModeFlags cullMode = toVkCullMode(iState.cullMode);
	const VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = cullMode,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0,
			.depthBiasSlopeFactor = 0.0f,
			.lineWidth = 2.0f};
	constexpr VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 0.0f,
			.pSampleMask = nullptr,
			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE};
	std::vector<VkPipelineColorBlendAttachmentState> att = {
			{.blendEnable = iState.blendMode == gpu::BlendMode::Alpha ? VK_TRUE : VK_FALSE,
			 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			 .colorBlendOp = VK_BLEND_OP_ADD,
			 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			 .alphaBlendOp = VK_BLEND_OP_ADD,
			 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
							   VK_COLOR_COMPONENT_A_BIT},
			{.blendEnable = VK_FALSE,
			 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			 .colorBlendOp = VK_BLEND_OP_ADD,
			 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			 .alphaBlendOp = VK_BLEND_OP_ADD,
			 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
							   VK_COLOR_COMPONENT_A_BIT}};
	const VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_COPY,
			.attachmentCount = static_cast<uint32_t>(att.size()),
			.pAttachments = att.data(),
			.blendConstants = {0.f, 0.f, 0.f, 0.f}};
	constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
												VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
												VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};
	const VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
														.pNext = nullptr,
														.flags = {},
														.dynamicStateCount =
																static_cast<uint32_t>(std::size(dynamicStates)),
														.pDynamicStates = dynamicStates};
	constexpr VkPipelineDepthStencilStateCreateInfo depthStencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
			.front = {.failOp = VK_STENCIL_OP_KEEP,
					  .passOp = VK_STENCIL_OP_KEEP,
					  .depthFailOp = VK_STENCIL_OP_KEEP,
					  .compareOp = VK_COMPARE_OP_ALWAYS,
					  .compareMask = 0,
					  .writeMask = 0,
					  .reference = 0},
			.back = {.failOp = VK_STENCIL_OP_KEEP,
					 .passOp = VK_STENCIL_OP_KEEP,
					 .depthFailOp = VK_STENCIL_OP_KEEP,
					 .compareOp = VK_COMPARE_OP_ALWAYS,
					 .compareMask = 0,
					 .writeMask = 0,
					 .reference = 0},
			.minDepthBounds = 0.0f,
			.maxDepthBounds = 1.0f};
	const VkGraphicsPipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
													.pNext = nullptr,
													.flags = {},
													.stageCount = static_cast<uint32_t>(iShaderStages.size()),
													.pStages = iShaderStages.data(),
													.pVertexInputState = &iVertexInputInfo,
													.pInputAssemblyState = &inputAssembly,
													.pTessellationState = nullptr,
													.pViewportState = &viewportState,
													.pRasterizationState = &rasterizer,
													.pMultisampleState = &multisampling,
													.pDepthStencilState = &depthStencil,
													.pColorBlendState = &colorBlending,
													.pDynamicState = &dynamicState,
													.layout = pData.layout,
													.renderPass = m_currentFramebuffer->getRenderPass(),
													.subpass = 0,
													.basePipelineHandle = VK_NULL_HANDLE,
													.basePipelineIndex = 0};

	OWL_CORE_TRACE("Vulkan pipeline: vkCreateGraphicsPipelines.")
	if (const VkResult result = vkCreateGraphicsPipelines(core.getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo,
														  nullptr, &pData.pipeLine);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create graphics pipeline for {} ({}).", iPipeLineName, resultString(result))
		m_state = State::ErrorCreatingPipeline;
		return -1;
	}

	const auto id = m_pipeLines.empty() ? 0 : static_cast<int32_t>(m_pipeLines.rbegin()->first) + 1;
	m_pipeLines.emplace(id, pData);
	m_pipelineCache.emplace(key, id);

	OWL_CORE_TRACE("Vulkan pipeline: {} Loaded.", iPipeLineName)
	return id;
}

void VulkanHandler::popPipeline(const int32_t iId) {
	const auto& core = VulkanCore::get();
	const auto it = m_pipeLines.find(iId);
	if (it == m_pipeLines.end())
		return;
	if (it->second.refCount > 1) {
		--it->second.refCount;
		return;
	}
	deferRelease(
			[device = core.getLogicalDevice(), pipeline = it->second.pipeLine, layout = it->second.layout]() -> void {
				if (pipeline != nullptr)
					vkDestroyPipeline(device, pipeline, nullptr);
				if (layout != nullptr)
					vkDestroyPipelineLayout(device, layout, nullptr);
			});
	m_pipelineCache.erase(it->second.key);
	m_pipeLines.erase(it);
}

void VulkanHandler::setClearColor(const math::vec4& iColor) { m_clearColor = iColor; }

void VulkanHandler::clear() {
	beginBatch();
	if (!inBatch)
		return;
	if (m_currentFramebuffer->getCurrentSubpass() != 0) {
		endBatch();
		beginBatch();
	}
	std::array<VkClearAttachment, 2> attachments{};
	uint32_t count = 0;
	attachments[count++] = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.colorAttachment = m_currentFramebuffer->colorAttachmentIndex(0),
							.clearValue = {.color = {.float32 = {m_clearColor.r(), m_clearColor.g(), m_clearColor.b(),
																 m_clearColor.a()}}}};
	if (m_currentFramebuffer->hasDepth())
		attachments[count++] = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
								.colorAttachment = 0,
								.clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}}};
	const VkClearRect rect{
			.rect = {.offset = {0, 0}, .extent = toExtent(m_currentFramebuffer->getSpecification().size)},
			.baseArrayLayer = 0,
			.layerCount = 1};
	vkCmdClearAttachments(m_frames[m_frameSlot].commandBuffer, count, attachments.data(), 1, &rect);
}

void VulkanHandler::drawData(const uint32_t iVertexCount, const bool iIndexed, const uint32_t iInstanceCount) {
	if (m_state != State::Running)
		return;
	if (iInstanceCount == 0)
		return;
	if (!inBatch)
		beginBatch();
	if (!inBatch)
		return;
	auto* const cmd = m_frames[m_frameSlot].commandBuffer;
	if (iIndexed)
		vkCmdDrawIndexed(cmd, iVertexCount, iInstanceCount, 0, 0, 0);
	else
		vkCmdDraw(cmd, iVertexCount, iInstanceCount, 0, 0);
}

void VulkanHandler::createFrames() {
	const auto& core = VulkanCore::get();
	auto* const device = core.getLogicalDevice();
	constexpr VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = {}};
	constexpr VkFenceCreateInfo signaledFence{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = VK_FENCE_CREATE_SIGNALED_BIT};
	constexpr VkFenceCreateInfo unsignaledFence{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
												.pNext = nullptr,
												.flags = {}};
	for (auto& frame: m_frames) {
		frame.commandBuffer = core.createCommandBuffer();
		if (frame.commandBuffer == nullptr) {
			m_state = State::ErrorCreatingCommandBuffer;
			return;
		}
		if (vkCreateFence(device, &signaledFence, nullptr, &frame.fence) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
			OWL_CORE_ERROR("Vulkan: failed to create the frame synchronisation objects.")
			m_state = State::ErrorCreatingSyncObjects;
			return;
		}
		frame.serial = 0;
	}
	if (vkCreateFence(device, &unsignaledFence, nullptr, &m_flushFence) != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create the flush fence.")
		m_state = State::ErrorCreatingSyncObjects;
		return;
	}
	m_transientAlignment = core.getMinBufferOffsetAlignment();
	m_frameSlot = 0;
	m_frameSerial = 0;
	m_completedSerial = 0;
}

void VulkanHandler::releaseFrames() {
	auto* const device = VulkanCore::get().getLogicalDevice();
	if (device == nullptr)
		return;
	for (auto& frame: m_frames) {
		if (frame.fence != nullptr)
			vkDestroyFence(device, frame.fence, nullptr);
		if (frame.imageAvailable != nullptr)
			vkDestroySemaphore(device, frame.imageAvailable, nullptr);
		frame = {};
	}
	if (m_flushFence != nullptr)
		vkDestroyFence(device, m_flushFence, nullptr);
	m_flushFence = nullptr;
	m_presentPending = false;
}

void VulkanHandler::runReleases() {
	while (!m_releases.empty() && m_releases.front().first <= m_completedSerial) {
		auto release = std::move(m_releases.front().second);
		m_releases.pop_front();
		release();
	}
}

void VulkanHandler::deferRelease(std::function<void()> iRelease) {
	if (!iRelease)
		return;
	if (m_state != State::Running || m_releasing) {
		iRelease();
		return;
	}
	m_releases.emplace_back(m_frameSerial, std::move(iRelease));
}

auto VulkanHandler::allocateTransient(const VkDeviceSize iSize) -> RingSlice {
	if (!m_recording && !startFrame())
		return {};
	return m_ring.allocate(iSize, m_transientAlignment);
}

void VulkanHandler::recreateSwapChain() {
	auto& core = VulkanCore::get();
	core.updateSurfaceInformation();
	const auto size = toSize(core.getCurrentExtent());
	if (size.x() == 0 || size.y() == 0) {
		m_resize = true;
		return;
	}
	m_resize = false;
	if (m_swapChain->getSpecification().size == size)
		m_swapChain->invalidate();
	else
		m_swapChain->resize(size);
}

void VulkanHandler::acquireImage() {
	const auto& core = VulkanCore::get();
	m_imageAcquired = false;
	m_imageWaited = false;
	if (m_resize)
		recreateSwapChain();
	const auto& frame = m_frames[m_frameSlot];
	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(core.getLogicalDevice(), m_swapChain->getSwapChain(), UINT64_MAX,
											frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		result = vkAcquireNextImageKHR(core.getLogicalDevice(), m_swapChain->getSwapChain(), UINT64_MAX,
									   frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
	}
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		m_resize = true;
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		OWL_CORE_ERROR("Vulkan: failed to acquire next image ({}).", resultString(result))
		m_state = State::ErrorAcquiringNextImage;
		return;
	}
	m_imageAcquired = true;
	m_swapChain->setCurrentImage(imageIndex);
}

auto VulkanHandler::beginCommandBuffer() -> bool {
	auto* const cmd = m_frames[m_frameSlot].commandBuffer;
	if (const VkResult result = vkResetCommandBuffer(cmd, 0); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to reset the frame command buffer ({}).", resultString(result))
		m_state = State::ErrorResetCommandBuffer;
		return false;
	}
	constexpr VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
												 .pNext = nullptr,
												 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
												 .pInheritanceInfo = nullptr};
	if (const VkResult result = vkBeginCommandBuffer(cmd, &beginInfo); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to begin the frame command buffer ({}).", resultString(result))
		m_state = State::ErrorBeginCommandBuffer;
		return false;
	}
	m_recording = true;
	m_pendingCommands = false;
	GpuProfiler::beginBatch(cmd);
	m_batchTimestamp = FrameProfiler::get().writeBegin(cmd);
	recordFullBarrier(cmd);
	return true;
}

auto VulkanHandler::submitCommandBuffer(const bool iLast, VkFence iFence) -> bool {
	auto& frame = m_frames[m_frameSlot];
	if (m_batchTimestamp.has_value()) {
		FrameProfiler::get().writeEnd(frame.commandBuffer, *m_batchTimestamp);
		m_batchTimestamp.reset();
	}
	GpuProfiler::endBatch();
	recordHostBarrier(frame.commandBuffer);
	m_recording = false;
	if (const VkResult result = vkEndCommandBuffer(frame.commandBuffer); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to end the frame command buffer ({}).", resultString(result))
		m_state = State::ErrorEndCommandBuffer;
		return false;
	}
	const bool waitImage = m_imageAcquired && !m_imageWaited;
	const bool signalImage = iLast && m_imageAcquired;
	constexpr VkPipelineStageFlags waitStage =
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
	VkSemaphore renderFinished =
			signalImage ? m_swapChain->getRenderFinishedSemaphore(m_swapChain->getCurrentImage()) : VK_NULL_HANDLE;
	const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								  .pNext = nullptr,
								  .waitSemaphoreCount = waitImage ? 1u : 0u,
								  .pWaitSemaphores = waitImage ? &frame.imageAvailable : nullptr,
								  .pWaitDstStageMask = waitImage ? &waitStage : nullptr,
								  .commandBufferCount = 1,
								  .pCommandBuffers = &frame.commandBuffer,
								  .signalSemaphoreCount = signalImage ? 1u : 0u,
								  .pSignalSemaphores = signalImage ? &renderFinished : nullptr};
	const auto& core = VulkanCore::get();
	vkResetFences(core.getLogicalDevice(), 1, &iFence);
	if (const VkResult result = vkQueueSubmit(core.getGraphicQueue(), 1, &submitInfo, iFence); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to submit the frame command buffer ({}).", resultString(result))
		m_state = State::ErrorSubmittingDrawCommand;
		return false;
	}
	m_imageWaited = m_imageWaited || waitImage;
	FrameProfiler::get().countSubmit();
	return true;
}

auto VulkanHandler::startFrame() -> bool {
	if (m_state != State::Running)
		return false;
	if (m_recording)
		return true;
	const auto& core = VulkanCore::get();
	auto& frame = m_frames[m_frameSlot];
	vkWaitForFences(core.getLogicalDevice(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
	m_completedSerial = std::max(m_completedSerial, frame.serial);
	runReleases();
	m_ring.beginFrame(m_frameSlot);
	frame.serial = ++m_frameSerial;
	FrameProfiler::get().onBeginFrame();
	acquireImage();
	if (m_state != State::Running || !beginCommandBuffer())
		return false;
	inFrame = true;
	return true;
}

void VulkanHandler::beginFrame() {
	if (m_state != State::Running)
		return;
	if (!isMainFramebuffer()) {
		OWL_CORE_WARN("Vulkan: begin frame on non main framebuffer {}.", m_currentFramebuffer->getName())
	}
	unbindFramebuffer();
	startFrame();
}

void VulkanHandler::flushFrame() {
	if (!m_recording || !m_pendingCommands)
		return;
	if (inBatch)
		endBatch();
	if (!submitCommandBuffer(false, m_flushFence))
		return;
	const auto& core = VulkanCore::get();
	FrameProfiler::get().countFenceWait();
	vkWaitForFences(core.getLogicalDevice(), 1, &m_flushFence, VK_TRUE, UINT64_MAX);
	beginCommandBuffer();
}

void VulkanHandler::recordFullBarrier(VkCommandBuffer iCmd) {
	constexpr VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
									  .pNext = nullptr,
									  .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
									  .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT};
	vkCmdPipelineBarrier(iCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier,
						 0, nullptr, 0, nullptr);
}

void VulkanHandler::recordHostBarrier(VkCommandBuffer iCmd) {
	constexpr VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
									  .pNext = nullptr,
									  .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
									  .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
	vkCmdPipelineBarrier(iCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
						 nullptr, 0, nullptr);
}

void VulkanHandler::recordTransfer(const std::function<void(VkCommandBuffer)>& iRecord) {
	if (!iRecord || VulkanCore::get().getLogicalDevice() == nullptr)
		return;
	if (m_recording) {
		if (inBatch)
			endBatch();
		m_pendingCommands = true;
		auto* const cmd = m_frames[m_frameSlot].commandBuffer;
		recordFullBarrier(cmd);
		iRecord(cmd);
		recordFullBarrier(cmd);
		return;
	}
	submitNow(iRecord);
}

void VulkanHandler::submitNow(const std::function<void(VkCommandBuffer)>& iRecord) {
	if (!iRecord || VulkanCore::get().getLogicalDevice() == nullptr)
		return;
	if (m_recording)
		flushFrame();
	const auto& core = VulkanCore::get();
	auto* const cmd = core.beginSingleTimeCommands();
	if (cmd == nullptr)
		return;
	recordFullBarrier(cmd);
	iRecord(cmd);
	recordFullBarrier(cmd);
	recordHostBarrier(cmd);
	core.endSingleTimeCommands(cmd);
}

void VulkanHandler::endFrame() {
	if (m_state != State::Running)
		return;
	if (!isMainFramebuffer()) {
		OWL_CORE_WARN("Vulkan: ending frame on non main framebuffer {}.", m_currentFramebuffer->getName())
		unbindFramebuffer();
	}
	inFrame = false;
	if (!m_recording)
		return;
	if (inBatch)
		endBatch();
	// The acquired image leaves in the present layout only through a render pass.
	if (m_imageAcquired && !m_swapChain->wasRenderedIn(m_frameSerial)) {
		beginBatch();
		endBatch();
	}
	if (!submitCommandBuffer(true, m_frames[m_frameSlot].fence))
		return;
	m_presentPending = m_imageAcquired;
	m_presentImage = m_swapChain->getCurrentImage();
	m_imageAcquired = false;
	m_frameSlot = (m_frameSlot + 1) % g_maxFrameInFlight;
}

void VulkanHandler::nextSubpass(bool internal) {
	if (!inBatch) {
		OWL_CORE_WARN("Vulkan next subpass called outside of batch.")
		return;
	}
	if (internal) {
		vkCmdNextSubpass(m_frames[m_frameSlot].commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
	} else {
		m_currentFramebuffer->nextSubpass();
	}
}

void VulkanHandler::beginBatch() {
	if (inBatch || m_state != State::Running)
		return;
	if (!m_recording && !startFrame())
		return;
	if (m_currentFramebuffer->isMainTarget() && !m_imageAcquired)
		return;
	auto* const cmd = m_frames[m_frameSlot].commandBuffer;
	m_pendingCommands = true;
	m_currentFramebuffer->prepareForRendering(cmd);
	auto& clearValues = m_currentFramebuffer->getClearValues();
	if (m_currentFramebuffer->isMainTarget() && !clearValues.empty())
		clearValues[0].color = {.float32 = {m_clearColor.r(), m_clearColor.g(), m_clearColor.b(), m_clearColor.a()}};
	const VkRenderPassBeginInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = m_currentFramebuffer->acquirePassRenderPass(m_frameSerial),
			.framebuffer = m_currentFramebuffer->getCurrentFramebuffer(),
			.renderArea = {.offset = {0, 0}, .extent = toExtent(m_currentFramebuffer->getSpecification().size)},
			.clearValueCount = static_cast<uint32_t>(clearValues.size()),
			.pClearValues = clearValues.empty() ? nullptr : clearValues.data()};
	vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	inBatch = true;
	m_currentFramebuffer->resetSubPass();
	const VkViewport viewport{.x = 0.0f,
							  .y = 0.0f,
							  .width = static_cast<float>(m_currentFramebuffer->getSpecification().size.x()),
							  .height = static_cast<float>(m_currentFramebuffer->getSpecification().size.y()),
							  .minDepth = 0.0f,
							  .maxDepth = 1.0f};
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	const VkRect2D scissor{.offset = {0, 0}, .extent = toExtent(m_currentFramebuffer->getSpecification().size)};
	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanHandler::endBatch() {
	if (!inBatch)
		return;
	while (m_currentFramebuffer->getCurrentSubpass() + 1 < m_currentFramebuffer->getSubpassCount())
		m_currentFramebuffer->nextSubpass();
	vkCmdEndRenderPass(m_frames[m_frameSlot].commandBuffer);
	inBatch = false;
}

void VulkanHandler::swapFrame() {
	if (m_state != State::Running)
		return;
	if (inFrame || m_recording)
		endFrame();
	if (!m_presentPending) {
		if (m_resize)
			recreateSwapChain();
		return;
	}
	m_presentPending = false;
	VkSemaphore waiter = m_swapChain->getRenderFinishedSemaphore(m_presentImage);
	VkSwapchainKHR swapChain = m_swapChain->getSwapChain();
	const VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
									   .pNext = nullptr,
									   .waitSemaphoreCount = 1,
									   .pWaitSemaphores = &waiter,
									   .swapchainCount = 1,
									   .pSwapchains = &swapChain,
									   .pImageIndices = &m_presentImage,
									   .pResults = nullptr};
	const auto& core = VulkanCore::get();
	if (const VkResult result = vkQueuePresentKHR(core.getPresentQueue(), &presentInfo);
		m_resize || result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || result == VK_SUCCESS) {
			recreateSwapChain();
		} else {
			OWL_CORE_ERROR("Vulkan: failed to present queue ({}).", resultString(result))
			m_state = State::ErrorPresentingQueue;
		}
	}
}

void VulkanHandler::bindPipeline(const int32_t iId, const gpu::PipelineState& iState) {
	if (m_state != State::Running)
		return;
	if (!m_pipeLines.contains(iId)) {
		OWL_CORE_WARN("Vulkan: cannot bind pipeline with id {}.", iId)
		return;
	}
	auto* const cmd = getCurrentCommandBuffer();
	if (cmd == nullptr)
		return;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLines[iId].pipeLine);
	vkCmdSetDepthTestEnable(cmd, iState.depthTest ? VK_TRUE : VK_FALSE);
	vkCmdSetDepthWriteEnable(cmd, iState.depthTest && iState.depthWrite ? VK_TRUE : VK_FALSE);
	const VkDescriptorSet* set = nullptr;
	if (auto* const rd = RendererDescriptors::getActive(); rd != nullptr) {
		set = rd->getDescriptorSet(getCurrentFrameIndex());
	}
	if (set == nullptr || *set == nullptr) {
		set = Descriptors::get().getDescriptorSet(getCurrentFrameIndex());
	}
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLines[iId].layout, 0, 1, set, 0, nullptr);
}

void VulkanHandler::setResize() {
	auto& core = VulkanCore::get();
	core.updateSurfaceInformation();
	m_resize = true;
}

void VulkanHandler::bindFramebuffer(Framebuffer* iFrameBuffer) {
	if (inBatch)
		endBatch();
	m_currentFramebuffer = iFrameBuffer;
}

void VulkanHandler::unbindFramebuffer() {
	if (inBatch)
		endBatch();
	m_currentFramebuffer = m_swapChain.get();
}

auto VulkanHandler::isMainFramebuffer() const -> bool { return m_currentFramebuffer == m_swapChain.get(); }

auto VulkanHandler::getCurrentFrameBufferName() const -> std::string {
	if (m_currentFramebuffer == nullptr)
		return "none";
	return m_currentFramebuffer->getName();
}

auto VulkanHandler::getCurrentFrameIndex() const -> uint32_t { return m_frameSlot; }

auto VulkanHandler::get() -> VulkanHandler& {
	static VulkanHandler handler;
	return handler;
}

}// namespace owl::renderer::gpu::vulkan::internal
