/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "DrawData.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer::gpu {
class StorageBuffer;
class Texture2D;
}// namespace owl::renderer::gpu

/**
 * @brief
 *  The Owl RHI: the render hardware interface every renderer draws through.
 *
 * Hosts the GPU-API-agnostic interfaces (`RenderAPI`, `RenderCommand`, `Texture`, `Shader`, `Buffer`, `DrawData`,
 * `PipelineState`, `RendererDescriptors`, `Framebuffer`, `GraphContext`, `UniformBuffer`, `StorageBuffer`,
 * `ComputeShader`) plus the data enums they share. A draw is a `DrawData` (shader, vertex layout and fixed-function
 * `PipelineState`, all given at `init`) drawn with the bindings recorded in the active renderer block
 * (`RendererDescriptors::ScopedActive`); no draw inherits state from the previous one. Backends live under
 * sub-namespaces: `vulkan` (reference), `opengl` (frozen fallback) and `null` (headless tests). The backend is chosen
 * at runtime by `RenderCommand::create(Type)`. See the *Owl RHI* section of the renderer documentation.
 */
namespace owl::renderer::gpu {
/**
 * @brief
 *  GPU time of one rendered frame, measured with timestamp queries.
 *
 * Vulkan times every submitted command buffer (batches and one-shot copies, transitions, clears and compute
 * dispatches); OpenGL times the span between `beginFrame` and `endFrame`.
 */
struct OWL_API GpuFrameTiming {
	/// Backend frame number the timing belongs to (see `RenderAPI::getGpuFrameId`).
	uint64_t frameId{0};
	/// Sum of the timed GPU intervals of the frame, in milliseconds (GPU busy time).
	double busyMs{0.0};
	/// Time between the first and the last timestamp of the frame, in milliseconds (includes GPU idle gaps).
	double spanMs{0.0};
	/// Number of timed intervals (Vulkan: command buffers, OpenGL: 1).
	uint32_t intervalCount{0};
};

/**
 * @brief
 *  Cumulative backend counters; profiling tools read them as per-frame deltas.
 */
struct OWL_API RenderCounters {
	/// Draw calls issued through `RenderCommand` (all backends).
	uint64_t drawCalls{0};
	/// Command buffer submissions to the GPU queue (Vulkan only).
	uint64_t submits{0};
	/// `vkQueueWaitIdle` calls: the CPU waits for the queue to drain (Vulkan only).
	uint64_t queueWaitIdles{0};
	/// `vkDeviceWaitIdle` calls: the CPU waits for the whole device to drain (Vulkan only).
	uint64_t deviceWaitIdles{0};
	/// Blocking fence waits besides the frame pacing: one-shot submissions and mid-frame read-backs (Vulkan only).
	uint64_t fenceWaits{0};
};

/**
 * @brief
 *  Abstract class to manage rendering API.
 */
class OWL_API RenderAPI {
public:
	/// Render API types.
	enum struct Type : uint8_t {
		Null = 0,///< Null Renderer.
		OpenGL = 1,///< OpenGL Renderer.
		Vulkan = 2,///< Vulkan renderer API.
	};

	explicit RenderAPI(const Type& iType) : m_type{iType} {}

	RenderAPI(const RenderAPI&) = delete;

	RenderAPI(RenderAPI&&) = delete;

	auto operator=(const RenderAPI&) -> RenderAPI& = delete;

	auto operator=(RenderAPI&&) -> RenderAPI& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~RenderAPI();

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	virtual void init() = 0;

	/**
	 * @brief
	 *  Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	virtual void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) = 0;

	/**
	 * @brief
	 *  Define the background colour.
	 * @param[in] iColor The background colour.
	 */
	virtual void setClearColor(const math::vec4& iColor) = 0;

	/**
	 * @brief
	 *  Clear the screen.
	 */
	virtual void clear() = 0;

	/**
	 * @brief
	 *  Draw a `DrawData` with its pipeline state and the bindings of the active renderer block.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	virtual void drawData(const shared<DrawData>& iData, uint32_t iIndexCount) = 0;

	/**
	 * @brief
	 *  Issue an instanced draw using a `DrawData` initialised via `initInstanced` (or reading its instances from a
	 *  storage buffer). Primitives follow the `PipelineState::topology` of the draw data.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of indices per instance.
	 * @param[in] iInstanceCount Number of instances to draw.
	 */
	virtual void drawDataInstanced(const shared<DrawData>& iData, uint32_t iIndexCount, uint32_t iInstanceCount) = 0;

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] virtual auto getMaxTextureSlots() const -> uint32_t = 0;

	/**
	 * @brief
	 *  Set the sampled textures of the active renderer block: slot `i` samples `iTextures[i]` (a null entry or a slot
	 *  past the span samples the default texture) for the next draws of that block.
	 * @param[in] iTextures Textures in slot order.
	 */
	virtual void bindTextures(std::span<const shared<Texture2D>> iTextures) = 0;

	/// Render API states.
	enum struct State : uint8_t {
		Created,///< Just created.
		Ready,///< Ready to work.
		Error///< in error.
	};

	/**
	 * @brief
	 *  Get the actual API type.
	 * @return API Type.
	 */
	[[nodiscard]] auto getApi() const -> Type { return m_type; }

	/**
	 * @brief
	 *  Static method to create a Render API.
	 * @param[in] iType Type of API.
	 * @return Render.
	 */
	static auto create(const Type& iType) -> uniq<RenderAPI>;

