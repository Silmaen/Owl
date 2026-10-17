/**
 * @file ImageViewMath.cpp
 * @author Silmaen
 * @date 17/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "ImageViewMath.h"

#include <math/vectors.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace owl::nest::viewer {

namespace {
// Zoom factor of one wheel notch.
constexpr float g_notchFactor = 1.25f;
}// namespace

auto clampImageZoom(const float iZoom) -> float { return std::clamp(iZoom, minImageZoom, maxImageZoom); }

auto fitImageZoom(const math::vec2ui& iImageSize, const math::vec2& iViewSize, const float iMargin) -> float {
	if (iImageSize.x() == 0 || iImageSize.y() == 0)
		return 1.f;
	const float availableX = std::max(iViewSize.x() - 2.f * iMargin, 1.f);
	const float availableY = std::max(iViewSize.y() - 2.f * iMargin, 1.f);
	return clampImageZoom(
			std::min(availableX / static_cast<float>(iImageSize.x()), availableY / static_cast<float>(iImageSize.y())));
}

auto centeredImageView(const math::vec2ui& iImageSize, const math::vec2& iViewSize, const float iZoom) -> ImageView {
	const float zoom = clampImageZoom(iZoom);
	return {.zoom = zoom,
			.offset = {(iViewSize.x() - static_cast<float>(iImageSize.x()) * zoom) * 0.5f,
					   (iViewSize.y() - static_cast<float>(iImageSize.y()) * zoom) * 0.5f}};
}

auto zoomImageView(const ImageView& iView, const math::vec2& iAnchor, const float iZoom) -> ImageView {
	const float zoom = clampImageZoom(iZoom);
	const auto image = viewToImage(iView, iAnchor);
	return {.zoom = zoom, .offset = {iAnchor.x() - image.x() * zoom, iAnchor.y() - image.y() * zoom}};
}

auto stepImageZoom(const float iZoom, const float iNotches) -> float {
	return clampImageZoom(iZoom * std::pow(g_notchFactor, iNotches));
}

auto viewToImage(const ImageView& iView, const math::vec2& iScreen) -> math::vec2 {
	return {(iScreen.x() - iView.offset.x()) / iView.zoom, (iScreen.y() - iView.offset.y()) / iView.zoom};
}

auto imageToView(const ImageView& iView, const math::vec2& iImage) -> math::vec2 {
	return {iView.offset.x() + iImage.x() * iView.zoom, iView.offset.y() + iImage.y() * iView.zoom};
}

auto pixelAt(const ImageView& iView, const math::vec2& iScreen, const math::vec2ui& iImageSize)
		-> std::optional<math::vec2ui> {
	const auto image = viewToImage(iView, iScreen);
	if (image.x() < 0.f || image.y() < 0.f)
		return std::nullopt;
	const auto x = static_cast<uint32_t>(std::floor(image.x()));
	const auto y = static_cast<uint32_t>(std::floor(image.y()));
	if (x >= iImageSize.x() || y >= iImageSize.y())
		return std::nullopt;
	return math::vec2ui{x, y};
}

auto tileIndexAt(const math::vec2ui& iPixel, const math::vec2ui& iTileSize, const uint32_t iColumns,
				 const uint32_t iRows) -> std::optional<uint32_t> {
	if (iTileSize.x() == 0 || iTileSize.y() == 0)
		return std::nullopt;
	const uint32_t column = iPixel.x() / iTileSize.x();
	const uint32_t row = iPixel.y() / iTileSize.y();
	if (column >= iColumns || row >= iRows)
		return std::nullopt;
	return row * iColumns + column;
}

auto formatFileSize(const uint64_t iBytes) -> std::string {
	constexpr uint64_t kibi = 1024;
	if (iBytes < kibi)
		return std::format("{} B", iBytes);
	if (iBytes < kibi * kibi)
		return std::format("{:.1f} KiB", static_cast<double>(iBytes) / static_cast<double>(kibi));
	return std::format("{:.1f} MiB", static_cast<double>(iBytes) / static_cast<double>(kibi * kibi));
}

}// namespace owl::nest::viewer
