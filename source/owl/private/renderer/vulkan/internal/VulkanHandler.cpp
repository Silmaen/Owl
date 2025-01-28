/**
 * @file VulkanHandler.cpp
 * @author Silmaen
 * @date 30/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "VulkanHandler.h"

#include "renderer/GraphContext.h"
#include "Descriptors.h"
#include "core/Application.h"
#include "utils.h"

namespace owl::renderer::vulkan::internal {

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
	m_state = State::Running;
}

void VulkanHandler::release() {
	auto& core = VulkanCore::get();
	if (core.getInstance() == nullptr)
		return;// nothing can exists without instance.

	for (auto&& [id, pipeLine]: m_pipeLines) {
		if (pipeLine.pipeLine != nullptr)
			vkDestroyPipeline(core.getLogicalDevice(), pipeLine.pipeLine, nullptr);
		if (pipeLine.layout != nullptr)
			vkDestroyPipelineLayout(core.getLogicalDevice(), pipeLine.layout, nullptr);
	}
	m_pipeLines.clear();

	if (m_ImGuiRenderPass != nullptr) {
		vkDestroyRenderPass(core.getLogicalDevice(), m_ImGuiRenderPass, nullptr);
		m_ImGuiRenderPass = nullptr;
		OWL_CORE_TRACE("Vulkan: ImGui render pass destroyed.")
	}

	m_swapChain.reset();
	m_currentframebuffer = nullptr;
	OWL_CORE_TRACE("Vulkan: swap destroyed.")
	auto& vkd = Descriptors::get();
	vkd.release();
	OWL_CORE_TRACE("Vulkan: Descriptors released.")
	core.release();
	OWL_CORE_TRACE("Vulkan: core destroyed.")
	m_state = State::Uninitialized;
}

namespace {
void func(const VkResult iResult) {
	if (iResult != VK_SUCCESS)
		OWL_CORE_ERROR("Vulkan Imgui: Error detected: {}", resultString(iResult))
}
}// namespace

auto VulkanHandler::toImGuiInfo(std::vector<VkFormat>& ioFormats) -> ImGui_ImplVulkan_InitInfo {
	const auto& core = VulkanCore::get();
	auto& vkd = Descriptors::get();
	vkd.createImguiDescriptorPool();
	ioFormats = m_swapChain->getColorAttachmentFormats();
	return {.Instance = core.getInstance(),
			.PhysicalDevice = core.getPhysicalDevice(),
			.Device = core.getLogicalDevice(),
			.QueueFamily = core.getGraphQueueFamilyIndex(),
			.Queue = core.getGraphicQueue(),
			.DescriptorPool = vkd.getImguiDescriptorPool(),
			.RenderPass = m_swapChain->getRenderPass(),
			.MinImageCount = m_swapChain->getImageCount(),
			.ImageCount = m_swapChain->getImageCount(),
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.PipelineCache = VK_NULL_HANDLE,
			.Subpass = 0,
			.UseDynamicRendering = false,
			.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
											.pNext = nullptr,
											.viewMask = 0,
											.colorAttachmentCount = static_cast<uint32_t>(ioFormats.size()),
											.pColorAttachmentFormats = ioFormats.data(),
											.depthAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT,
											.stencilAttachmentFormat = VK_FORMAT_D24_UNORM_S8_UINT},
			.Allocator = nullptr,
			.CheckVkResultFn = func,
			.MinAllocationSize = 1048576u};
}

void VulkanHandler::createCore() {
	auto& core = VulkanCore::get();
	core.init({.activeValidation = m_validation});
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
							//{AttachmentSpecification::Format::Depth24Stencil8,
							// AttachmentSpecification::Tiling::Optimal}
					},
			.samples = 2,
			.swapChainTarget = true,
	});
	unbindFramebuffer();
}

auto VulkanHandler::getPipeline(const int32_t iId) const -> VulkanHandler::PipeLineData {
	if (m_state == State::Running || !m_pipeLines.contains(iId))
		return {};
	return m_pipeLines.at(iId);
}

auto VulkanHandler::getCurrentCommandBuffer() const -> VkCommandBuffer {
	return *m_currentframebuffer->getCurrentCommandbuffer();
}

auto VulkanHandler::pushPipeline(const std::string& iPipeLineName,
								 std::vector<VkPipelineShaderStageCreateInfo>& iShaderStages,
								 VkPipelineVertexInputStateCreateInfo iVertexInputInfo, bool iDoubleSided) -> int32_t {
	const auto& core = VulkanCore::get();
	auto& vkd = Descriptors::get();
	PipeLineData pData;
	// PipeLine Layout
	const VkPipelineLayoutCreateInfo pipelineLayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
														.pNext = nullptr,
														.flags = {},
														.setLayoutCount = 1,
														.pSetLayouts = vkd.getDescriptorSetLayout(),
														.pushConstantRangeCount = 0,
														.pPushConstantRanges = nullptr};
	if (const VkResult result =
				vkCreatePipelineLayout(core.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pData.layout);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: Shader: failed to create pipeline layout {} ({})", iPipeLineName, resultString(result))
		m_state = State::ErrorCreatingPipelineLayout;
		return -1;
	}
	// TODO(Silmaen): find a better switch.
	const bool isLine = iPipeLineName == "line";
	const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.topology = isLine ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE};
	constexpr VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr};
	const VkCullModeFlags cullMode = iDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
	const VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = cullMode,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,
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
			{.blendEnable = VK_TRUE,
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
	constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	const VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
														.pNext = nullptr,
														.flags = {},
														.dynamicStateCount = 2,
														.pDynamicStates = dynamicStates};
	constexpr VkPipelineDepthStencilStateCreateInfo depthStensil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_FALSE,
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
													.pDepthStencilState = &depthStensil,
													.pColorBlendState = &colorBlending,
													.pDynamicState = &dynamicState,
													.layout = pData.layout,
													.renderPass = m_currentframebuffer->getRenderPass(),
													.subpass = 0,
													.basePipelineHandle = VK_NULL_HANDLE,
													.basePipelineIndex = 0};

	OWL_CORE_TRACE("Vulkan pipeline: vkCreateGraphicsPipelines")
	if (const VkResult result = vkCreateGraphicsPipelines(core.getLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo,
														  nullptr, &pData.pipeLine);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to create graphics pipeline for {} ({})", iPipeLineName, resultString(result))
		m_state = State::ErrorCreatingPipeline;
		return -1;
	}

	const auto id = m_pipeLines.empty() ? 0 : static_cast<int32_t>(m_pipeLines.rbegin()->first) + 1;
	m_pipeLines.emplace(id, pData);
	OWL_CORE_TRACE("Vulkan pipeline: {} Loaded.", iPipeLineName)
	return id;
}

void VulkanHandler::popPipeline(const int32_t iId) {
	const auto& core = VulkanCore::get();
	if (!m_pipeLines.contains(iId))
		return;
	if (m_pipeLines[iId].pipeLine != nullptr)
		vkDestroyPipeline(core.getLogicalDevice(), m_pipeLines[iId].pipeLine, nullptr);
	if (m_pipeLines[iId].layout != nullptr)
		vkDestroyPipelineLayout(core.getLogicalDevice(), m_pipeLines[iId].layout, nullptr);
	m_pipeLines.erase(iId);
}

void VulkanHandler::setClearColor(const math::vec4& iColor) { m_clearColor = iColor; }

void VulkanHandler::clear() const { m_currentframebuffer->clearAttachment(0, m_clearColor); }

void VulkanHandler::drawData(const uint32_t iVertexCount, const bool iIndexed) {
	if (m_state != State::Running)
		return;
	if (!inBatch)
		beginBatch();
	if (iIndexed)
		vkCmdDrawIndexed(getCurrentCommandBuffer(), iVertexCount, 1, 0, 0, 0);
	else
		vkCmdDraw(getCurrentCommandBuffer(), iVertexCount, 1, 0, 0);
}

void VulkanHandler::beginFrame() {
	if (m_state != State::Running)
		return;
	const auto& core = VulkanCore::get();
	unbindFramebuffer();
	m_currentframebuffer->resetBatch();
	vkWaitForFences(core.getLogicalDevice(), 1, m_currentframebuffer->getCurrentFence(), VK_TRUE, UINT64_MAX);
	uint32_t imageIndex = 0;
	if (const VkResult result = vkAcquireNextImageKHR(
				core.getLogicalDevice(), m_currentframebuffer->getSwapChain(), UINT64_MAX,
				m_currentframebuffer->getCurrentImageAvailableSemaphore(), VK_NULL_HANDLE, &imageIndex);
		result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			m_currentframebuffer->resize(toSize(core.getCurrentExtent()));
			return;
		}
		if (result != VK_SUBOPTIMAL_KHR) {
			OWL_CORE_ERROR("Vulkan: failed to aquire next image ({}).", resultString(result))
			m_state = State::ErrorAcquiringNextImage;
			return;
		}
		vkResetFences(core.getLogicalDevice(), 1, m_currentframebuffer->getCurrentFence());
	}
	m_currentframebuffer->setCurrentImage(imageIndex);
}

void VulkanHandler::endFrame() {
	if (m_state != State::Running)
		return;
	if (inBatch)
		endBatch();
	unbindFramebuffer();
	m_currentframebuffer->resetBatch();
}

void VulkanHandler::beginBatch() {
	inBatch = true;

	const auto& core = VulkanCore::get();
	vkWaitForFences(core.getLogicalDevice(), 1, m_currentframebuffer->getCurrentFence(), VK_TRUE, UINT64_MAX);
	vkResetFences(core.getLogicalDevice(), 1, m_currentframebuffer->getCurrentFence());

	if (const VkResult result = vkResetCommandBuffer(getCurrentCommandBuffer(), /*VkCommandBufferResetFlagBits*/ 0);
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to reset recording command buffer ({}).", resultString(result))
		m_state = State::ErrorResetCommandBuffer;
		return;
	}
	constexpr VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
												 .pNext = nullptr,
												 .flags = {},
												 .pInheritanceInfo = nullptr};
	if (const VkResult result = vkBeginCommandBuffer(getCurrentCommandBuffer(), &beginInfo); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to begin recording command buffer ({}).", resultString(result))
		m_state = State::ErrorBeginCommandBuffer;
		return;
	}

	const VkRenderPassBeginInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = nullptr,
			.renderPass = m_currentframebuffer->getRenderPass(),
			.framebuffer = m_currentframebuffer->getCurrentFramebuffer(),
			.renderArea = {.offset = {0, 0}, .extent = toExtent(m_currentframebuffer->getSpecification().size)},
			.clearValueCount = 0,
			.pClearValues = nullptr};
	vkCmdBeginRenderPass(getCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	const VkViewport viewport{.x = 0.0f,
							  .y = 0.0f,
							  .width = static_cast<float>(m_currentframebuffer->getSpecification().size.x()),
							  .height = static_cast<float>(m_currentframebuffer->getSpecification().size.y()),
							  .minDepth = 0.0f,
							  .maxDepth = 1.0f};
	vkCmdSetViewport(getCurrentCommandBuffer(), 0, 1, &viewport);
	const VkRect2D scissor{.offset = {0, 0}, .extent = toExtent(m_currentframebuffer->getSpecification().size)};
	vkCmdSetScissor(getCurrentCommandBuffer(), 0, 1, &scissor);
}

