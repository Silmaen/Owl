/**
 * @file CircleRenderer.h
 * @author Silmaen
 * @date 14/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief Struct for component drawing a circle.
 */
struct OWL_API CircleRenderer {
	/// The circle color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// The circle thickness.
	float thickness = 1.0f;
	/// The circle fading.
	float fade = 0.005f;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Circle Renderer"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "CircleRenderer"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
