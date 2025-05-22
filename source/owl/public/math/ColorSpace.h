/**
 * @file ColorSpace.h
 * @author Silmaen
 * @date 15/02/2024
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"

namespace owl::math {

/**
 * \brief Convert sRGB color to linear color.
 * \param[in] iColor Input sRGB color.
 * \return Linear color.
 */
constexpr auto sRgbToLinear(const vec4& iColor) -> vec4 {
	constexpr auto toLinear = [](const float iSrgb) {
		return iSrgb <= 0.04045f ? iSrgb / 12.92f : std::pow((iSrgb + 0.055f) / 1.055f, 2.4f);
	};
	return {toLinear(iColor.r()), toLinear(iColor.g()), toLinear(iColor.b()), iColor.a()};
}

/**
 * \brief Convert linear color to sRgB color.
 * \param[in] iColor Input linear color.
 * \return sRGB colr.
 */
constexpr auto linearTosRgb(const vec4& iColor) -> vec4 {
	constexpr auto toSrgb = [](const float iRgb) {
		constexpr float power = 1.0f / 2.4f;
		return iRgb <= 0.0031308f ? 12.92f * iRgb : 1.055f * std::pow(iRgb, power) - 0.055f;
	};
	return {toSrgb(iColor.r()), toSrgb(iColor.g()), toSrgb(iColor.b()), iColor.a()};
}

}// namespace owl::math
