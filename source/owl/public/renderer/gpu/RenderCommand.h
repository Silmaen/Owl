/**
 * @file RenderCommand.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "RenderAPI.h"
#include "core/Core.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace owl::renderer::gpu {
/**
 * @brief
 *  Class gathering all render commands.
 */
class OWL_API RenderCommand final {
public:
	RenderCommand() = default;

	RenderCommand(const RenderCommand&) = delete;

	RenderCommand(RenderCommand&&) = delete;

	auto operator=(const RenderCommand&) -> RenderCommand& = delete;

	auto operator=(RenderCommand&&) -> RenderCommand& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RenderCommand() = default;

	/**
	 * @brief
	 *  Initialize the renderer.
	 */
	static void init() {
		if (m_renderAPI)
			m_renderAPI->init();
	}

	/**
	 * @brief
	 *  Reset RenderAPI.
	 */
	static void invalidate() { m_renderAPI.reset(); }

	/**
	 * @brief
	 *  Get the state of the API.
	 * @return API state.
	 */
	static auto getState() -> RenderAPI::State;

	/**
	 * @brief
	 *  Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	static void setViewport(const uint32_t iX, const uint32_t iY, const uint32_t iWidth, const uint32_t iHeight) {
		m_renderAPI->setViewport(iX, iY, iWidth, iHeight);
	}

	/**
	 * @brief
	 *  Binding to the definition of background colour.
	 * @param[in] iColor The new background colour.
	 */
	static void setClearColor(const math::vec4& iColor) { m_renderAPI->setClearColor(iColor); }

	/**
	 * @brief
	 *  Binding to clear screen.
	 */
	static void clear() { m_renderAPI->clear(); }

	/**
	 * @brief
	 *  Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	static void drawData(const shared<DrawData>& iData, const uint32_t iIndexCount = 0) {
		++m_drawCallCount;
		m_renderAPI->drawData(iData, iIndexCount);
	}

	/**
	 * @brief
	 *  Instanced draw — used by `RendererTilemap` and any future renderer
	 *  that wants `instanceCount > 1`.
	 * @param[in] iData Draw data initialised via `initInstanced`.
	 * @param[in] iIndexCount Indices per instance.
	 * @param[in] iInstanceCount Number of instances to draw.
	 */
	static void drawDataInstanced(const shared<DrawData>& iData, const uint32_t iIndexCount,
								  const uint32_t iInstanceCount) {
		++m_drawCallCount;
		m_renderAPI->drawDataInstanced(iData, iIndexCount, iInstanceCount);
	}

	/**
	 * @brief
	 *  Set the sampled textures of the active renderer block (slot `i` = `iTextures[i]`) for its next draws.
	 * @param[in] iTextures Textures in slot order.
	 */
	static void bindTextures(const std::span<const shared<Texture2D>> iTextures) {
		if (m_renderAPI)
			m_renderAPI->bindTextures(iTextures);
	}

	/**
	 * @brief
	 *  Create or replace the API base on it type.
	 * @param[in] iType The type of the new render API.
	 */
	static void create(const RenderAPI::Type& iType);

	/**
	 * @brief
	 *  Get the actual API type.
	 * @return API Type.
	 */
	static auto getApi() -> RenderAPI::Type {
		if (m_renderAPI)
			return m_renderAPI->getApi();
		return static_cast<RenderAPI::Type>(-1);// NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
	}

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	static auto getMaxTextureSlots() -> uint32_t {
		if (m_renderAPI)
			return m_renderAPI->getMaxTextureSlots();
		return 0;
	}

	/**
	 * @brief
	 *  Reset value for the frame to render.
	 */
	static void beginFrame() {
		if (m_renderAPI)
			m_renderAPI->beginFrame();
	}

	/**
	 * @brief
	 *  Reset value for the batch to render.
	 */
	static void beginBatch() {
		if (m_renderAPI)
			m_renderAPI->beginBatch();
	}


	/**
	 * @brief
	 *  Ends draw call for the current batch.
	 */
	static void endBatch() {
		if (m_renderAPI)
			m_renderAPI->endBatch();
	}

	/**
	 * @brief
	 *  Change to the next subpass.
	 */
	static void nextSubpass() {
		if (m_renderAPI)
			m_renderAPI->nextSubpass();
	}

