/**
 * @file SpriteRenderer.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "renderer/Texture.h"

namespace owl::scene::component {

/**
 * @brief Struct for draw a quad sprite.
 */
struct OWL_API SpriteRenderer {
	/// Sprite color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Sprite's texture.
	shared<renderer::Texture2D> texture = nullptr;
	/// Texture's tiling factor.
	float tilingFactor = 1.0f;
	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Sprite Renderer"; }
	/**
	 * @brief Get the YAML key for this component
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SpriteRenderer"; }

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
