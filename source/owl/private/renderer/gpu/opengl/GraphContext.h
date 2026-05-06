/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 07/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/GraphContext.h"
#include "core/external/glfw3.h"

namespace owl::renderer::gpu::opengl {
/**
 * @brief
 *  Specialized class for manipulate OpenGL graph context.
 */
class OWL_API GraphContext final : public renderer::gpu::GraphContext {
public:
	GraphContext(const GraphContext&) = delete;

	GraphContext(GraphContext&&) = delete;

	auto operator=(const GraphContext&) -> GraphContext& = delete;

	auto operator=(GraphContext&&) -> GraphContext& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in,out] ioWindow The window into render context.
	 */
	explicit GraphContext(GLFWwindow* ioWindow);

	/**
	 * @brief
	 *  Destructor.
	 */
	~GraphContext() override = default;

	/**
	 * @brief
	 *  Initialize the context.
	 */
	void init() override;

	/**
	 * @brief
	 *  Doo the buffer swap.
	 */
	void swapBuffers() override;

	/**
	 * @brief
	 *  Get version number of the backend API.
	 * @return The version number.
	 */
	[[nodiscard]] auto getVersion() const -> Version override;

private:
	/// Link to the parent window.
	GLFWwindow* mp_windowHandle;
	/// OpenGL version
	int m_version = 0;
};
}// namespace owl::renderer::gpu::opengl
