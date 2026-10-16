/**
 * @file ScrollMath.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "scene/ui/ScrollMath.h"

#include <algorithm>
#include <cmath>

namespace owl::scene::ui {

auto maxScroll(const float iContent, const float iViewport) -> float { return std::max(0.f, iContent - iViewport); }

auto clampScroll(const float iOffset, const float iContent, const float iViewport) -> float {
	return std::clamp(iOffset, 0.f, maxScroll(iContent, iViewport));
}

auto thumbOf(const float iTrack, const float iViewport, const float iContent, const float iOffset,
			 const float iMinLength) -> ScrollThumb {
	if (iTrack <= 0.f || iContent <= iViewport || iContent <= 0.f)
		return {.start = 0.f, .length = std::max(0.f, iTrack)};
	const float length = std::clamp(iTrack * iViewport / iContent, std::min(iMinLength, iTrack), iTrack);
	const float room = iTrack - length;
	const float range = maxScroll(iContent, iViewport);
	return {.start = range > 0.f ? room * std::clamp(iOffset / range, 0.f, 1.f) : 0.f, .length = length};
}

auto offsetFromThumb(const float iTrack, const float iViewport, const float iContent, const float iThumbStart,
					 const float iMinLength) -> float {
	const float range = maxScroll(iContent, iViewport);
	const float room = iTrack - thumbOf(iTrack, iViewport, iContent, 0.f, iMinLength).length;
	if (range <= 0.f || room <= 0.f)
		return 0.f;
	return std::clamp(iThumbStart / room, 0.f, 1.f) * range;
}

auto revealRange(const float iOffset, const float iViewport, const float iStart, const float iEnd) -> float {
	if (iStart < iOffset)
		return iStart;
	if (iEnd > iOffset + iViewport)
		return std::min(iStart, iEnd - iViewport);
	return iOffset;
}

auto computeScrollGeometry(const component::UiRect::Bounds& iBounds, const math::vec2& iContent, const bool iVertical,
						   const bool iHorizontal, const bool iShowBars, const float iBarWidth) -> ScrollGeometry {
	ScrollGeometry result;
	result.content = iContent;
	math::vec2 size = iBounds.size;
	const float bar = iShowBars ? std::max(0.f, iBarWidth) : 0.f;
	// Two passes: a bar takes room, which can make the other axis overflow.
	for (int pass = 0; pass < 2; ++pass) {
		result.verticalBar = iShowBars && iVertical && iContent.y() > size.y();
		result.horizontalBar = iShowBars && iHorizontal && iContent.x() > size.x();
		size = {iBounds.size.x() - (result.verticalBar ? bar : 0.f),
				iBounds.size.y() - (result.horizontalBar ? bar : 0.f)};
	}
	result.viewport = {.position = iBounds.position, .size = {std::max(0.f, size.x()), std::max(0.f, size.y())}};
	result.maxOffset = {iHorizontal ? maxScroll(iContent.x(), result.viewport.size.x()) : 0.f,
						iVertical ? maxScroll(iContent.y(), result.viewport.size.y()) : 0.f};
	result.verticalTrack = {.position = {iBounds.position.x() + result.viewport.size.x(), iBounds.position.y()},
							.size = {result.verticalBar ? bar : 0.f, result.viewport.size.y()}};
	result.horizontalTrack = {.position = {iBounds.position.x(), iBounds.position.y() + result.viewport.size.y()},
							  .size = {result.viewport.size.x(), result.horizontalBar ? bar : 0.f}};
	return result;
}

auto visibleRows(const float iOffset, const float iViewport, const float iRowHeight, const float iRowSpacing,
				 const size_t iCount) -> RowRange {
	const float pitch = iRowHeight + iRowSpacing;
	if (iCount == 0 || pitch <= 0.f || iViewport <= 0.f)
		return {};
	const float top = std::max(0.f, iOffset);
	const auto first = std::min(iCount - 1, static_cast<size_t>(std::floor(top / pitch)));
	const auto end = std::min(iCount, static_cast<size_t>(std::ceil((top + iViewport) / pitch)));
	return {.first = first, .end = std::max(first + 1, end)};
}

auto rowAt(const float iPosition, const float iRowHeight, const float iRowSpacing, const size_t iCount)
		-> std::optional<size_t> {
	const float pitch = iRowHeight + iRowSpacing;
	if (iPosition < 0.f || pitch <= 0.f)
		return std::nullopt;
	const auto row = static_cast<size_t>(std::floor(iPosition / pitch));
	if (row >= iCount || iPosition - static_cast<float>(row) * pitch >= iRowHeight)
		return std::nullopt;
	return row;
}

auto listContentHeight(const float iRowHeight, const float iRowSpacing, const size_t iCount) -> float {
	if (iCount == 0)
		return 0.f;
	return static_cast<float>(iCount) * (iRowHeight + iRowSpacing) - iRowSpacing;
}

}// namespace owl::scene::ui
