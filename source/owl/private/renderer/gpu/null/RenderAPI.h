/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 30/07/2027
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

/**
 * @brief
 *  Headless / null-backend implementations of the renderer abstractions.
 *
 * Used by unit tests and headless server contexts: every class accepts the
 * same calls as its Vulkan / OpenGL counterpart but performs no GPU work,
 * so the engine can run without a graphics device. Selected via
 * `RenderCommand::create(RenderAPI::Type::Null)`.
 */
namespace owl::renderer::gpu::null {
/**
 * @brief
 *  Specialized class to manage null rendering API.
 */
class RenderAPI final : public renderer::gpu::RenderAPI {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	RenderAPI() : renderer::gpu::RenderAPI(Type::Null) {}

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
	 *  Null-backend stub for instanced draws.
	 * @param[in] iData Draw data.
	 * @param[in] iIndexCount Indices per instance.
	 * @param[in] iInstanceCount Instance count.
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
};
}// namespace owl::renderer::gpu::null
