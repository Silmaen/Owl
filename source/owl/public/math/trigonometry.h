/**
 * @file trigonometry.h
 * @author Silmaen
 * @date 02/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "core/Core.h"

namespace owl::math {

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wimplicit-float-conversion")
/**
 * @brief Convert degrees to radians.
 * @tparam BaseType The internal Data type.
 * @param iInput Angle in degree.
 * @return Angle in radians.
 */
template<typename BaseType>
constexpr auto radians(const BaseType& iInput) -> BaseType {
	return iInput * BaseType{0.01745329251994329576923690768489};
}
/**
 * @brief Convert degrees to radians.
 * @tparam BaseType The internal Data type.
 * @param iInput Angle in degree.
 * @return Angle in radians.
 */
template<typename BaseType, size_t Dim>
constexpr auto radians(const Vector<BaseType, Dim>& iInput) -> Vector<BaseType, Dim> {
	return iInput * BaseType{0.01745329251994329576923690768489};
}

/**
 * @brief Convert radians to degrees.
 * @tparam BaseType The internal Data type.
 * @param iInput Angle in radians.
 * @return Angle in degrees.
 */
template<typename BaseType>
constexpr auto degrees(const BaseType& iInput) -> BaseType {
	return iInput / BaseType{0.01745329251994329576923690768489};
}
/**
 * @brief Convert radians to degrees.
 * @tparam BaseType The internal Data type.
 * @param iInput Angle in radians.
 * @return Angle in degrees.
 */
template<typename BaseType, size_t Dim>
constexpr auto degrees(const Vector<BaseType, Dim>& iInput) -> Vector<BaseType, Dim> {
	return iInput / BaseType{0.01745329251994329576923690768489};
}
OWL_DIAG_POP

/**
 * @brief Arc tangent 2, safe atan(y/(x + |x,y|)).
 * @param[in] iY Numerator.
 * @param[in] iX Denominator.
 * @return Arc tangent with the right quadrant.
 */
OWL_API auto atan2(float iY, float iX) -> float;

}// namespace owl::math