void VulkanHandler::endBatch() {
	static bool alreadyRun = false;
	vkCmdEndRenderPass(getCurrentCommandBuffer());
	if (const VkResult result = vkEndCommandBuffer(getCurrentCommandBuffer()); result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to end command buffer ({}).", resultString(result))
		m_state = State::ErrorEndCommandBuffer;
	}
	constexpr VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	const VkSemaphore signalSemaphores[] = {m_currentframebuffer->getCurrentFinishedSemaphore()};
	VkSemaphore* waiter = nullptr;
	if (alreadyRun) {
		VkSemaphore waitSemaphoresStart = m_currentframebuffer->getCurrentImageAvailableSemaphore();
		VkSemaphore waitSemaphores = m_currentframebuffer->getCurrentFinishedSemaphore();
		waiter = m_currentframebuffer->isFirstBatch() ? &waitSemaphoresStart : &waitSemaphores;
	}
	const VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
								  .pNext = nullptr,
								  .waitSemaphoreCount = alreadyRun ? 1u : 0u,
								  .pWaitSemaphores = waiter,
								  .pWaitDstStageMask = waitStages,
								  .commandBufferCount = 1,
								  .pCommandBuffers = m_currentframebuffer->getCurrentCommandbuffer(),
								  .signalSemaphoreCount = 1,
								  .pSignalSemaphores = signalSemaphores};
	const auto& core = VulkanCore::get();
	if (const VkResult result =
				vkQueueSubmit(core.getGraphicQueue(), 1, &submitInfo, *m_currentframebuffer->getCurrentFence());
		result != VK_SUCCESS) {
		OWL_CORE_ERROR("Vulkan: failed to submit draw command buffer ({}).", resultString(result))
		m_state = State::ErrorSubmitingDrawCommand;
		return;
	}
	inBatch = false;
