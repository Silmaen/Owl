/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/glfw3.h"
#include "renderer/GraphContext.h"

namespace owl::renderer::vulkan {
/**
 * @brief Specialized class for manipulate Vulkan graph context.
 */
class OWL_API GraphContext final : public ::owl::renderer::GraphContext {
public:
	GraphContext(const GraphContext&) = delete;
	GraphContext(GraphContext&&) = delete;
	auto operator=(const GraphContext&) -> GraphContext& = delete;
	auto operator=(GraphContext&&) -> GraphContext& = delete;

	/**
	 * @brief Default constructor.
	 * @param[in] ioWindow The windows draw surface.
	 */
	explicit GraphContext(GLFWwindow* ioWindow);

	/**
	 * @brief Destructor.
	 */
	~GraphContext() override;

	/**
	 * @brief Initialize the context.
	 */
	void init() override;

	/**
	 * @brief Do the buffer swap.
	 */
	void swapBuffers() override;
	/**
	 * @brief Get version number of the backend API.
	 * @return The version number.
	 */
	[[nodiscard]] auto getVersion() const -> Version override;

	/**
	 * @brief Create the Window surface.
	 * @param[in] iInstance The Vulkan instance
	 * @return The operation result.
	 */
	auto createSurface(const VkInstance& iInstance) -> VkResult;

	/**
	 * @brief Destroy the Window surface.
	 * @param[in] iInstance The Vulkan instance
	 */
	void destroySurface(const VkInstance& iInstance);

	/**
	 * \brief Access to the vulkn surface.
	 * \return The vulkan surface.
	 */
	[[nodiscard]] auto getSurface() const -> VkSurfaceKHR { return m_surface; }
	/**
	 * @brief Wait for device ready.
	 */
	void waitIdle() override;

private:
	/// The window.
	GLFWwindow* mp_wnd = nullptr;
	/// the presenttin surface.
	VkSurfaceKHR m_surface = nullptr;
};
}// namespace owl::renderer::vulkan