	/**
	 * @brief
	 *  Get the actual API state.
	 * @return API State.
	 */
	[[nodiscard]] auto getState() const -> State { return m_state; }

	/**
	 * @brief
	 *  Check if the API type require initializations.
	 * @return tRue if initialization required.
	 */
	[[nodiscard]] auto requireInit() const -> bool { return m_type == Type::OpenGL || m_type == Type::Vulkan; }

	/**
	 * @brief
	 *  Reset value for the frame to render.
	 */
	virtual void beginFrame() {}

	/**
	 * @brief
	 *  Reset value for the batch to render.
	 */
	virtual void beginBatch() {}


	/**
	 * @brief
	 *  Ends draw call for the current batch.
	 */
	virtual void endBatch() {}

	/**
	 * @brief
	 *  Change the subpass.
	 */
	virtual void nextSubpass() {}

	/**
	 * @brief
	 *  Ends draw call for the current frame.
	 */
	virtual void endFrame() {}


	/**
	 * @brief
	 *  Fence between compute writes to SSBOs and downstream graphics reads
	 *  (vertex-attribute pulls, vertex/fragment shader SSBO/UBO loads).
	 *  Must be issued by any caller that runs a `ComputeShader::dispatch()`
	 *  before consuming the output buffer in the draw pass. No-op on the
	 *  null backend.
	 */
	virtual void storageBufferMemoryBarrier() {}

	/**
	 * @brief
	 *  GPU-driven indexed indirect draw. Reads `iMaxDrawCount`
	 *  `DrawIndexedIndirectCommand` records from `iCommandBuffer`
	 *  (starting at offset 0) and `uint32_t` drawCount from
	 *  `iCountBuffer` at offset 0. The actual emitted draw count is
	 *  clamped to `min(*iCountBuffer, iMaxDrawCount)`. Vulkan: maps to
	 *  `vkCmdDrawIndexedIndirectCount`. OpenGL: maps to
	 *  `glMultiDrawElementsIndirectCount`. Null: no-op.
	 * @param[in] iData Draw data with bound vertex / index buffers.
	 * @param[in] iCommandBuffer SSBO carrying the indirect commands
	 *  (also usable as VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT /
	 *  GL_DRAW_INDIRECT_BUFFER).
	 * @param[in] iCountBuffer Single-uint SSBO carrying the actual
	 *  command count.
	 * @param[in] iMaxDrawCount Upper bound on the emitted draw count.
	 */
	virtual void drawIndexedIndirect([[maybe_unused]] const shared<DrawData>& iData,
									 [[maybe_unused]] const shared<StorageBuffer>& iCommandBuffer,
									 [[maybe_unused]] const shared<StorageBuffer>& iCountBuffer,
									 [[maybe_unused]] uint32_t iMaxDrawCount) {}

	/**
	 * @brief
	 *  Check whether the backend can measure GPU time with timestamp queries.
	 * @return True when `setGpuTimestampsEnabled(true)` produces timings.
	 */
	[[nodiscard]] virtual auto hasGpuTimestamps() const -> bool { return false; }

	/**
	 * @brief
	 *  Start or stop the per-frame GPU timestamp queries (off by default; no-op on the null backend).
	 * @param[in] iEnabled True to time the next frames.
	 */
	virtual void setGpuTimestampsEnabled([[maybe_unused]] bool iEnabled) {}

	/**
	 * @brief
	 *  Get the number of the frame being recorded, counted from the first timed frame.
	 * @return The frame number, 0 when timing is off or unsupported.
	 */
	[[nodiscard]] virtual auto getGpuFrameId() const -> uint64_t { return 0; }

	/**
	 * @brief
	 *  Hand over the GPU timings of the frames completed since the last call.
	 *
	 * Results arrive a few frames late (the queries are read once the GPU is done with them), in frame order.
	 * @return The completed timings, oldest first.
	 */
	virtual auto popGpuFrameTimings() -> std::vector<GpuFrameTiming> { return {}; }

	/**
	 * @brief
	 *  Get the cumulative backend counters (submissions, queue and device drains).
	 * @return The counters since start-up; `drawCalls` is filled by `RenderCommand`.
	 */
	[[nodiscard]] virtual auto getRenderCounters() const -> RenderCounters { return {}; }

	/**
	 * @brief
	 *  Record the vertical synchronisation request. On Vulkan it selects the present mode of the next swap chain
	 *  creation (FIFO or MAILBOX when on, IMMEDIATE when off and available); OpenGL uses the window swap interval.
	 * @param[in] iEnabled True to synchronise presentation with the display.
	 */
	virtual void setVSync([[maybe_unused]] bool iEnabled) {}

	/**
	 * @brief
	 *  Describe how frames are presented.
	 * @return The present mode (`fifo`, `mailbox`, `immediate`, `swap-interval-0`, ...), `none` without display.
	 */
	[[nodiscard]] virtual auto getPresentMode() const -> std::string { return "none"; }

	/**
	 * @brief
	 *  Get the name of the GPU the backend runs on.
	 * @return The device name, `none` for the null backend.
	 */
	[[nodiscard]] virtual auto getDeviceName() const -> std::string { return "none"; }

protected:
	/**
	 * @brief
	 *  Define the API State.
	 * @param[in] iState The new API State.
	 */
	void setState(const State& iState) { m_state = iState; }

private:
	/// Type of Renderer API.
	Type m_type = Type::Null;
	/// The current state of the API.
	State m_state = State::Created;
};

}// namespace owl::renderer::gpu