#ifdef OWL_DEBUG
	// TODO(Silmaen): Investigate why we got a crash in release with wait semaphore
	if (!alreadyRun)
		alreadyRun = true;
#endif
	m_currentframebuffer->batchTouch();
}

void VulkanHandler::swapFrame() {
	if (m_state != State::Running)
		return;
	if (inBatch)
		endFrame();
	const VkSemaphore signalSemaphores[] = {m_currentframebuffer->getCurrentFinishedSemaphore()};
	const VkSwapchainKHR swapChains[] = {m_currentframebuffer->getSwapChain()};

	const VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
									   .pNext = nullptr,
									   .waitSemaphoreCount = 1,
									   .pWaitSemaphores = signalSemaphores,
									   .swapchainCount = 1,
									   .pSwapchains = swapChains,
									   .pImageIndices = m_currentframebuffer->getCurrentImage(),
									   .pResults = nullptr};
	const auto& core = VulkanCore::get();
	if (const VkResult result = vkQueuePresentKHR(core.getPresentQueue(), &presentInfo);
		m_resize || result != VK_SUCCESS) {
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_resize) {
			m_resize = false;
			m_currentframebuffer->resize(toSize(core.getCurrentExtent()));
		} else {
			OWL_CORE_ERROR("Vulkan: failed to present queue ({}).", resultString(result))
			m_state = State::ErrorPresentingQueue;
		}
	}
	m_currentframebuffer->nextFrame();
}

