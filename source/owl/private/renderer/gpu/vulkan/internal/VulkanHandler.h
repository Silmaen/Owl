/**
 * @file VulkanHandler.h
 * @author Silmaen
 * @date 30/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "VulkanCore.h"

#include <backends/imgui_impl_vulkan.h>
#include <renderer/gpu/vulkan/Framebuffer.h>

/**
 * @brief
 *  Internal functions of the vulkan renderer.
 */
namespace owl::renderer::gpu::vulkan::internal {
/**
 * @brief
 *  Class for handling Vulkan API.
 */
class VulkanHandler final {
public:
	VulkanHandler(const VulkanHandler&) = delete;

	VulkanHandler(VulkanHandler&&) = delete;

	auto operator=(const VulkanHandler&) -> VulkanHandler& = delete;

	auto operator=(VulkanHandler&&) -> VulkanHandler& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~VulkanHandler();

	/**
	 * @brief
	 *  Handler for vulkan objects.
	 * @return Vulcan handler
	 */
	static auto get() -> VulkanHandler& {
		static VulkanHandler handler;
		return handler;
	}

	/**
	 * @brief
	 *  Initialize the vulkan handler.
	 */
	void initVulkan();

	/**
	 * @brief
	 *  Release the vulkan handler.
	 */
	void release();

	/// List of handler states.
	enum struct State : uint8_t {
		/// Not initialized of closed.
		Uninitialized,
		/// Initialized and ready.
		Running,
		/// Encounter an error while creating the core of vulkan.
		ErrorCreatingCore,
		ErrorCreatingSwapChain,
		ErrorCreatingImagesView,
		ErrorCreatingRenderPass,
		ErrorCreatingPipelineLayout,
		ErrorCreatingPipeline,
		ErrorCreatingDescriptorPool,
		ErrorCreatingCommandPool,
		ErrorCreatingCommandBuffer,
		ErrorSubmittingDrawCommand,
		ErrorPresentingQueue,
		ErrorAcquiringNextImage,
		ErrorResetCommandBuffer,
		ErrorBeginCommandBuffer,
		ErrorEndCommandBuffer,
		ErrorCreatingSyncObjects,
		ErrorCreatingDescriptorSet,
		ErrorCreatingDescriptorSetLayout,
	};

	/**
	 * @brief
	 *  Gets the current state of the handler.
	 * @return The state of the handler.
	 */
	[[nodiscard]] auto getState() const -> const State& { return m_state; }

	/**
	 * @brief
	 *  Define a new state for the vulkan handler.
	 * @param[in] iState The new state.
	 */
	void setState(const State& iState) { m_state = iState; }

	/**
	 * @brief
	 *  Activate the validation layer, if not already initialized.
	 */
	void activateValidation() {
		if (m_state == State::Uninitialized)
			m_validation = true;
	}

	/**
	 * @brief
	 *  Activate debug message.
	 */
	void activateDebugMessage() {
		if (m_state == State::Uninitialized)
			m_debugMessage = true;
	}

	/**
	 * @brief
	 *  Get the swap chain.
	 * @return The swap chain.
	 */
	[[nodiscard]] auto getSwapChain() const -> Framebuffer* { return m_swapChain.get(); }

	/**
	 * @brief
	 *  Get the im gui render pass.
	 * @return The im gui render pass.
	 */
	[[nodiscard]] auto getImGuiRenderPass() const -> VkRenderPass { return m_imGuiRenderPass; }

	/**
	 * @brief
	 *  Build the `ImGui_ImplVulkan_InitInfo` structure used to bring up the ImGui Vulkan backend.
	 * @param[in,out] ioFormats Filled with the swapchain colour formats expected by ImGui.
	 * @return Populated init-info ready to be passed to `ImGui_ImplVulkan_Init`.
	 */
	[[nodiscard]] auto toImGuiInfo(std::vector<VkFormat>& ioFormats) -> ImGui_ImplVulkan_InitInfo;

	/**
	 * @brief
	 *  Get the global render pass.
	 * @return The global render pass.
	 */
	[[nodiscard]] auto getGlobalRenderPass() const -> VkRenderPass { return m_swapChain->getRenderPass(); }

	/**
	 * @brief
	 *  Get the current command buffer.
	 * @return The current command buffer.
	 */
	[[nodiscard]] auto getCurrentCommandBuffer() const -> VkCommandBuffer;

	/**
	 * @brief
	 *  Clear .
	 */
	void clear() const;

	/**
	 * @brief
	 *  Information about pipelines.
	 */
	struct PipeLineData {
		VkPipeline pipeLine = nullptr;
		VkPipelineLayout layout = nullptr;
		/// Signature key (shader + layout + render pass + vertex format) shared pipelines are deduplicated by.
		size_t key = 0;
		/// Number of live `DrawData` sharing this pipeline; the pipeline is destroyed when it reaches zero.
		uint32_t refCount = 0;
	};

	/**
	 * @brief
	 *  Get the pipeline data registered under the given id.
	 * @param[in] iId Pipeline identifier returned by `pushPipeline`.
	 * @return The pipeline handle and its layout.
	 */
	[[nodiscard]] auto getPipeline(int32_t iId) const -> PipeLineData;

