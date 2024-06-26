/**
 * @file SpriteRenderer.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/Texture.h"

namespace owl::scene::component {

/**
 * @brief Struct SpriteRenderer.
 */
struct OWL_API SpriteRenderer {
	/// Sprite color.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Sprite's texture.
	shared<renderer::Texture2D> texture = nullptr;
	/// Texture's tiling factor.
	float tilingFactor = 1.0f;
};

}// namespace owl::scene::component
