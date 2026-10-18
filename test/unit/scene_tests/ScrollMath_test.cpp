/**
 * @file ScrollMath_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <scene/ui/ScrollMath.h>

#include <cstddef>
#include <optional>

using namespace owl;
using namespace owl::scene::ui;

TEST(ScrollMath, theOffsetStaysInsideTheContent) {
	EXPECT_FLOAT_EQ(maxScroll(500.f, 200.f), 300.f);
	EXPECT_FLOAT_EQ(maxScroll(100.f, 200.f), 0.f);
	EXPECT_FLOAT_EQ(clampScroll(-5.f, 500.f, 200.f), 0.f);
	EXPECT_FLOAT_EQ(clampScroll(120.f, 500.f, 200.f), 120.f);
	EXPECT_FLOAT_EQ(clampScroll(900.f, 500.f, 200.f), 300.f);
	EXPECT_FLOAT_EQ(clampScroll(50.f, 100.f, 200.f), 0.f);
}

TEST(ScrollMath, theThumbIsProportionalToTheViewport) {
	const auto top = thumbOf(100.f, 200.f, 400.f, 0.f, 10.f);
	EXPECT_FLOAT_EQ(top.start, 0.f);
	EXPECT_FLOAT_EQ(top.length, 50.f);
	const auto bottom = thumbOf(100.f, 200.f, 400.f, 200.f, 10.f);
	EXPECT_FLOAT_EQ(bottom.start, 50.f);
	EXPECT_FLOAT_EQ(bottom.length, 50.f);
	const auto middle = thumbOf(100.f, 200.f, 400.f, 100.f, 10.f);
	EXPECT_FLOAT_EQ(middle.start, 25.f);
}

TEST(ScrollMath, theThumbKeepsAMinimumLengthAndFillsTheTrackWhenNothingScrolls) {
	const auto tiny = thumbOf(100.f, 10.f, 100000.f, 0.f, 16.f);
	EXPECT_FLOAT_EQ(tiny.length, 16.f);
	const auto fits = thumbOf(100.f, 200.f, 150.f, 0.f, 16.f);
	EXPECT_FLOAT_EQ(fits.start, 0.f);
	EXPECT_FLOAT_EQ(fits.length, 100.f);
	EXPECT_FLOAT_EQ(thumbOf(0.f, 10.f, 100.f, 0.f, 16.f).length, 0.f);
}

TEST(ScrollMath, theThumbPositionGivesTheOffsetBack) {
	for (const float offset: {0.f, 37.f, 150.f, 300.f}) {
		const auto thumb = thumbOf(120.f, 200.f, 500.f, offset, 16.f);
		EXPECT_NEAR(offsetFromThumb(120.f, 200.f, 500.f, thumb.start, 16.f), offset, 1e-3f);
	}
	EXPECT_FLOAT_EQ(offsetFromThumb(120.f, 200.f, 500.f, -30.f, 16.f), 0.f);
	EXPECT_FLOAT_EQ(offsetFromThumb(120.f, 200.f, 500.f, 5000.f, 16.f), 300.f);
	EXPECT_FLOAT_EQ(offsetFromThumb(120.f, 200.f, 100.f, 10.f, 16.f), 0.f);
}

TEST(ScrollMath, revealingARangeMovesTheLeast) {
	EXPECT_FLOAT_EQ(revealRange(100.f, 200.f, 150.f, 250.f), 100.f);
	EXPECT_FLOAT_EQ(revealRange(100.f, 200.f, 40.f, 60.f), 40.f);
	EXPECT_FLOAT_EQ(revealRange(100.f, 200.f, 320.f, 360.f), 160.f);
	EXPECT_FLOAT_EQ(revealRange(100.f, 200.f, 300.f, 800.f), 300.f);
}

TEST(ScrollMath, geometryTakesRoomForTheBarsThatOverflowNeeds) {
	const scene::component::UiRect::Bounds bounds{.position = {10.f, 20.f}, .size = {200.f, 100.f}};
	const auto fits = computeScrollGeometry(bounds, {100.f, 80.f}, true, true, true, 8.f);
	EXPECT_FALSE(fits.verticalBar);
	EXPECT_FALSE(fits.horizontalBar);
	EXPECT_FLOAT_EQ(fits.viewport.size.x(), 200.f);
	EXPECT_FLOAT_EQ(fits.maxOffset.y(), 0.f);

	const auto tall = computeScrollGeometry(bounds, {100.f, 300.f}, true, false, true, 8.f);
	EXPECT_TRUE(tall.verticalBar);
	EXPECT_FALSE(tall.horizontalBar);
	EXPECT_FLOAT_EQ(tall.viewport.size.x(), 192.f);
	EXPECT_FLOAT_EQ(tall.viewport.size.y(), 100.f);
	EXPECT_FLOAT_EQ(tall.maxOffset.y(), 200.f);
	EXPECT_FLOAT_EQ(tall.verticalTrack.position.x(), 202.f);
	EXPECT_FLOAT_EQ(tall.verticalTrack.size.x(), 8.f);
	EXPECT_FLOAT_EQ(tall.verticalTrack.size.y(), 100.f);
}

TEST(ScrollMath, aBarCanMakeTheOtherAxisOverflow) {
	const scene::component::UiRect::Bounds bounds{.position = {0.f, 0.f}, .size = {200.f, 100.f}};
	// 195 fits 200 wide, but not once the vertical bar has taken 8: the horizontal bar follows.
	const auto both = computeScrollGeometry(bounds, {195.f, 300.f}, true, true, true, 8.f);
	EXPECT_TRUE(both.verticalBar);
	EXPECT_TRUE(both.horizontalBar);
	EXPECT_FLOAT_EQ(both.viewport.size.x(), 192.f);
	EXPECT_FLOAT_EQ(both.viewport.size.y(), 92.f);
}

TEST(ScrollMath, hiddenBarsTakeNoRoomButTheContentStillScrolls) {
	const scene::component::UiRect::Bounds bounds{.position = {0.f, 0.f}, .size = {200.f, 100.f}};
	const auto hidden = computeScrollGeometry(bounds, {100.f, 300.f}, true, false, false, 8.f);
	EXPECT_FALSE(hidden.verticalBar);
	EXPECT_FLOAT_EQ(hidden.viewport.size.x(), 200.f);
	EXPECT_FLOAT_EQ(hidden.maxOffset.y(), 200.f);
	const auto locked = computeScrollGeometry(bounds, {100.f, 300.f}, false, false, true, 8.f);
	EXPECT_FLOAT_EQ(locked.maxOffset.y(), 0.f);
}

TEST(ScrollMath, onlyTheRowsInViewAreListed) {
	// Rows 20 high with 4 between: a pitch of 24.
	const auto top = visibleRows(0.f, 100.f, 20.f, 4.f, 1000);
	EXPECT_EQ(top.first, 0u);
	EXPECT_EQ(top.end, 5u);
	const auto shifted = visibleRows(30.f, 100.f, 20.f, 4.f, 1000);
	EXPECT_EQ(shifted.first, 1u);
	EXPECT_EQ(shifted.end, 6u);
	const auto bottom = visibleRows(23900.f, 100.f, 20.f, 4.f, 1000);
	EXPECT_EQ(bottom.end, 1000u);
	EXPECT_LE(bottom.size(), 6u);
	EXPECT_EQ(visibleRows(0.f, 100.f, 20.f, 4.f, 0).size(), 0u);
	EXPECT_EQ(visibleRows(0.f, 100.f, 20.f, 4.f, 3).end, 3u);
	EXPECT_EQ(visibleRows(0.f, 0.f, 20.f, 4.f, 10).size(), 0u);
}

TEST(ScrollMath, aPositionGivesItsRow) {
	EXPECT_EQ(rowAt(0.f, 20.f, 4.f, 10), std::optional<size_t>{0});
	EXPECT_EQ(rowAt(19.f, 20.f, 4.f, 10), std::optional<size_t>{0});
	EXPECT_EQ(rowAt(24.f, 20.f, 4.f, 10), std::optional<size_t>{1});
	EXPECT_EQ(rowAt(21.f, 20.f, 4.f, 10), std::nullopt);
	EXPECT_EQ(rowAt(-1.f, 20.f, 4.f, 10), std::nullopt);
	EXPECT_EQ(rowAt(240.f, 20.f, 4.f, 10), std::nullopt);
}

TEST(ScrollMath, theHeightOfAListHasNoTrailingSpacing) {
	EXPECT_FLOAT_EQ(listContentHeight(20.f, 4.f, 0), 0.f);
	EXPECT_FLOAT_EQ(listContentHeight(20.f, 4.f, 1), 20.f);
	EXPECT_FLOAT_EQ(listContentHeight(20.f, 4.f, 10), 236.f);
}