void VulkanHandler::bindPipeline(const int32_t iId) {
	if (m_state != State::Running)
		return;
	if (!m_pipeLines.contains(iId)) {
		OWL_CORE_WARN("Vulkan: cannot bind pipeline with id {}", iId)
		return;
	}
	auto& vkd = Descriptors::get();
	vkCmdBindPipeline(getCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLines[iId].pipeLine);
	vkCmdBindDescriptorSets(getCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeLines[iId].layout, 0, 1,
							vkd.getDescriptorSet(getCurrentFrameIndex()), 0, nullptr);
}

void VulkanHandler::setResize() {
	auto& core = VulkanCore::get();
	core.updateSurfaceInformation();
	m_resize = true;
}

void VulkanHandler::bindFramebuffer(Framebuffer* iFrameBuffer) { m_currentframebuffer = iFrameBuffer; }

void VulkanHandler::unbindFramebuffer() {
	if (inBatch)
		endBatch();
	m_currentframebuffer = m_swapChain.get();
}

auto VulkanHandler::getCurrentFrameBufferName() const -> std::string {
	if (m_currentframebuffer == nullptr)
		return "none";
	return m_currentframebuffer->getName();
}

auto VulkanHandler::getCurrentFrameIndex() const -> uint32_t { return m_swapChain->getImageIndex(); }

}// namespace owl::renderer::vulkan::internal
