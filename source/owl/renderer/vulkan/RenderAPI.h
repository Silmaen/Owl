/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../RenderAPI.h"

/**
 * @brief Namespace for vulkan specific rendering objects.
 */
namespace owl::renderer::vulkan {
/**
 * @brief Class RenderAPI.
 */
class RenderAPI final : public ::owl::renderer::RenderAPI {
public:
	RenderAPI() = default;
	RenderAPI(const RenderAPI &) = delete;
	RenderAPI(RenderAPI &&) = delete;
	RenderAPI &operator=(const RenderAPI &) = delete;
	RenderAPI &operator=(RenderAPI &&) = delete;

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
	void setClearColor(const glm::vec4 &iColor) override;

	/**
	 * @brief Clear the screen.
	 */
	void clear() override;

	/**
	 * @brief Binding the draw of vertex array.
	 * @param[in] iData Draw data to render.
	 * @param[in] iIndexCount Number of vertex to draw (=0 all).
	 */
	void drawData(const shared<DrawData> &iData, uint32_t iIndexCount) override;

	/**
	 * @brief Define the line width.
	 * @param[in] iWidth New line width.
	 */
	void setLineWidth(float iWidth) override;

	/**
	 * @brief Get the maximum number of texture slots.
	 * @return Number of texture slots.
	 */
	[[nodiscard]] uint32_t getMaxTextureSlots() const override { return 16; }

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
	 * @brief Ends draw call for the current frame.
	 */
	void endFrame() override;
};
}// namespace owl::renderer::vulkan
