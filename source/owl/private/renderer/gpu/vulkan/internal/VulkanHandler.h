/**
 * @file VulkanHandler.h
 * @author Silmaen
 * @date 30/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "FrameRing.h"
#include "VulkanCore.h"

#if OWL_WITH_GUI
#include <backends/imgui_impl_vulkan.h>
<<<<<<< HEAD
#endif
=======
#include <renderer/gpu/PipelineState.h>
>>>>>>> 79e382d7 (Name the Owl RHI and give every draw an explicit pipeline state and binding block)
#include <renderer/gpu/vulkan/Framebuffer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
	static auto get() -> VulkanHandler&;

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
#if OWL_WITH_GUI
	[[nodiscard]] auto toImGuiInfo(std::vector<VkFormat>& ioFormats) -> ImGui_ImplVulkan_InitInfo;
#endif

	/**
	 * @brief
	 *  Get the global render pass.
	 * @return The global render pass.
	 */
	[[nodiscard]] auto getGlobalRenderPass() const -> VkRenderPass { return m_swapChain->getRenderPass(); }

	/**
	 * @brief
	 *  Get the command buffer of the frame being recorded, starting the frame when none is.
	 * @return The frame command buffer, null when the handler is not running.
	 */
	[[nodiscard]] auto getCurrentCommandBuffer() -> VkCommandBuffer;

	/**
	 * @brief
	 *  Get the frame command buffer only while a render pass is open on a target that can be drawn to.
	 * @return The command buffer, null outside a render pass.
	 */
	[[nodiscard]] auto getRenderPassCommandBuffer() const -> VkCommandBuffer;

	/**
	 * @brief
	 *  Clear the first colour attachment and the depth of the current framebuffer, inside its render pass.
	 */
	void clear();

	/**
	 * @brief
	 *  Information about pipelines.
	 */
	struct PipeLineData {
		VkPipeline pipeLine = nullptr;
		VkPipelineLayout layout = nullptr;
		/// Signature key (shader, fixed-function state, layout, render pass, vertex format) of the deduplication.
		size_t key = 0;
		/// Number of live `DrawData` sharing this pipeline; the pipeline is destroyed when it reaches zero.
		uint32_t refCount = 0;
		/// Descriptor set layout the pipeline was built with, reused when a shader reload rebuilds it.
		VkDescriptorSetLayout setLayout = nullptr;
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
	 * @param[in] iState Fixed-function state: topology, culling and blending are baked in, depth stays dynamic.
	 * @param[in] iSetLayout Descriptor set layout to build with; null takes the active renderer block's (or the
	 *  global one).
	 * @return The pipeline id (use it with `getPipeline`/`bindPipeline`).
	 */
	auto pushPipeline(const std::string& iPipeLineName, std::vector<VkPipelineShaderStageCreateInfo>& iShaderStages,
					  VkPipelineVertexInputStateCreateInfo iVertexInputInfo, const gpu::PipelineState& iState,
					  VkDescriptorSetLayout iSetLayout = nullptr) -> int32_t;

	// Command buffer data
	/// True while a render pass (batch) is open in the frame command buffer.
	bool inBatch = false;
	/// True while a frame is being recorded (between begin and end frame).
	bool inFrame = false;

	/**
	 * @brief
	 *  Release a reference to a pipeline; the pipeline is destroyed once the last `DrawData` releases it.
	 * @param[in] iId Pipeline identifier returned by `pushPipeline`.
	 */
	void popPipeline(int32_t iId);

	/**
	 * @brief
	 *  Bind a pipeline, the depth state of the draw and its descriptor set into the current command buffer.
	 * @param[in] iId Pipeline identifier returned by `pushPipeline`.
	 * @param[in] iState State of the draw: its depth test and write become the dynamic depth state.
	 */
	void bindPipeline(int32_t iId, const gpu::PipelineState& iState);

	/**
	 * @brief
	 *  Open a render pass on the current framebuffer in the frame command buffer (idempotent).
	 *
	 *  The first pass of a frame on a framebuffer clears its depth (and the swapchain image); later ones load it.
	 */
	void beginBatch();

	/**
	 * @brief
	 *  Close the open render pass; nothing is submitted.
	 */
	void endBatch();

	/**
	 * @brief
	 *  Start a frame on the main framebuffer: wait for its frame slot, acquire a swapchain image, begin the
	 *  frame command buffer.
	 */
	void beginFrame();

	/**
	 * @brief
	 *  Submit what the frame recorded so far and wait for it, then go on recording (CPU read-back mid-frame).
	 */
	void flushFrame();

	/**
	 * @brief
	 *  Record transfer commands in the frame command buffer, or in a one-shot command buffer outside a frame.
	 *
	 *  The commands run after every earlier GPU access and before every later one (memory barriers on both sides);
	 *  an open render pass is closed first.
	 * @param[in] iRecord Callback recording the commands into the given command buffer.
	 */
	void recordTransfer(const std::function<void(VkCommandBuffer)>& iRecord);

	/**
	 * @brief
	 *  Record commands in a one-shot command buffer, submit it and wait for it (the frame recorded so far is flushed
	 *  first): for read-backs the CPU needs now.
	 * @param[in] iRecord Callback recording the commands into the given command buffer.
	 */
	void submitNow(const std::function<void(VkCommandBuffer)>& iRecord);

	/**
	 * @brief
	 *  Destroy a resource once the GPU no longer uses it: after every frame recorded so far is complete.
	 * @param[in] iRelease Callback destroying the resource (run immediately when the handler is not running).
	 */
	void deferRelease(std::function<void()> iRelease);

	/**
	 * @brief
	 *  Allocate host-visible memory valid for the frame being recorded (uniforms, streamed buffers, staging).
	 * @param[in] iSize Size in bytes.
	 * @return The region, empty on failure.
	 */
	[[nodiscard]] auto allocateTransient(VkDeviceSize iSize) -> RingSlice;

	/**
	 * @brief
	 *  Serial of the frame being recorded (or of the last one recorded); starts at 1, grows by one per frame.
	 * @return The frame serial.
	 */
	[[nodiscard]] auto getFrameSerial() const -> uint64_t { return m_frameSerial; }

	/**
	 * @brief
	 *  Check a frame command buffer is recording.
	 * @return True between the start of a frame and its submission.
	 */
	[[nodiscard]] auto isRecording() const -> bool { return m_recording; }

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
	 *  Get the framebuffer the next batch renders into.
	 * @return The current framebuffer.
	 */
	[[nodiscard]] auto getCurrentFramebuffer() const -> Framebuffer* { return m_currentFramebuffer; }

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

	/**
	 * @brief
	 *  Create the per-frame command buffers, fences and semaphores.
	 */
	void createFrames();

	/**
	 * @brief
	 *  Destroy the per-frame objects (the device must be idle).
	 */
	void releaseFrames();

	/**
	 * @brief
	 *  Wait for a frame slot, run the releases it unlocks and start recording in it.
	 * @return True when the command buffer is recording.
	 */
	auto startFrame() -> bool;

	/**
	 * @brief
	 *  Acquire the next swapchain image, recreating the swapchain once when it is out of date.
	 */
	void acquireImage();

	/**
	 * @brief
	 *  Begin the frame command buffer (timestamps, global barrier).
	 * @return True on success.
	 */
	auto beginCommandBuffer() -> bool;

	/**
	 * @brief
	 *  End the frame command buffer and submit it.
	 * @param[in] iLast True for the frame's last submission (signals the frame fence and the present semaphore).
	 * @param[in] iFence Fence signalled by the submission.
	 * @return True on success.
	 */
	auto submitCommandBuffer(bool iLast, VkFence iFence) -> bool;

	/**
	 * @brief
	 *  Run the deferred releases of every completed frame.
	 */
	void runReleases();

	/**
	 * @brief
	 *  Recreate the swapchain at the current surface size (the device waits idle).
	 */
	void recreateSwapChain();

	/**
	 * @brief
	 *  Record a memory barrier between every earlier and every later command.
	 * @param[in] iCmd Command buffer.
	 */
	static void recordFullBarrier(VkCommandBuffer iCmd);

	/**
	 * @brief
	 *  Record a memory barrier making every earlier device write visible to the host (before a submission ends).
	 * @param[in] iCmd Command buffer.
	 */
	static void recordHostBarrier(VkCommandBuffer iCmd);

	/// Objects of one frame in flight.
	struct FrameContext {
		VkCommandBuffer commandBuffer = nullptr;///< Primary command buffer recording the whole frame.
		VkFence fence = nullptr;///< Signalled when the frame's last submission completes.
		VkSemaphore imageAvailable = nullptr;///< Signalled by the swapchain image acquisition.
		uint64_t serial = 0;///< Serial of the frame last recorded in this slot.
	};
	/// Frames in flight.
	std::array<FrameContext, g_maxFrameInFlight> m_frames{};
	/// Slot of the frame being recorded.
	uint32_t m_frameSlot = 0;
	/// Serial of the frame being recorded (or of the last one).
	uint64_t m_frameSerial = 0;
	/// Serial of the last frame known complete on the GPU.
	uint64_t m_completedSerial = 0;
	/// True while the frame command buffer records.
	bool m_recording = false;
	/// True once commands were recorded since the command buffer began (a flush without them is skipped).
	bool m_pendingCommands = false;
	/// True during `release()`: deferred releases run at once.
	bool m_releasing = false;
	/// True when this frame acquired a swapchain image.
	bool m_imageAcquired = false;
	/// True once a submission of this frame waited on the image-available semaphore.
	bool m_imageWaited = false;
	/// True when the last submitted frame has an image to present.
	bool m_presentPending = false;
	/// Swapchain image to present.
	uint32_t m_presentImage = 0;
	/// Fence of the mid-frame submissions (`flushFrame`).
	VkFence m_flushFence = nullptr;
	/// Alignment of the transient allocations (uniform and storage offsets).
	VkDeviceSize m_transientAlignment = 256;
	/// Per-frame host-visible memory.
	FrameRing m_ring;
	/// Deferred releases, tagged with the serial of the last frame that may use the resource.
	std::deque<std::pair<uint64_t, std::function<void()>>> m_releases;

	/// The current state of the handler.
	State m_state = State::Uninitialized;
	/// Enable Validation layers.
	bool m_validation = false;
	/// Whether the validation-layer debug-message callback was registered.
	bool m_debugMessage = false;
	/// True when the swap-chain needs to be recreated on the next frame (window resize).
	bool m_resize = false;
	/// Begin timestamp query of the recording command buffer (set only while GPU timing is on).
	std::optional<uint32_t> m_batchTimestamp;
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
