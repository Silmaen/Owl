/**
 * @file RenderAPI.h
 * @author Silmaen
 * @date 09/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../RenderAPI.h"

/**
 * @brief Namespace for opengl specific rendering objects.
 */
namespace owl::renderer::opengl {
/**
 * @brief Class RenderAPI.
 */
class RenderAPI final : public renderer::RenderAPI {
public:
	RenderAPI(): renderer::RenderAPI(Type::OpenGL) {}
	RenderAPI(const RenderAPI &) = delete;
	RenderAPI(RenderAPI &&) = delete;
	RenderAPI &operator=(const RenderAPI &) = delete;
	RenderAPI &operator=(RenderAPI &&) = delete;

	/**
	 * @brief Destructor.
	 */
	~RenderAPI() override = default;

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
	void setClearColor(const math::vec4 &iColor) override;

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
	[[nodiscard]] uint32_t getMaxTextureSlots() const override;
};
}// namespace owl::renderer::opengl
