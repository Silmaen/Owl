/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 07/01/2024
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/GraphContext.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace owl::renderer::gpu::vulkan {
/**
 * @brief
 *  Specialized class for manipulate Vulkan graph context.
 */
class OWL_API GraphContext final : public ::owl::renderer::gpu::GraphContext {
public:
	GraphContext(const GraphContext&) = delete;

	GraphContext(GraphContext&&) = delete;

	auto operator=(const GraphContext&) -> GraphContext& = delete;

	auto operator=(GraphContext&&) -> GraphContext& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] ioWindow The native window to draw in: a `GLFWwindow` on the desktop, an `ANativeWindow` on Android.
	 */
	explicit GraphContext(void* ioWindow);

	/**
	 * @brief
	 *  Destructor.
	 */
	~GraphContext() override;

	/**
	 * @brief
	 *  Initialize the context.
	 */
	void init() override;

	/**
	 * @brief
	 *  Do the buffer swap.
	 */
	void swapBuffers() override;

	/**
	 * @brief
	 *  Get version number of the backend API.
	 * @return The version number.
	 */
	[[nodiscard]] auto getVersion() const -> Version override;

	/**
	 * @brief
	 *  Create the Window surface.
	 * @param[in] iInstance The Vulkan instance.
	 * @return The operation result.
	 */
	auto createSurface(const VkInstance& iInstance) -> VkResult;

	/**
	 * @brief
	 *  Instance extensions the window system needs to create a surface.
	 * @return The extension names.
	 */
	[[nodiscard]] static auto getRequiredInstanceExtensions() -> std::vector<const char*>;

	/**
	 * @brief
	 *  Destroy the Window surface.
	 * @param[in] iInstance The Vulkan instance.
	 */
	void destroySurface(const VkInstance& iInstance);

	/**
	 * @brief
	 *  Access to the Vulkan surface.
	 * @return The Vulkan surface.
	 */
	[[nodiscard]] auto getSurface() const noexcept -> VkSurfaceKHR { return m_surface; }

	/**
	 * @brief
	 *  Wait for device ready.
	 */
	void waitIdle() override;

private:
	/// The native window.
	void* mp_wnd = nullptr;
	/// the presenting surface.
	VkSurfaceKHR m_surface = nullptr;
};
}// namespace owl::renderer::gpu::vulkan
