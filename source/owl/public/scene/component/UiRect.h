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
#include <string_view>

namespace owl::scene::component {
/**
 * @brief
 *  UI rectangle component: the layout of a HUD widget inside its parent rectangle.
 *
 * Coordinates are Y-down pixels of the canvas reference resolution, the origin at the top left of the parent. The
 * anchors are normalised points of the parent ((0, 0) its top-left corner, (1, 1) its bottom-right corner): equal
 * anchors pin the widget to one point (the 9 presets of `Anchor`), distinct ones stretch it with the parent. The
 * parent of a widget is the nearest ancestor carrying a `UiRect`, or the canvas (the whole screen).
 *
 * Each of the four numbers `size.x`, `size.y`, `offset.x`, `offset.y` has a `Unit`: reference pixels (the default,
 * scaled by the canvas scale mode) or a ratio of the width or height of the parent rectangle or of the screen. The
 * ratios are resolved at layout time against the parent and the canvas, in reference pixels (the scale being uniform
 * this is the same as in screen pixels), so a square widget can use `ParentHeight` for both its sizes.
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

	/// Unit of a size or offset number.
	enum struct Unit : uint8_t {
		Pixels,///< Reference pixels, scaled by the canvas scale mode.
		ParentWidth,///< Ratio of the width of the parent rectangle (0.25 = a quarter of it).
		ParentHeight,///< Ratio of the height of the parent rectangle.
		ScreenWidth,///< Ratio of the width of the canvas (the whole screen).
		ScreenHeight,///< Ratio of the height of the canvas (the whole screen).
	};

	/// Axis of a size or an offset.
	enum struct Axis : uint8_t {
		X,///< Horizontal.
		Y,///< Vertical.
	};

	/**
	 * @brief
	 *  The units of the two numbers of a size or an offset.
	 */
	struct OWL_API Units {
		/// Unit of the horizontal number.
		Unit x = Unit::Pixels;
		/// Unit of the vertical number.
		Unit y = Unit::Pixels;

		/**
		 * @brief
		 *  Get the unit of an axis.
		 * @param[in] iAxis The axis.
		 * @return The unit.
		 */
		[[nodiscard]] auto get(const Axis iAxis) const -> Unit { return iAxis == Axis::X ? x : y; }

		/**
		 * @brief
		 *  Set the unit of an axis.
		 * @param[in] iAxis The axis.
		 * @param[in] iUnit The unit.
		 */
		void set(const Axis iAxis, const Unit iUnit) { (iAxis == Axis::X ? x : y) = iUnit; }

		/**
		 * @brief
		 *  Check whether both numbers are in pixels.
		 * @return True when both are in pixels.
		 */
		[[nodiscard]] auto isPixels() const -> bool { return x == Unit::Pixels && y == Unit::Pixels; }

		/**
		 * @brief
		 *  Equality operator.
		 * @param[in] iOther The other units.
		 * @return True when both units are the same.
		 */
		[[nodiscard]] auto operator==(const Units& iOther) const -> bool = default;
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
	/// Size, added to the span of the anchors (the whole size when the anchors are equal); in `sizeUnit`.
	math::vec2 size = {100.f, 100.f};
	/// Offset of the pivot from its anchor point (positive Y goes down); in `offsetUnit`.
	math::vec2 anchorOffset = {0.f, 0.f};
	/// Units of `size`.
	Units sizeUnit;
	/// Units of `anchorOffset`.
	Units offsetUnit;

	/**
	 * @brief
	 *  What the ratio units are relative to, in reference pixels.
	 */
	struct OWL_API Frame {
		/// The parent rectangle.
		Bounds parent;
		/// The canvas size.
		math::vec2 screen{0.f, 0.f};
	};

	/**
	 * @brief
	 *  Get the name of a unit, as written in files and commands.
	 * @param[in] iUnit The unit.
	 * @return The name (`Pixels`, `ParentWidth`...).
	 */
	[[nodiscard]] static auto getUnitName(Unit iUnit) -> std::string_view;

	/**
	 * @brief
	 *  Find a unit from its name.
	 * @param[in] iName The name, as `getUnitName` gives it.
	 * @return The unit, or nothing for an unknown name.
	 */
	[[nodiscard]] static auto parseUnit(std::string_view iName) -> std::optional<Unit>;

	/**
	 * @brief
	 *  Get how many reference pixels one unit of a `Unit` makes.
	 * @param[in] iUnit The unit.
	 * @param[in] iFrame The parent and the canvas.
	 * @return 1 for pixels, the width or height of the parent or the canvas otherwise.
	 */
	[[nodiscard]] static auto getUnitExtent(Unit iUnit, const Frame& iFrame) -> float;

	/**
	 * @brief
	 *  Get the size in reference pixels.
	 * @param[in] iFrame The parent and the canvas.
	 * @return The size, the ratios resolved.
	 */
	[[nodiscard]] auto getPixelSize(const Frame& iFrame) const -> math::vec2;

	/**
	 * @brief
	 *  Get the offset in reference pixels.
	 * @param[in] iFrame The parent and the canvas.
	 * @return The offset, the ratios resolved.
	 */
	[[nodiscard]] auto getPixelOffset(const Frame& iFrame) const -> math::vec2;

	/**
	 * @brief
	 *  Set the size from a value in reference pixels, in the current units.
	 * @param[in] iPixels The size in reference pixels.
	 * @param[in] iFrame The parent and the canvas.
	 */
	void setPixelSize(const math::vec2& iPixels, const Frame& iFrame);

	/**
	 * @brief
	 *  Set the offset from a value in reference pixels, in the current units.
	 * @param[in] iPixels The offset in reference pixels.
	 * @param[in] iFrame The parent and the canvas.
	 */
	void setPixelOffset(const math::vec2& iPixels, const Frame& iFrame);

	/**
	 * @brief
	 *  Change the unit of one number of the size, converting its value so the widget keeps its size.
	 * @param[in] iAxis The number.
	 * @param[in] iUnit The new unit.
	 * @param[in] iFrame The parent and the canvas.
	 */
	void convertSizeUnit(Axis iAxis, Unit iUnit, const Frame& iFrame);

	/**
	 * @brief
	 *  Change the unit of one number of the offset, converting its value so the widget keeps its place.
	 * @param[in] iAxis The number.
	 * @param[in] iUnit The new unit.
	 * @param[in] iFrame The parent and the canvas.
	 */
	void convertOffsetUnit(Axis iAxis, Unit iUnit, const Frame& iFrame);

	/**
	 * @brief
	 *  Move the widget, the offset staying in its units.
	 * @param[in] iDelta The distance in reference pixels (positive Y goes down).
	 * @param[in] iFrame The parent and the canvas.
	 */
	void moveBy(const math::vec2& iDelta, const Frame& iFrame);

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
	 * @param[in] iScreen The canvas size, for the `Screen*` units.
	 * @return The widget rectangle, in the parent's coordinates.
	 */
	[[nodiscard]] auto computeBounds(const Bounds& iParent, const math::vec2& iScreen) const -> Bounds;

	/**
	 * @brief
	 *  Compute the rectangle of the widget in its parent, the canvas being the parent.
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
