/**
 * @file linAlgebra.h
 * @author Silmaen
 * @date 24/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "matrixCreation.h"

namespace owl::math {

/**
 * @brief Get the epsilon value for given data type.
 * @tparam BaseType The internal type of data.
 * @return Value of epsilon.
 */
template<typename BaseType>
constexpr auto epsilon() -> BaseType {
	return std::numeric_limits<BaseType>::epsilon();
}

/**
 * @brief Compare two values within precision
 * @tparam BaseType The internal type of data.
 * @param iA First value.
 * @param iB Second value.
 * @param iEpsi Epsilon value.
 * @return True if the 2 values are nearly equal.
 */
template<typename BaseType>
constexpr auto epsilonEqual(const BaseType& iA, const BaseType& iB, const BaseType& iEpsi) -> bool {
	return std::abs(iA - iB) < iEpsi;
}

/**
 * @brief Compare two values within precision
 * @tparam BaseType The internal type of data.
 * @param iA First value.
 * @param iB Second value.
 * @param iEpsi Epsilon value.
 * @return True if the 2 values are not nearly equal.
 */
template<typename BaseType>
constexpr auto epsilonNotEqual(const BaseType& iA, const BaseType& iB, const BaseType& iEpsi) -> bool {
	return std::abs(iA - iB) > iEpsi;
}

/**
 * @brief Matrix-Vector product.
 * @tparam BaseType The internal Data type.
 * @tparam NCol The number of columns in the matrix.
 * @tparam NRow The number of rows in the matrix.
 * @param iMatrix The Matrix to multiply.
 * @param iVector The vector to multiply
 * @return The resulting vector.
 */
template<typename BaseType, std::size_t NRow, std::size_t NCol>
constexpr auto operator*(const Matrix<BaseType, NRow, NCol>& iMatrix, const Vector<BaseType, NCol>& iVector)
		-> Vector<BaseType, NRow> {
	Vector<BaseType, NRow> result{};
	for (std::size_t idx = 0; idx < NRow; ++idx) {
		for (std::size_t it = 0; it < NCol; ++it) { result[idx] += iMatrix(idx, it) * iVector[it]; }
	}
	return result;
}
/**
 * @brief Vector-Matrix product.
 * @tparam BaseType The internal Data type.
 * @tparam NCol The number of columns in the matrix.
 * @tparam NRow The number of rows in the matrix.
 * @param iMatrix The Matrix to multiply.
 * @param iVector The vector to multiply
 * @return The resulting vector.
 */
template<typename BaseType, std::size_t NRow, std::size_t NCol>
constexpr auto operator*(const Vector<BaseType, NRow>& iVector, const Matrix<BaseType, NRow, NCol>& iMatrix)
		-> Vector<BaseType, NCol> {
	Vector<BaseType, NRow> result{};
	for (std::size_t idx = 0; idx < NCol; ++idx) {
		for (std::size_t it = 0; it < NRow; ++it) { result[idx] += iMatrix(it, idx) * iVector[it]; }
	}
	return result;
}


}// namespace owl::math
