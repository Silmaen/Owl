/**
 * @file UiRect.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>
#include <optional>

namespace owl::scene::component {
/**
 * @brief
 *  UI rectangle component: the layout of a HUD widget inside its parent rectangle.
 *
 * Coordinates are Y-down pixels of the canvas reference resolution, the origin at the top left of the parent. The
 * anchors are normalised points of the parent ((0, 0) its top-left corner, (1, 1) its bottom-right corner): equal
 * anchors pin the widget to one point (the 9 presets of `Anchor`), distinct ones stretch it with the parent. The
 * parent of a widget is the nearest ancestor carrying a `UiRect`, or the canvas (the whole screen).
 */
struct OWL_API UiRect {
	/// Anchor presets: both anchors on one point of the parent.
	enum struct Anchor : uint8_t {
		TopLeft,///< Top-left of parent.
		TopCenter,///< Top-centre of parent.
		TopRight,///< Top-right of parent.
		MiddleLeft,///< Middle-left of parent.
		Center,///< Centre of parent.
		MiddleRight,///< Middle-right of parent.
		BottomLeft,///< Bottom-left of parent.
		BottomCenter,///< Bottom-centre of parent.
		BottomRight,///< Bottom-right of parent.
	};

	/**
	 * @brief
	 *  An axis-aligned rectangle in Y-down coordinates.
	 */
	struct OWL_API Bounds {
		/// Top-left corner.
		math::vec2 position{0.f, 0.f};
		/// Width and height.
		math::vec2 size{0.f, 0.f};

		/**
		 * @brief
		 *  Check whether a point lies in the rectangle (edges included).
		 * @param[in] iPoint The point, in the rectangle's coordinates.
		 * @return True when the point is inside.
		 */
		[[nodiscard]] auto contains(const math::vec2& iPoint) const -> bool;

		/**
		 * @brief
		 *  Get the centre of the rectangle.
		 * @return The centre.
		 */
		[[nodiscard]] auto getCenter() const -> math::vec2;

		/**
		 * @brief
		 *  Get the part shared with another rectangle.
		 * @param[in] iOther The other rectangle.
		 * @return The common rectangle, with a zero size when they do not overlap.
		 */
		[[nodiscard]] auto intersect(const Bounds& iOther) const -> Bounds;
	};

	/// Anchor of the top-left corner, normalised in the parent ((0, 0) = top-left, (1, 1) = bottom-right).
	math::vec2 anchorMin = {0.5f, 0.5f};
	/// Anchor of the bottom-right corner, normalised in the parent (equal to `anchorMin` for a fixed size).
	math::vec2 anchorMax = {0.5f, 0.5f};
	/// Normalised point of the widget placed at the anchors plus the offset ((0, 0) = top-left, (1, 1) = bottom-right).
	math::vec2 pivot = {0.5f, 0.5f};
	/// Size in reference pixels, added to the span of the anchors (the whole size when the anchors are equal).
	math::vec2 size = {100.f, 100.f};
	/// Offset of the pivot from its anchor point, in reference pixels (positive Y goes down).
	math::vec2 anchorOffset = {0.f, 0.f};

	/**
	 * @brief
	 *  Get the normalised point of a preset.
	 * @param[in] iAnchor The preset.
	 * @return The point ((0, 0) = top-left, (1, 1) = bottom-right).
	 */
	[[nodiscard]] static auto getAnchorPoint(Anchor iAnchor) -> math::vec2;

	/**
	 * @brief
	 *  Put both anchors on the point of a preset.
	 * @param[in] iAnchor The preset.
	 */
	void setAnchor(Anchor iAnchor);

	/**
	 * @brief
	 *  Get the preset matching the anchors.
	 * @return The preset, or `std::nullopt` when the anchors stretch or sit on another point.
	 */
	[[nodiscard]] auto getAnchor() const -> std::optional<Anchor>;

	/**
	 * @brief
	 *  Compute the rectangle of the widget in its parent.
	 * @param[in] iParent The parent rectangle.
	 * @return The widget rectangle, in the parent's coordinates.
	 */
	[[nodiscard]] auto computeBounds(const Bounds& iParent) const -> Bounds;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "UI Rect"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "UiRect"; }

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
