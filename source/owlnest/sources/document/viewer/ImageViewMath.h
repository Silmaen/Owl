/**
 * @file ImageViewMath.h
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <math/vectors.h>

#include <cstdint>
#include <optional>
#include <string>

namespace owl::nest::viewer {

/// Smallest zoom of an image view (1 image pixel = 0.05 screen pixel).
inline constexpr float minImageZoom = 0.05f;
/// Largest zoom of an image view.
inline constexpr float maxImageZoom = 32.f;
/// Zoom from which the pixel grid is drawn.
inline constexpr float pixelGridZoom = 8.f;

/**
 * @brief
 *  Zoom and pan of an image shown in a view area.
 */
struct ImageView {
	/// Screen pixels per image pixel.
	float zoom = 1.f;
	/// Position of the image top-left corner in the view area, in screen pixels.
	math::vec2 offset{0.f, 0.f};
};

/**
 * @brief
 *  Keep a zoom in `[minImageZoom, maxImageZoom]`.
 * @param[in] iZoom The zoom.
 * @return The clamped zoom.
 */
[[nodiscard]] auto clampImageZoom(float iZoom) -> float;

/**
 * @brief
 *  Zoom that shows a whole image in a view area, with a margin; never above `maxImageZoom`.
 * @param[in] iImageSize Size of the image in pixels.
 * @param[in] iViewSize Size of the view area in screen pixels.
 * @param[in] iMargin Free space kept around the image, on each side.
 * @return The zoom (1 for an empty image).
 */
[[nodiscard]] auto fitImageZoom(const math::vec2ui& iImageSize, const math::vec2& iViewSize, float iMargin) -> float;

/**
 * @brief
 *  View showing the image at a zoom, centred in the view area.
 * @param[in] iImageSize Size of the image in pixels.
 * @param[in] iViewSize Size of the view area in screen pixels.
 * @param[in] iZoom The zoom (clamped).
 * @return The view.
 */
[[nodiscard]] auto centeredImageView(const math::vec2ui& iImageSize, const math::vec2& iViewSize, float iZoom)
		-> ImageView;

/**
 * @brief
 *  Change the zoom of a view keeping the image point under a screen point where it is.
 * @param[in] iView The view.
 * @param[in] iAnchor The screen point (in the view area) that stays still, usually the cursor.
 * @param[in] iZoom The new zoom (clamped).
 * @return The new view.
 */
[[nodiscard]] auto zoomImageView(const ImageView& iView, const math::vec2& iAnchor, float iZoom) -> ImageView;

/**
 * @brief
 *  Zoom after some wheel notches: 25% per notch, clamped.
 * @param[in] iZoom The zoom before.
 * @param[in] iNotches The wheel notches, positive to zoom in.
 * @return The new zoom.
 */
[[nodiscard]] auto stepImageZoom(float iZoom, float iNotches) -> float;

/**
 * @brief
 *  Image pixel position (fractional) under a screen point.
 * @param[in] iView The view.
 * @param[in] iScreen The screen point, in the view area.
 * @return The image position.
 */
[[nodiscard]] auto viewToImage(const ImageView& iView, const math::vec2& iScreen) -> math::vec2;

/**
 * @brief
 *  Screen point of an image position.
 * @param[in] iView The view.
 * @param[in] iImage The image position, in pixels.
 * @return The screen point, in the view area.
 */
[[nodiscard]] auto imageToView(const ImageView& iView, const math::vec2& iImage) -> math::vec2;

/**
 * @brief
 *  Image pixel under a screen point.
 * @param[in] iView The view.
 * @param[in] iScreen The screen point, in the view area.
 * @param[in] iImageSize Size of the image in pixels.
 * @return The pixel, nothing when the point is outside the image.
 */
[[nodiscard]] auto pixelAt(const ImageView& iView, const math::vec2& iScreen, const math::vec2ui& iImageSize)
		-> std::optional<math::vec2ui>;

/**
 * @brief
 *  Check whether the pixel grid is drawn at a zoom.
 * @param[in] iZoom The zoom.
 * @return True from `pixelGridZoom`.
 */
[[nodiscard]] constexpr auto isPixelGridVisible(const float iZoom) -> bool { return iZoom >= pixelGridZoom; }

/**
 * @brief
 *  Index of the tile of a regular grid under an image pixel.
 * @param[in] iPixel The pixel.
 * @param[in] iTileSize Size of a tile in pixels.
 * @param[in] iColumns Number of columns.
 * @param[in] iRows Number of rows.
 * @return The row-major tile index, nothing when the pixel is outside the grid.
 */
[[nodiscard]] auto tileIndexAt(const math::vec2ui& iPixel, const math::vec2ui& iTileSize, uint32_t iColumns,
							   uint32_t iRows) -> std::optional<uint32_t>;

/**
 * @brief
 *  Write a file size for a human: bytes, then KiB, MiB with one decimal.
 * @param[in] iBytes The size in bytes.
 * @return The text (`512 B`, `1.5 KiB`, `3.0 MiB`).
 */
[[nodiscard]] auto formatFileSize(uint64_t iBytes) -> std::string;

}// namespace owl::nest::viewer