	/**
	 * @brief
	 *  Build a graphics pipeline and store it under a stable id.
	 * @param[in] iPipeLineName Name of the pipeline.
	 * @param[in] iShaderStages Shader stage create-infos to bind into the pipeline.
	 * @param[in] iVertexInputInfo Vertex input layout description.
	 * @param[in] iDoubleSided When true, both faces are rendered.
	 * @return The pipeline id (use it with `getPipeline`/`bindPipeline`).
	 */
	auto pushPipeline(const std::string& iPipeLineName, std::vector<VkPipelineShaderStageCreateInfo>& iShaderStages,
					  VkPipelineVertexInputStateCreateInfo iVertexInputInfo, bool iDoubleSided = true) -> int32_t;

	// Command buffer data
	/// True while a batch (command buffer) is being recorded.
	bool inBatch = false;
	/// True while a frame is in progress (between begin/end frame).
	bool inFrame = false;
	/// Desired depth test state for the next batch (off by default; Renderer3D enables it around 3D mesh draws).
	bool depthTestEnabled = false;
	/// Desired depth write state for the next batch (on by default; disabled for the blended transparent pass).
	bool depthWriteEnabled = true;
	/// True until the main target's first batch of the frame is submitted (gates the image-available wait).
	bool firstBatch = true;

	/**
	 * @brief
	 *  Release a reference to a pipeline; the pipeline is destroyed once the last `DrawData` releases it.
	 * @param[in] iId Pipeline identifier returned by `pushPipeline`.
	 */
	void popPipeline(int32_t iId);

	/**
	 * @brief
	 *  Bind a pipeline and its descriptor set into the current command buffer for upcoming draws.
	 * @param[in] iId Pipeline identifier returned by `pushPipeline`.
	 */
	void bindPipeline(int32_t iId);

	/**
	 * @brief
	 *  Begin batch.
	 */
	void beginBatch();

	/**
	 * @brief
	 *  End batch.
	 */
	void endBatch();

	/**
	 * @brief
	 *  Begin frame.
	 */
	void beginFrame();

	/**
	 * @brief
	 *  Advance the active render pass to the next subpass.
	 * @param[in] internal When true, marks the transition as internal (skips client-visible bookkeeping).
	 */
	void nextSubpass(bool internal = false);

	/**
	 * @brief
	 *  End frame.
	 */
	void endFrame();

	/**
	 * @brief
	 *  Swap frame.
	 */
	void swapFrame();

	/**
	 * @brief
	 *  Record a draw into the current command buffer.
	 * @param[in] iVertexCount Index count when indexed, otherwise vertex count.
	 * @param[in] iIndexed When true, issue `vkCmdDrawIndexed`; otherwise `vkCmdDraw`.
	 * @param[in] iInstanceCount Number of instances to draw.
	 */
	void drawData(uint32_t iVertexCount, bool iIndexed = true, uint32_t iInstanceCount = 1);

	void setClearColor(const math::vec4& iColor);

	/**
	 * @brief
	 *  Set the resize.
	 */
	void setResize();

	/**
	 * @brief
	 *  Get the current frame index.
	 * @return The current frame index.
	 */
	[[nodiscard]] auto getCurrentFrameIndex() const -> uint32_t;

	void bindFramebuffer(Framebuffer* iFrameBuffer);

	/**
	 * @brief
	 *  Unbind framebuffer.
	 */
	void unbindFramebuffer();

	/**
	 * @brief
	 *  Get the current frame buffer name.
	 * @return The current frame buffer name.
	 */
	[[nodiscard]] auto getCurrentFrameBufferName() const -> std::string;

	/**
	 * @brief
	 *  Check whether main framebuffer.
	 * @return True when main framebuffer.
	 */
	[[nodiscard]] auto isMainFramebuffer() const -> bool;

private:
	/**
	 * @brief
	 *  Default Constructor.
	 */
	VulkanHandler();

	/**
	 * @brief
	 *  Create the instance.
	 */
	void createCore();

	/**
	 * @brief
	 *  Create swap chain.
	 */
	void createSwapChain();

	/// The current state of the handler.
	State m_state = State::Uninitialized;
	/// Enable Validation layers.
	bool m_validation = false;
	/// Whether the validation-layer debug-message callback was registered.
	bool m_debugMessage = false;
	/// True when the swap-chain needs to be recreated on the next frame (window resize).
	bool m_resize = false;
	/// Render pass used by the ImGui overlay layer.
	VkRenderPass m_imGuiRenderPass{};

	/// The swap chain (main framebuffer).
	uniq<Framebuffer> m_swapChain;
	/// The active framebuffer.
	Framebuffer* m_currentFramebuffer = nullptr;

	/// Clear colour applied at the start of each render pass.
	math::vec4 m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

	/// List of pipelines.
	std::map<int32_t, PipeLineData> m_pipeLines;
	/// Signature key -> pipeline id, so identical pipelines (e.g. every voxel chunk) are built once and shared.
	std::unordered_map<size_t, int32_t> m_pipelineCache;
};
}// namespace owl::renderer::gpu::vulkan::internal
