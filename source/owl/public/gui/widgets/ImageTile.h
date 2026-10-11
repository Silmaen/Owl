/**
 * @file ImageTile.h
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::gui::widgets {

/**
 * @brief
 *  Clicks on an image tile this frame.
 */
struct ImageTileResult {
	/// Left button clicked.
	bool left = false;
	/// Right button clicked.
	bool right = false;
	/// Mouse over the tile.
	bool hovered = false;
};

/**
 * @brief
 *  What an image tile shows.
 */
struct ImageTileSpec {
	/// Renderer id of the texture, 0 for an empty well.
	uint64_t textureId = 0;
	/// Texture coordinates of the top-left corner.
	math::vec2 uv0{0.f, 0.f};
	/// Texture coordinates of the bottom-right corner.
	math::vec2 uv1{1.f, 1.f};
	/// Side of the tile, in pixels.
	float size = 32.f;
	/// Outline of a selected tile (alpha 0 for none): the accent for the main choice, another token for a second one.
	math::vec4 outline{0.f, 0.f, 0.f, 0.f};
};

/**
 * @brief
 *  Square swatch of a texture region (a tile, a block face) on the input well, outlined when selected and lit on
 *  hover; for palettes. The tooltip is the caller's (`itemTooltip` right after).
 * @param[in] iId Unique ImGui identifier.
 * @param[in] iSpec What to show.
 * @return The clicks of this frame.
 */
OWL_API auto imageTile(const char* iId, const ImageTileSpec& iSpec) -> ImageTileResult;

}// namespace owl::gui::widgets
