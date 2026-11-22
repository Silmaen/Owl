/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/GraphContext.h"

struct GLFWwindow;

namespace owl::renderer::gpu::webgpu {

/**
 * @brief
 *  Graphics context of the Web: the canvas of the page as a WebGPU surface.
 */
class OWL_API GraphContext final : public ::owl::renderer::gpu::GraphContext {
public:
	GraphContext(const GraphContext&) = delete;
	GraphContext(GraphContext&&) = delete;
	auto operator=(const GraphContext&) -> GraphContext& = delete;
	auto operator=(GraphContext&&) -> GraphContext& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] ioWindow The GLFW window of the canvas.
	 */
	explicit GraphContext(GLFWwindow* ioWindow);

	/**
	 * @brief
	 *  Destructor.
	 */
	~GraphContext() override = default;

	/**
	 * @brief
	 *  Takes the device of the page and makes the surface of the canvas.
	 */
	void init() override;

	/**
	 * @brief
	 *  Nothing to do: the browser shows the canvas when the frame returns to it.
	 */
	void swapBuffers() override;

	/**
	 * @brief
	 *  Version of the API.
	 * @return WebGPU 1.0.
	 */
	[[nodiscard]] auto getVersion() const -> Version override { return {.major = 1, .minor = 0}; }

private:
	/// The GLFW window of the canvas.
	GLFWwindow* mp_window = nullptr;
};

}// namespace owl::renderer::gpu::webgpu
