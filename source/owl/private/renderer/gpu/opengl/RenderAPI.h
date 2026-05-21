/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

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
	~RenderAPI() override = default;

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
	 *  Binding the draw of vertex array as line.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawLine(const shared<DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief
	 *  Instanced line draw via `glDrawElementsInstanced(GL_LINES, …)`.
	 * @param[in] iData Draw data with index buffer `{0, 1}`.
	 * @param[in] iIndexCount Indices per instance (2).
	 * @param[in] iInstanceCount Number of instances.
	 */
	void drawLineInstanced(const shared<DrawData>& iData, uint32_t iIndexCount, uint32_t iInstanceCount) override;

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override;

	/**
	 * @brief
	 *  Enable or disable depth buffer writing.
	 * @param[in] iEnabled True to enable depth writing, false to disable.
	 */
	void setDepthMask(bool iEnabled) override;

	/**
	 * @brief
	 *  Set the depth test.
	 * @param[in] iEnabled Enable flag.
	 */
	void setDepthTest(bool iEnabled) override;

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
};
}// namespace owl::renderer::gpu::opengl
