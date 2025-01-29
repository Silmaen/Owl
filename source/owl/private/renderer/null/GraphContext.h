/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/external/glfw3.h"
#include "renderer/GraphContext.h"

namespace owl::renderer::null {
/**
 * @brief Specialized class for manipulate null graph context.
 */
class OWL_API GraphContext final : public ::owl::renderer::GraphContext {
public:
	GraphContext(const GraphContext&) = delete;
	GraphContext(GraphContext&&) = delete;
	auto operator=(const GraphContext&) -> GraphContext& = delete;
	auto operator=(GraphContext&&) -> GraphContext& = delete;

	/**
	 * @brief Default constructor.
	 * @param[in,out] ioWindow The window into render context.
	 */
	explicit GraphContext(GLFWwindow* ioWindow);

	/**
	 * @brief Destructor.
	 */
	~GraphContext() override = default;

	/**
	 * @brief Initialize the context.
	 */
	void init() override;

	/**
	 * @brief Doo the buffer swap.
	 */
	void swapBuffers() override;
	/**
	 * @brief Get version number of the backend API.
	 * @return The version number.
	 */
	[[nodiscard]] auto getVersion() const -> Version override { return {0, 0}; }
};
}// namespace owl::renderer::null
