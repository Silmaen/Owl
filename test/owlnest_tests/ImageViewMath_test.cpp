/**
 * @file ImageViewMath_test.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 *
 * Tests of the zoom, pan and tile maths of the image viewer.
 */

#include "document/viewer/ImageViewMath.h"

#include <gtest/gtest.h>

using namespace owl;
using namespace owl::nest::viewer;

TEST(ImageViewMath, clampsTheZoom) {
	EXPECT_FLOAT_EQ(clampImageZoom(0.001f), minImageZoom);
	EXPECT_FLOAT_EQ(clampImageZoom(100.f), maxImageZoom);
	EXPECT_FLOAT_EQ(clampImageZoom(2.f), 2.f);
	EXPECT_FLOAT_EQ(maxImageZoom, 32.f);
}

TEST(ImageViewMath, fitsTheImageInTheView) {
	EXPECT_FLOAT_EQ(fitImageZoom({200, 100}, {400.f, 400.f}, 0.f), 2.f);
	EXPECT_FLOAT_EQ(fitImageZoom({100, 200}, {400.f, 400.f}, 0.f), 2.f);
	EXPECT_FLOAT_EQ(fitImageZoom({100, 100}, {120.f, 120.f}, 10.f), 1.f);
	EXPECT_FLOAT_EQ(fitImageZoom({2, 2}, {4000.f, 4000.f}, 0.f), maxImageZoom);
	EXPECT_FLOAT_EQ(fitImageZoom({0, 0}, {400.f, 400.f}, 0.f), 1.f);
}

TEST(ImageViewMath, centersTheImage) {
	const auto view = centeredImageView({100, 50}, {400.f, 200.f}, 2.f);
	EXPECT_FLOAT_EQ(view.zoom, 2.f);
	EXPECT_FLOAT_EQ(view.offset.x(), 100.f);
	EXPECT_FLOAT_EQ(view.offset.y(), 50.f);
}

TEST(ImageViewMath, zoomKeepsTheAnchorStill) {
	const ImageView view{.zoom = 1.f, .offset = {20.f, 10.f}};
	const math::vec2 anchor{70.f, 60.f};
	const auto before = viewToImage(view, anchor);
	const auto zoomed = zoomImageView(view, anchor, 4.f);
	EXPECT_FLOAT_EQ(zoomed.zoom, 4.f);
	const auto after = viewToImage(zoomed, anchor);
	EXPECT_NEAR(after.x(), before.x(), 1e-4f);
	EXPECT_NEAR(after.y(), before.y(), 1e-4f);
	EXPECT_FLOAT_EQ(zoomImageView(view, anchor, 500.f).zoom, maxImageZoom);
}

TEST(ImageViewMath, wheelSteps) {
	EXPECT_NEAR(stepImageZoom(1.f, 1.f), 1.25f, 1e-5f);
	EXPECT_NEAR(stepImageZoom(1.25f, -1.f), 1.f, 1e-5f);
	EXPECT_FLOAT_EQ(stepImageZoom(30.f, 10.f), maxImageZoom);
	EXPECT_FLOAT_EQ(stepImageZoom(0.06f, -10.f), minImageZoom);
}

TEST(ImageViewMath, convertsBetweenImageAndView) {
	const ImageView view{.zoom = 8.f, .offset = {-16.f, 24.f}};
	const auto screen = imageToView(view, {3.f, 2.f});
	EXPECT_FLOAT_EQ(screen.x(), 8.f);
	EXPECT_FLOAT_EQ(screen.y(), 40.f);
	const auto image = viewToImage(view, screen);
	EXPECT_FLOAT_EQ(image.x(), 3.f);
	EXPECT_FLOAT_EQ(image.y(), 2.f);
}

TEST(ImageViewMath, findsThePixelUnderTheCursor) {
	const ImageView view{.zoom = 10.f, .offset = {5.f, 5.f}};
	const auto pixel = pixelAt(view, {27.f, 38.f}, {4, 4});
	ASSERT_TRUE(pixel.has_value());
	EXPECT_EQ(pixel->x(), 2u);
	EXPECT_EQ(pixel->y(), 3u);
	EXPECT_FALSE(pixelAt(view, {2.f, 20.f}, {4, 4}).has_value());
	EXPECT_FALSE(pixelAt(view, {20.f, 46.f}, {4, 4}).has_value());
	EXPECT_FALSE(pixelAt(view, {46.f, 20.f}, {4, 4}).has_value());
}

TEST(ImageViewMath, showsThePixelGridWhenZoomedIn) {
	EXPECT_FALSE(isPixelGridVisible(1.f));
	EXPECT_FALSE(isPixelGridVisible(7.9f));
	EXPECT_TRUE(isPixelGridVisible(pixelGridZoom));
	EXPECT_TRUE(isPixelGridVisible(maxImageZoom));
}

TEST(ImageViewMath, numbersTheTilesRowMajor) {
	EXPECT_EQ(tileIndexAt({0, 0}, {16, 16}, 4, 2), 0u);
	EXPECT_EQ(tileIndexAt({17, 3}, {16, 16}, 4, 2), 1u);
	EXPECT_EQ(tileIndexAt({63, 31}, {16, 16}, 4, 2), 7u);
	EXPECT_EQ(tileIndexAt({5, 20}, {16, 16}, 4, 2), 4u);
	EXPECT_FALSE(tileIndexAt({64, 0}, {16, 16}, 4, 2).has_value());
	EXPECT_FALSE(tileIndexAt({0, 32}, {16, 16}, 4, 2).has_value());
	EXPECT_FALSE(tileIndexAt({0, 0}, {0, 16}, 4, 2).has_value());
}

TEST(ImageViewMath, writesFileSizes) {
	EXPECT_EQ(formatFileSize(0), "0 B");
	EXPECT_EQ(formatFileSize(1023), "1023 B");
	EXPECT_EQ(formatFileSize(1536), "1.5 KiB");
	EXPECT_EQ(formatFileSize(3 * 1024 * 1024), "3.0 MiB");
}
