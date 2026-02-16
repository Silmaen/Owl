/**
 * @file BackgroundTexture.h
 * @author Silmaen
 * @date 02/15/26
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "renderer/Texture.h"

namespace owl::scene::component {

/**
 * @brief Struct for a background or skybox component.
 */
struct OWL_API BackgroundTexture {
	/// Background display mode.
	enum struct Mode : uint8_t { Background = 0, Skybox = 1 };
	/// Background type (used in Background mode).
	enum struct Type : uint8_t { SolidColor = 0, Gradient = 1, Texture = 2 };

	/// Display mode.
	Mode mode = Mode::Background;
	/// Background type.
	Type type = Type::SolidColor;
	/// Main color / bottom color for gradient.
	math::vec4 color{0.2f, 0.3f, 0.8f, 1.0f};
	/// Top color for gradient.
	math::vec4 topColor{0.8f, 0.9f, 1.0f, 1.0f};
	/// Texture (background or equirectangular for skybox).
	shared<renderer::Texture2D> texture = nullptr;

	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Background Texture"; }
	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "BackgroundTexture"; }

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
