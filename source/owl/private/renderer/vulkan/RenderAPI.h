/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/RenderAPI.h"

/**
 * @brief Namespace for vulkan specific rendering objects.
 */
namespace owl::renderer::vulkan {
/**
 * @brief Specialized class to manage Vulkan rendering API.
 */
class OWL_API RenderAPI final : public renderer::RenderAPI {
public:
	RenderAPI() : renderer::RenderAPI(Type::Vulkan) {}
	RenderAPI(const RenderAPI&) = delete;
	RenderAPI(RenderAPI&&) = delete;
	auto operator=(const RenderAPI&) -> RenderAPI& = delete;
	auto operator=(RenderAPI&&) -> RenderAPI& = delete;

	/**
	 * @brief Destructor.
	 */
	~RenderAPI() override;

	/**
	 * @brief Initialize the renderer.
	 */
	void init() override;

	/**
	 * @brief Define the view port for this API.
	 * @param[in] iX Starting X coordinate.
	 * @param[in] iY Starting Y coordinate.
	 * @param[in] iWidth Viewport's width.
	 * @param[in] iHeight Viewport Height.
	 */
	void setViewport(uint32_t iX, uint32_t iY, uint32_t iWidth, uint32_t iHeight) override;

	/**
	 * @brief Define the background color.
	 * @param[in] iColor The background color.
	 */
	void setClearColor(const math::vec4& iColor) override;

	/**
	 * @brief Clear the screen.
	 */
	void clear() override;

	/**
	 * @brief Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawData(const shared<DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief Binding the draw of vertex array as line.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawLine(const shared<DrawData>& iData, uint32_t iIndexCount) override;

	/**
	 * @brief Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] auto getMaxTextureSlots() const -> uint32_t override { return 16; }

	/**
	 * @brief Reset value for the frame to render.
	 */
	void beginFrame() override;
	/**
		 * @brief Reset value for the batch to render.
		 */
	void beginBatch() override;
	/**
	 * @brief Reset value for the texture load.
	 */
	void beginTextureLoad() override;

	/**
	 * @brief Ends texture load.
	 */
	void endTextureLoad() override;
	/**
	 * @brief Ends draw call for the current batch.
	 */
	void endBatch() override;

	/**
	 * @brief Change the subpass.
	 */
	void nextSubpass() override;

	/**
	 * @brief Ends draw call for the current frame.
	 */
	void endFrame() override;

	/**
	 * @brief Enable or disable depth buffer writing.
	 * @param[in] iEnabled True to enable depth writing, false to disable.
	 */
	void setDepthMask(bool iEnabled) override;
};
}// namespace owl::renderer::vulkan
