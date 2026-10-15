/**
 * @file UiToolCursor.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

namespace owl::scene::component {
/**
 * @brief
 *  Tool cursor: a sprite following the mouse over the HUD while a construction tool is active ("build road", "raise
 *  land").
 *
 * It needs no `UiRect`: it is drawn last, over the windows and the tooltips, at the mouse position, only in play and
 * only while `active`. A script switches it with `ui.set_tool_cursor`; the first active one found (HUD asset first)
 * is drawn.
 */
struct OWL_API UiToolCursor {
	/// Sprite of the cursor.
	shared<renderer::gpu::Texture2D> texture = nullptr;
	/// Colour multiplied with the texture.
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	/// Size of the sprite, in reference pixels.
	math::vec2 size{32.f, 32.f};
	/// Point of the sprite put on the mouse, normalised ((0, 0) = top-left, (1, 1) = bottom-right).
	math::vec2 hotspot{0.5f, 0.5f};
	/// Whether the cursor is drawn.
	bool active = false;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Tool Cursor"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiToolCursor"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
