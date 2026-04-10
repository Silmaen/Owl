/**
 * @file UIRect.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {

/**
 * @brief UI rectangle component — defines layout for UI entities.
 *
 * Provides anchoring, pivot, size and offset for screen-space positioning
 * within a Canvas hierarchy. Used instead of Transform for UI layout.
 */
struct OWL_API UIRect {
	/// Anchor presets for positioning within the parent.
	enum struct Anchor : uint8_t {
		TopLeft,///< Top-left of parent.
		TopCenter,///< Top-center of parent.
		TopRight,///< Top-right of parent.
		MiddleLeft,///< Middle-left of parent.
		Center,///< Center of parent.
		MiddleRight,///< Middle-right of parent.
		BottomLeft,///< Bottom-left of parent.
		BottomCenter,///< Bottom-center of parent.
		BottomRight,///< Bottom-right of parent.
	};
	/// Anchor preset.
	Anchor anchor = Anchor::Center;
	/// Normalized pivot point (0,0 = top-left, 1,1 = bottom-right).
	math::vec2 pivot = {0.5f, 0.5f};
	/// Size in pixels.
	math::vec2 size = {100.f, 100.f};
	/// Offset from anchor position in pixels.
	math::vec2 anchorOffset = {0.f, 0.f};

	/**
	 * @brief Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "UI Rect"; }
	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "UIRect"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);

	/**
	 * @brief Compute the screen-space position of this UI rect within a parent of given size.
	 * @param[in] iParentSize The parent's size in pixels.
	 * @return The top-left corner position in pixels (relative to parent top-left).
	 */
	[[nodiscard]] auto computePosition(const math::vec2& iParentSize) const -> math::vec2;
};

}// namespace owl::scene::component
