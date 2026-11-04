/**
 * @file elementaryFunctions.h
 * @author Silmaen
 * @date 04/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

/**
 * @brief
 *  Elementary functions that give the same bits on every target.
 *
 * The system libm differs between glibc, MinGW, Android and the Web by one ulp or more; the simulation calls these
 * instead. They only use the IEEE operations that every target rounds the same way (`+ - * /`, `sqrt`, `floor`,
 * `ldexp`, `frexp`), compiled without FMA contraction. Errors stay within a few ulp of the exact value; trigonometry
 * keeps that accuracy for |x| < 1e6. The `float` overloads compute in `double` and round once.
 */
namespace owl::math {

/**
 * @brief
 *  Sine.
 * @param[in] iX Angle in radians.
 * @return The sine.
 */
[[nodiscard]] OWL_API auto sin(double iX) -> double;
/**
 * @brief
 *  Sine.
 * @param[in] iX Angle in radians.
 * @return The sine.
 */
[[nodiscard]] OWL_API auto sin(float iX) -> float;

/**
 * @brief
 *  Cosine.
 * @param[in] iX Angle in radians.
 * @return The cosine.
 */
[[nodiscard]] OWL_API auto cos(double iX) -> double;
/**
 * @brief
 *  Cosine.
 * @param[in] iX Angle in radians.
 * @return The cosine.
 */
[[nodiscard]] OWL_API auto cos(float iX) -> float;

/**
 * @brief
 *  Tangent.
 * @param[in] iX Angle in radians.
 * @return The tangent.
 */
[[nodiscard]] OWL_API auto tan(double iX) -> double;
/**
 * @brief
 *  Tangent.
 * @param[in] iX Angle in radians.
 * @return The tangent.
 */
[[nodiscard]] OWL_API auto tan(float iX) -> float;

/**
 * @brief
 *  Arc sine.
 * @param[in] iX Value in [-1, 1].
 * @return The angle in [-pi/2, pi/2], NaN out of the domain.
 */
[[nodiscard]] OWL_API auto asin(double iX) -> double;
/**
 * @brief
 *  Arc sine.
 * @param[in] iX Value in [-1, 1].
 * @return The angle in [-pi/2, pi/2], NaN out of the domain.
 */
[[nodiscard]] OWL_API auto asin(float iX) -> float;

/**
 * @brief
 *  Arc cosine.
 * @param[in] iX Value in [-1, 1].
 * @return The angle in [0, pi], NaN out of the domain.
 */
[[nodiscard]] OWL_API auto acos(double iX) -> double;
/**
 * @brief
 *  Arc cosine.
 * @param[in] iX Value in [-1, 1].
 * @return The angle in [0, pi], NaN out of the domain.
 */
[[nodiscard]] OWL_API auto acos(float iX) -> float;

/**
 * @brief
 *  Arc tangent.
 * @param[in] iX The value.
 * @return The angle in [-pi/2, pi/2].
 */
[[nodiscard]] OWL_API auto atan(double iX) -> double;
/**
 * @brief
 *  Arc tangent.
 * @param[in] iX The value.
 * @return The angle in [-pi/2, pi/2].
 */
[[nodiscard]] OWL_API auto atan(float iX) -> float;

/**
 * @brief
 *  Angle of the vector (iX, iY).
 * @param[in] iY Ordinate.
 * @param[in] iX Abscissa.
 * @return The angle in [-pi, pi].
 */
[[nodiscard]] OWL_API auto atan2(double iY, double iX) -> double;
/**
 * @brief
 *  Angle of the vector (iX, iY).
 * @param[in] iY Ordinate.
 * @param[in] iX Abscissa.
 * @return The angle in [-pi, pi].
 */
[[nodiscard]] OWL_API auto atan2(float iY, float iX) -> float;

/**
 * @brief
 *  Exponential.
 * @param[in] iX The exponent.
 * @return e to the power iX.
 */
[[nodiscard]] OWL_API auto exp(double iX) -> double;
/**
 * @brief
 *  Exponential.
 * @param[in] iX The exponent.
 * @return e to the power iX.
 */
[[nodiscard]] OWL_API auto exp(float iX) -> float;

/**
 * @brief
 *  Natural logarithm.
 * @param[in] iX The value.
 * @return The logarithm, -inf for 0, NaN for a negative value.
 */
[[nodiscard]] OWL_API auto log(double iX) -> double;
/**
 * @brief
 *  Natural logarithm.
 * @param[in] iX The value.
 * @return The logarithm, -inf for 0, NaN for a negative value.
 */
[[nodiscard]] OWL_API auto log(float iX) -> float;

/**
 * @brief
 *  Power.
 * @param[in] iX The base.
 * @param[in] iY The exponent (an integer one up to 64 is exact repeated multiplication).
 * @return iX to the power iY, NaN for a negative base with a non-integer exponent.
 */
[[nodiscard]] OWL_API auto pow(double iX, double iY) -> double;
/**
 * @brief
 *  Power.
 * @param[in] iX The base.
 * @param[in] iY The exponent (an integer one up to 64 is exact repeated multiplication).
 * @return iX to the power iY, NaN for a negative base with a non-integer exponent.
 */
[[nodiscard]] OWL_API auto pow(float iX, float iY) -> float;

}// namespace owl::math
