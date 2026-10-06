/**
 * @file ImageCompare.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <math/vectors.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace owl::test::render {

/**
 * @brief
 *  Tolerance of an image comparison.
 */
struct ImageTolerance {
	/// Largest difference on any RGB channel (0-255) for a pixel to still count as equal.
	uint8_t channel = 24;
	/// Largest fraction of differing pixels (0-1) for the images to still match.
	double ratio = 0.0025;
};

/**
 * @brief
 *  Result of an image comparison.
 */
struct ImageComparison {
	/// True when both images have the same size and the differing fraction is within the tolerance.
	bool match = false;
	/// Number of pixels above the channel tolerance.
	size_t differingPixels = 0;
	/// Fraction of pixels above the channel tolerance.
	double ratio = 1.0;
	/// Largest channel difference over the image.
	uint8_t maxDifference = 0;
	/// Visual diff, RGBA: dimmed reference, differing pixels in red.
	std::vector<uint8_t> diff;
};

/**
 * @brief
 *  Swap the row order of an RGBA image (the engine decoder returns the bottom row first, PNG files store the top
 *  row first).
 * @param[in] iSize Image size in pixels.
 * @param[in] iRgba `width * height * 4` bytes.
 * @return The same image with the rows in the opposite order.
 */
inline auto flipRows(const math::vec2ui iSize, const std::span<const uint8_t> iRgba) -> std::vector<uint8_t> {
	const size_t rowSize = static_cast<size_t>(iSize.x()) * 4;
	std::vector<uint8_t> flipped(iRgba.size());
	for (size_t row = 0; row < iSize.y() && (row + 1) * rowSize <= iRgba.size(); ++row) {
		const auto source = iRgba.subspan(row * rowSize, rowSize);
		std::ranges::copy(source, flipped.begin() + static_cast<std::ptrdiff_t>((iSize.y() - 1 - row) * rowSize));
	}
	return flipped;
}

/**
 * @brief
 *  Compare two RGBA images of the same size (alpha is ignored).
 * @param[in] iSize Image size in pixels.
 * @param[in] iActual Rendered image, `width * height * 4` bytes.
 * @param[in] iReference Reference image, same layout.
 * @param[in] iTolerance Comparison tolerance.
 * @return The comparison result (no match when the buffers do not fit the size).
 */
inline auto compareImages(const math::vec2ui iSize, const std::span<const uint8_t> iActual,
						  const std::span<const uint8_t> iReference, const ImageTolerance& iTolerance)
		-> ImageComparison {
	ImageComparison result;
	const size_t pixelCount = static_cast<size_t>(iSize.x()) * iSize.y();
	if (pixelCount == 0 || iActual.size() != pixelCount * 4 || iReference.size() != pixelCount * 4)
		return result;
	result.diff.resize(pixelCount * 4);
	for (size_t i = 0; i < pixelCount; ++i) {
		uint8_t pixelMax = 0;
		for (size_t c = 0; c < 3; ++c) {
			const auto delta = static_cast<uint8_t>(
					std::abs(static_cast<int>(iActual[i * 4 + c]) - static_cast<int>(iReference[i * 4 + c])));
			pixelMax = std::max(pixelMax, delta);
		}
		result.maxDifference = std::max(result.maxDifference, pixelMax);
		const bool differs = pixelMax > iTolerance.channel;
		if (differs)
			++result.differingPixels;
		for (size_t c = 0; c < 3; ++c) result.diff[i * 4 + c] = static_cast<uint8_t>(iReference[i * 4 + c] / 4);
		if (differs) {
			result.diff[i * 4] = 255;
			result.diff[i * 4 + 1] = 0;
			result.diff[i * 4 + 2] = 0;
		}
		result.diff[i * 4 + 3] = 255;
	}
	result.ratio = static_cast<double>(result.differingPixels) / static_cast<double>(pixelCount);
	result.match = result.ratio <= iTolerance.ratio;
	return result;
}

}// namespace owl::test::render
