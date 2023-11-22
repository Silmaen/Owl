/**
 * @file GraphContext.h
 * @author Silmaen
 * @date 30/07/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../GraphContext.h"
#include "core/external/glfw3.h"

namespace owl::renderer::null {

/**
 * @brief Class GraphContext.
 */
class OWL_API GraphContext : public ::owl::renderer::GraphContext {
public:
	GraphContext(const GraphContext &) = delete;
	GraphContext(GraphContext &&) = delete;

	/**
	 * @brief Default constructor.
	 */
	explicit GraphContext(GLFWwindow *window);

	/**
	 * @brief Destructor.
	 */
	~GraphContext() override = default;//---UNCOVER---

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
	Version getVersion() const override { return {0, 0}; }
};

}// namespace owl::renderer::null
