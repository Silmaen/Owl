/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

/**
 * @brief
 *  OpenGL 4.5-backed implementations of the renderer abstractions.
 *
 * Each class derives from the matching `owl::renderer::gpu::*` base and
 * implements the abstract API against `glad`-loaded GL 4.5 entry points
 * (DSA-style where available). Selected via
 * `RenderCommand::create(RenderAPI::Type::OpenGL)`.
 */
namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  Specialized class to manage OpenGL rendering API.
 */
class RenderAPI final : public renderer::gpu::RenderAPI {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	RenderAPI() : renderer::gpu::RenderAPI(Type::OpenGL) {}

	RenderAPI(const RenderAPI&) = delete;

	RenderAPI(RenderAPI&&) = delete;

	auto operator=(const RenderAPI&) -> RenderAPI& = delete;

	auto operator=(RenderAPI&&) -> RenderAPI& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RenderAPI() override;

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	void init() override;

	/**
	 * @brief
	 *  Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) override;

	/**
	 * @brief
	 *  Define the background colour.
	 * @param[in] iColor The background colour.
	 */
	void setClearColor(const math::vec4& iColor) override;

	/**
	 * @brief
	 *  Clear the screen.
	 */
	void clear() override;

	/**
	 * @brief
	 *  Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawData(const shared<DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief
	 *  Issue an instanced draw via `glDrawElementsInstanced`.
	 * @param[in] iData Draw data initialised via `initInstanced`.
	 * @param[in] iIndexCount Indices per instance.
	 * @param[in] iInstanceCount Number of instances.
	 */
	void drawDataInstanced(const shared<DrawData>& iData, uint32_t iIndexCount, uint32_t iInstanceCount) override;

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override;

	/**
	 * @brief
	 *  Bind the textures to units `0..N-1` (`glBindTextures`) and record them in the active renderer block.
	 * @param[in] iTextures Textures in unit order.
	 */
	void bindTextures(std::span<const shared<Texture2D>> iTextures) override;

	/**
	 * @brief
	 *  Begin a frame: opens the frame GPU zone when profiling with Tracy and, when GPU timestamps are
	 *  enabled, reads back the timestamp slot it reuses and writes the begin timestamp.
	 */
	void beginFrame() override;

	/**
	 * @brief
	 *  End a frame: closes the frame GPU zone when profiling with Tracy and writes the end timestamp.
	 */
	void endFrame() override;


	/**
	 * @brief
	 *  `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
	 *  GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT)` — flush compute SSBO writes for
	 *  the next draw pass.
	 */
	void storageBufferMemoryBarrier() override;

	/**
	 * @brief
	 *  `glMultiDrawElementsIndirectCount` against an SSBO bound to
	 *  `GL_DRAW_INDIRECT_BUFFER` + a parameter buffer bound to
	 *  `GL_PARAMETER_BUFFER`. Used by the compute-culling pipeline (#34).
	 */
	void drawIndexedIndirect(const shared<DrawData>& iData, const shared<renderer::gpu::StorageBuffer>& iCommandBuffer,
							 const shared<renderer::gpu::StorageBuffer>& iCountBuffer, uint32_t iMaxDrawCount) override;

	/**
	 * @brief
	 *  Check whether the context can write `GL_TIMESTAMP` queries.
	 * @return True when GPU frame timings are available.
	 */
	[[nodiscard]] auto hasGpuTimestamps() const -> bool override;

	/**
	 * @brief
	 *  Start or stop the `glQueryCounter` timestamps around every frame.
	 * @param[in] iEnabled True to time the next frames.
	 */
	void setGpuTimestampsEnabled(bool iEnabled) override;

	/**
	 * @brief
	 *  Get the number of the frame being recorded.
	 * @return The frame number, 0 when timing is off.
	 */
	[[nodiscard]] auto getGpuFrameId() const -> uint64_t override { return m_timingEnabled ? m_frameId : 0; }

	/**
	 * @brief
	 *  Hand over the GPU timings read back since the last call.
	 * @return The completed timings, oldest first.
	 */
	auto popGpuFrameTimings() -> std::vector<GpuFrameTiming> override;

	/**
	 * @brief
	 *  Record the swap interval requested through the window.
	 * @param[in] iEnabled True for a swap interval of 1.
	 */
	void setVSync(const bool iEnabled) override { m_vSync = iEnabled; }

	/**
	 * @brief
	 *  Name the swap interval in use.
	 * @return `swap-interval-1` or `swap-interval-0`.
	 */
	[[nodiscard]] auto getPresentMode() const -> std::string override {
		return m_vSync ? "swap-interval-1" : "swap-interval-0";
	}

	/**
	 * @brief
	 *  Get the `GL_RENDERER` string.
	 * @return The device name.
	 */
	[[nodiscard]] auto getDeviceName() const -> std::string override;

private:
	/**
	 * @brief
	 *  Read back one timestamp slot and push its timing.
	 * @param[in] iSlot The slot index.
	 */
	void harvest(size_t iSlot);

	/**
	 * @brief
	 *  Prepare a draw: apply the fixed-function state that differs from the previous draw, bind the draw data and
	 *  re-apply the bindings of the active renderer block when another block changed them.
	 * @param[in] iData The draw data.
	 * @return The GL primitive mode of the draw.
	 */
	auto prepareDraw(const shared<DrawData>& iData) -> uint32_t;

	/// Fixed-function state applied by the last draw (unset until the first one).
	std::optional<PipelineState> m_appliedState;

	/// Frames kept in the query ring.
	static constexpr size_t g_slotCount = 4;
	/// Begin and end timestamp query of each slot.
	std::array<std::array<uint32_t, 2>, g_slotCount> m_queries{};
	/// Frame number recorded in each slot (0 = empty).
	std::array<uint64_t, g_slotCount> m_slotFrame{};
	/// Frames read back and not handed over yet.
	std::vector<GpuFrameTiming> m_completed;
	/// Number of the frame being recorded.
	uint64_t m_frameId{0};
	/// True once the query objects exist.
	bool m_queriesCreated{false};
	/// True while timestamps are recorded.
	bool m_timingEnabled{false};
	/// True while the begin timestamp of the current frame is written and the end one is not.
	bool m_frameOpen{false};
	/// Swap interval requested through the window.
	bool m_vSync{true};
};
}// namespace owl::renderer::gpu::opengl
