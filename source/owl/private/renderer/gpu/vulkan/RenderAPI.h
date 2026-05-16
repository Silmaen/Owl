/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

/**
 * @brief
 *  Vulkan 1.4-backed implementations of the renderer abstractions.
 *
 * Each class derives from the matching `owl::renderer::gpu::*` base.
 * Internal helpers (instance / device / swapchain handlers, descriptor
 * sets, physical-device capability queries) live in
 * `owl::renderer::gpu::vulkan::internal`. Selected via
 * `RenderCommand::create(RenderAPI::Type::Vulkan)`.
 */
namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class to manage Vulkan rendering API.
 */
class OWL_API RenderAPI final : public renderer::gpu::RenderAPI {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	RenderAPI() : renderer::gpu::RenderAPI(Type::Vulkan) {}

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
	 *  Issue an instanced draw via `vkCmdDrawIndexed` with instanceCount > 1.
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
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override { return 16; }

	/**
	 * @brief
	 *  Reset value for the frame to render.
	 */
	void beginFrame() override;

	/**
	 * @brief
	 *  Reset value for the batch to render.
		 */
	void beginBatch() override;

	/**
	 * @brief
	 *  Reset value for the texture load.
	 */
	void beginTextureLoad() override;

	/**
	 * @brief
	 *  Ends texture load.
	 */
	void endTextureLoad() override;

	/**
	 * @brief
	 *  Ends draw call for the current batch.
	 */
	void endBatch() override;

	/**
	 * @brief
	 *  Change the subpass.
	 */
	void nextSubpass() override;

	/**
	 * @brief
	 *  Ends draw call for the current frame.
	 */
	void endFrame() override;

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
};
}// namespace owl::renderer::gpu::vulkan
