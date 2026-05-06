/**
 * @file UIImage.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "renderer/gpu/Texture.h"

namespace owl::scene::component {

/**
 * @brief UI image widget component.
 *
 * Renders a textured quad in screen space within a Canvas hierarchy.
 */
struct OWL_API UIImage {
	/// Texture to display.
	shared<renderer::gpu::Texture2D> texture = nullptr;
	/// Tint colour (multiplied with texture).
	math::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};

	/// @brief Get the class title.
	static auto name() -> const char* { return "UI Image"; }
	/// @brief Get the YAML key.
	static auto key() -> const char* { return "UIImage"; }

	/// @brief Write this component to a YAML context.
	void serialize(const core::Serializer& iOut) const;
	/// @brief Read this component from YAML node.
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
