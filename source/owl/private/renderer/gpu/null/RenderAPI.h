/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 30/07/2027
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/RenderAPI.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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
class OWL_API RenderAPI final : public renderer::gpu::RenderAPI {
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
	 *  Record the pipeline state of the draw (no GPU work).
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawData(const shared<renderer::gpu::DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief
	 *  Record the pipeline state of the instanced draw (no GPU work).
	 * @param[in] iData Draw data.
	 * @param[in] iIndexCount Indices per instance.
	 * @param[in] iInstanceCount Instance count.
	 */
	void drawDataInstanced(const shared<renderer::gpu::DrawData>& iData, uint32_t iIndexCount,
						   uint32_t iInstanceCount) override;

	/**
	 * @brief
	 *  Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override { return 16; }

	/**
	 * @brief
	 *  Record the number of textures bound (no GPU work).
	 * @param[in] iTextures Textures in slot order.
	 */
	void bindTextures(std::span<const shared<renderer::gpu::Texture2D>> iTextures) override;

	/**
	 * @brief
	 *  Start a frame: forget the draws recorded so far.
	 */
	void beginFrame() override { m_drawnStates.clear(); }

	/**
	 * @brief
	 *  Get the pipeline states of the draws issued since the last `beginFrame` (the first 256 of them).
	 * @return The states, in draw order.
	 */
	[[nodiscard]] auto getDrawnStates() const -> const std::vector<PipelineState>& { return m_drawnStates; }

	/**
	 * @brief
	 *  Get the number of textures given to the last `bindTextures`.
	 * @return The texture count.
	 */
	[[nodiscard]] auto getBoundTextureCount() const -> size_t { return m_boundTextureCount; }

private:
	/**
	 * @brief
	 *  Keep the state of one draw.
	 * @param[in] iData The drawn data.
	 */
	void recordDraw(const shared<renderer::gpu::DrawData>& iData);

	/// Pipeline states of the draws since the last `beginFrame`.
	std::vector<PipelineState> m_drawnStates;
	/// Texture count of the last `bindTextures`.
	size_t m_boundTextureCount = 0;
};
}// namespace owl::renderer::gpu::null
