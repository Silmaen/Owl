/**
 * @file ScrollMath.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "scene/component/UiRect.h"

#include <cstddef>
#include <optional>

/**
 * @brief
 *  Pure maths of the HUD scroll containers (`UiScrollView`, `UiList`), in pixels of one axis unless said otherwise.
 */
namespace owl::scene::ui {

/**
 * @brief
 *  Position and length of a scroll bar thumb along its track.
 */
struct OWL_API ScrollThumb {
	/// Distance from the start of the track.
	float start = 0.f;
	/// Length of the thumb.
	float length = 0.f;
};

/**
 * @brief
 *  Rows of a list that are at least partly in view: `[first, end)`.
 */
struct OWL_API RowRange {
	/// First visible row.
	size_t first = 0;
	/// One past the last visible row.
	size_t end = 0;

	/**
	 * @brief
	 *  Get the number of rows in the range.
	 * @return The row count.
	 */
	[[nodiscard]] auto size() const -> size_t { return end - first; }
};

/**
 * @brief
 *  Where a scroll container puts its viewport and its bars: the room left for the content once the bars that its
 *  overflow needs are taken.
 */
struct OWL_API ScrollGeometry {
	/// Area the content is seen through, in screen pixels.
	component::UiRect::Bounds viewport;
	/// Size of the whole content, in screen pixels.
	math::vec2 content{0.f, 0.f};
	/// Largest offset on each axis.
	math::vec2 maxOffset{0.f, 0.f};
	/// Whether the vertical bar is shown (at the right edge).
	bool verticalBar = false;
	/// Whether the horizontal bar is shown (at the bottom edge).
	bool horizontalBar = false;
	/// Track of the vertical bar.
	component::UiRect::Bounds verticalTrack;
	/// Track of the horizontal bar.
	component::UiRect::Bounds horizontalTrack;
};

/**
 * @brief
 *  Get the largest scroll offset of an axis.
 * @param[in] iContent Size of the content.
 * @param[in] iViewport Size of the viewport.
 * @return The content that does not fit, 0 when it fits.
 */
[[nodiscard]] OWL_API auto maxScroll(float iContent, float iViewport) -> float;

/**
 * @brief
 *  Keep a scroll offset inside its range.
 * @param[in] iOffset The wanted offset.
 * @param[in] iContent Size of the content.
 * @param[in] iViewport Size of the viewport.
 * @return The offset in [0, `maxScroll`].
 */
[[nodiscard]] OWL_API auto clampScroll(float iOffset, float iContent, float iViewport) -> float;

/**
 * @brief
 *  Get the thumb of a scroll bar.
 * @param[in] iTrack Length of the track.
 * @param[in] iViewport Size of the viewport.
 * @param[in] iContent Size of the content.
 * @param[in] iOffset Current scroll offset.
 * @param[in] iMinLength Shortest thumb (kept inside the track).
 * @return The thumb; the whole track when the content fits.
 */
[[nodiscard]] OWL_API auto thumbOf(float iTrack, float iViewport, float iContent, float iOffset, float iMinLength)
		-> ScrollThumb;

/**
 * @brief
 *  Get the scroll offset a thumb position stands for (the inverse of `thumbOf`).
 * @param[in] iTrack Length of the track.
 * @param[in] iViewport Size of the viewport.
 * @param[in] iContent Size of the content.
 * @param[in] iThumbStart Distance from the start of the track to the start of the thumb.
 * @param[in] iMinLength Shortest thumb.
 * @return The offset, in [0, `maxScroll`].
 */
[[nodiscard]] OWL_API auto offsetFromThumb(float iTrack, float iViewport, float iContent, float iThumbStart,
										   float iMinLength) -> float;

/**
 * @brief
 *  Get the scroll offset that brings a range of the content into view with the least movement.
 * @param[in] iOffset Current scroll offset.
 * @param[in] iViewport Size of the viewport.
 * @param[in] iStart Start of the range, from the start of the content.
 * @param[in] iEnd End of the range, from the start of the content.
 * @return The offset; a range larger than the viewport shows its start.
 */
[[nodiscard]] OWL_API auto revealRange(float iOffset, float iViewport, float iStart, float iEnd) -> float;

/**
 * @brief
 *  Lay a viewport and its bars out inside a rectangle.
 * @param[in] iBounds The rectangle of the container, in screen pixels.
 * @param[in] iContent Size of the content, in screen pixels.
 * @param[in] iVertical Whether the container scrolls vertically.
 * @param[in] iHorizontal Whether the container scrolls horizontally.
 * @param[in] iShowBars Whether the bars are shown (and take room) when the content overflows.
 * @param[in] iBarWidth Thickness of a bar, in screen pixels.
 * @return The geometry.
 */
[[nodiscard]] OWL_API auto computeScrollGeometry(const component::UiRect::Bounds& iBounds, const math::vec2& iContent,
												 bool iVertical, bool iHorizontal, bool iShowBars, float iBarWidth)
		-> ScrollGeometry;

/**
 * @brief
 *  Get the rows of a list in view.
 * @param[in] iOffset Scroll offset.
 * @param[in] iViewport Height of the viewport.
 * @param[in] iRowHeight Height of a row.
 * @param[in] iRowSpacing Space between two rows.
 * @param[in] iCount Number of rows.
 * @return The range of rows at least partly in view.
 */
[[nodiscard]] OWL_API auto visibleRows(float iOffset, float iViewport, float iRowHeight, float iRowSpacing,
									   size_t iCount) -> RowRange;

/**
 * @brief
 *  Get the row at a position of the content.
 * @param[in] iPosition Distance from the top of the content.
 * @param[in] iRowHeight Height of a row.
 * @param[in] iRowSpacing Space between two rows.
 * @param[in] iCount Number of rows.
 * @return The row, or nothing in the gap between two rows or past the last one.
 */
[[nodiscard]] OWL_API auto rowAt(float iPosition, float iRowHeight, float iRowSpacing, size_t iCount)
		-> std::optional<size_t>;

/**
 * @brief
 *  Get the height of the content of a list.
 * @param[in] iRowHeight Height of a row.
 * @param[in] iRowSpacing Space between two rows.
 * @param[in] iCount Number of rows.
 * @return The height, 0 for no row.
 */
[[nodiscard]] OWL_API auto listContentHeight(float iRowHeight, float iRowSpacing, size_t iCount) -> float;

}// namespace owl::scene::ui