	/**
	 * @brief
	 *  Ends draw call for the current frame.
	 */
	static void endFrame() {
		if (m_renderAPI)
			m_renderAPI->endFrame();
	}


	/**
	 * @brief
	 *  Fence compute SSBO writes for downstream graphics reads. Required
	 *  after any `ComputeShader::dispatch()` whose output is consumed by
	 *  the next draw pass.
	 */
	static void storageBufferMemoryBarrier() {
		if (m_renderAPI)
			m_renderAPI->storageBufferMemoryBarrier();
	}

	/**
	 * @brief
	 *  GPU-driven indexed indirect draw. See
	 *  `RenderAPI::drawIndexedIndirect`.
	 * @param[in] iData Draw data with bound vertex / index buffers.
	 * @param[in] iCommandBuffer SSBO of indirect commands.
	 * @param[in] iCountBuffer Single-uint SSBO with the command count.
	 * @param[in] iMaxDrawCount Upper bound on draw count.
	 */
	static void drawIndexedIndirect(const shared<DrawData>& iData, const shared<StorageBuffer>& iCommandBuffer,
									const shared<StorageBuffer>& iCountBuffer, const uint32_t iMaxDrawCount) {
		++m_drawCallCount;
		if (m_renderAPI)
			m_renderAPI->drawIndexedIndirect(iData, iCommandBuffer, iCountBuffer, iMaxDrawCount);
	}

	/**
	 * @brief
	 *  Check if the API type require initializations.
	 * @return True if initialization required.
	 */
	static auto requireInit() -> bool {
		if (m_renderAPI)
			return m_renderAPI->requireInit();
		return false;
	}

	/**
	 * @brief
	 *  Check whether the backend can measure GPU time with timestamp queries.
	 * @return True when GPU frame timings are available.
	 */
	static auto hasGpuTimestamps() -> bool {
		if (m_renderAPI)
			return m_renderAPI->hasGpuTimestamps();
		return false;
	}

	/**
	 * @brief
	 *  Start or stop the per-frame GPU timestamp queries.
	 * @param[in] iEnabled True to time the next frames.
	 */
	static void setGpuTimestampsEnabled(const bool iEnabled) {
		if (m_renderAPI)
			m_renderAPI->setGpuTimestampsEnabled(iEnabled);
	}

	/**
	 * @brief
	 *  Get the number of the frame being recorded (see `RenderAPI::getGpuFrameId`).
	 * @return The frame number, 0 when timing is off or unsupported.
	 */
	static auto getGpuFrameId() -> uint64_t {
		if (m_renderAPI)
			return m_renderAPI->getGpuFrameId();
		return 0;
	}

	/**
	 * @brief
	 *  Hand over the GPU timings of the frames completed since the last call.
	 * @return The completed timings, oldest first.
	 */
	static auto popGpuFrameTimings() -> std::vector<GpuFrameTiming> {
		if (m_renderAPI)
			return m_renderAPI->popGpuFrameTimings();
		return {};
	}

	/**
	 * @brief
	 *  Get the cumulative render counters (draw calls, submissions, queue and device drains).
	 * @return The counters since start-up.
	 */
	static auto getRenderCounters() -> RenderCounters {
		RenderCounters counters;
		if (m_renderAPI)
			counters = m_renderAPI->getRenderCounters();
		counters.drawCalls = m_drawCallCount;
		return counters;
	}

	/**
	 * @brief
	 *  Record the vertical synchronisation request (see `RenderAPI::setVSync`).
	 * @param[in] iEnabled True to synchronise presentation with the display.
	 */
	static void setVSync(const bool iEnabled) {
		if (m_renderAPI)
			m_renderAPI->setVSync(iEnabled);
	}

	/**
	 * @brief
	 *  Describe how frames are presented.
	 * @return The present mode name.
	 */
	static auto getPresentMode() -> std::string {
		if (m_renderAPI)
			return m_renderAPI->getPresentMode();
		return "none";
	}

	/**
	 * @brief
	 *  Get the name of the GPU the backend runs on.
	 * @return The device name.
	 */
	static auto getDeviceName() -> std::string {
		if (m_renderAPI)
			return m_renderAPI->getDeviceName();
		return "none";
	}

private:
	/// Pointer to the render API
	static uniq<RenderAPI> m_renderAPI;
	/// Draw calls issued since start-up.
	static uint64_t m_drawCallCount;
};
}// namespace owl::renderer::gpu
