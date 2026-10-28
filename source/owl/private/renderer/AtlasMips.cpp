/**
 * @file AtlasMips.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/AtlasMips.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

auto halve(const std::vector<uint8_t>& iPixels, const math::vec2ui& iSize) -> std::vector<uint8_t> {
	const math::vec2ui half{iSize.x() / 2, iSize.y() / 2};
	std::vector<uint8_t> result(static_cast<size_t>(half.surface()) * 4);
	for (uint32_t y = 0; y < half.y(); ++y) {
		for (uint32_t x = 0; x < half.x(); ++x) {
			uint32_t alpha = 0;
			std::array<uint32_t, 3> weighted{};
			std::array<uint32_t, 3> plain{};
			for (uint32_t dy = 0; dy < 2; ++dy) {
				for (uint32_t dx = 0; dx < 2; ++dx) {
					const auto* texel =
							&iPixels[((static_cast<size_t>(y) * 2 + dy) * iSize.x() + static_cast<size_t>(x) * 2 + dx) *
									 4];
					for (size_t channel = 0; channel < 3; ++channel) {
						weighted[channel] += static_cast<uint32_t>(texel[channel]) * texel[3];
						plain[channel] += texel[channel];
					}
					alpha += texel[3];
				}
			}
			auto* out = &result[(static_cast<size_t>(y) * half.x() + x) * 4];
			for (size_t channel = 0; channel < 3; ++channel)
				out[channel] = static_cast<uint8_t>(alpha > 0 ? (weighted[channel] + alpha / 2) / alpha
															  : (plain[channel] + 2) / 4);
			out[3] = static_cast<uint8_t>((alpha + 2) / 4);
		}
	}
	return result;
}

}// namespace

auto cellMipLevelCount(const math::vec2ui& iCell) -> uint32_t {
	if (iCell.x() == 0 || iCell.y() == 0)
		return 1;
	return static_cast<uint32_t>(std::min(std::countr_zero(iCell.x()), std::countr_zero(iCell.y()))) + 1;
}

auto buildCellMips(const std::span<const uint8_t> iRgba, const math::vec2ui& iSize, const math::vec2ui& iCell)
		-> gpu::TextureLevels {
	OWL_PROFILE_FUNCTION()

	gpu::TextureLevels result{.size = iSize, .format = gpu::ImageFormat::Rgba8, .levels = {}};
	if (iRgba.size() < static_cast<size_t>(iSize.surface()) * 4) {
		OWL_CORE_WARN("AtlasMips: {} bytes given for a {}x{} RGBA atlas.", iRgba.size(), iSize.x(), iSize.y())
		return result;
	}
	result.levels.emplace_back(iRgba.begin(), iRgba.begin() + static_cast<std::ptrdiff_t>(iSize.surface()) * 4);
	const uint32_t count = cellMipLevelCount(iCell);
	math::vec2ui size = iSize;
	for (uint32_t level = 1; level < count && size.x() >= 2 && size.y() >= 2; ++level) {
		result.levels.push_back(halve(result.levels.back(), size));
		size = {size.x() / 2, size.y() / 2};
	}
	return result;
}

}// namespace owl::renderer
