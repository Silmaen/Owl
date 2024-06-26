/**
 * @file matrices.h
 * @author Silmaen
 * @date 24/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "vectors.h"

namespace owl::math {

/**
 * @brief Basic class for math matrix.
 * @tparam BaseType The internal Data type.
 * @tparam NCol The number of columns in the matrix.
 * @tparam NRow The number of rows in the matrix.
 */
template<typename BaseType, std::size_t NCol, std::size_t NRow>
class Matrix {
public:
	/**
	 * @brief Default constructor.
	 */
	constexpr Matrix() { std::fill(m_data.begin(), m_data.end(), BaseType{}); }
	/**
	 * @brief Copy constructor.
	 */
	constexpr Matrix(const Matrix&) = default;
	/**
	 * @brief Move constructor.
	 */
	constexpr Matrix(Matrix&&) = default;
	/**
	 * @brief Default Destructor.
	 */
	constexpr ~Matrix() = default;
	/**
	 * @brief Copy affectation operator.
	 * @return This vector.
	 */
	constexpr Matrix& operator=(const Matrix&) = default;
	/**
	 * @brief Move affectation operator.
	 * @return This vector.
	 */
	constexpr Matrix& operator=(Matrix&&) = default;
	/**
	 * @brief Constructor with initializer list.
	 * @param iInitList List for initialization.
	 */
	constexpr Matrix(std::initializer_list<BaseType> iInitList) {
		std::copy_n(iInitList.begin(), NCol * NRow, m_data.begin());
	}
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return true if identical vectors.
	 */
	constexpr bool operator==(const Matrix& iOther) const { return m_data == iOther.m_data; }
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return false if identical vectors.
	 */
	constexpr bool operator!=(const Matrix& iOther) const { return m_data != iOther.m_data; }
	/**
	 * @brief Access to component.
	 * @param iRow Row number.
	 * @param iCol Column number.
	 * @return The component value.
	 */
	constexpr BaseType& operator()(const std::size_t iRow, const std::size_t iCol) { return get(iRow, iCol); }
	/**
	 * @brief Access to component.
	 * @param iRow Row number.
	 * @param iCol Column number.
	 * @return The component value.
	 */
	constexpr const BaseType& operator()(const std::size_t iRow, const std::size_t iCol) const {
		return get(iRow, iCol);
	}

	/**
	 * @brief Access to const column values.
	 * @param i The column's number.
	 * @return Const column's value.
	 */
	[[nodiscard]] constexpr Vector<BaseType, NRow> column(const std::size_t i) const {
		Vector<BaseType, NRow> result;
		for (size_t it = 0; it < NRow; ++it) result[it] = get(it, i);
		return result;
	}
	/**
	 * @brief Access to const row values.
	 * @param i The row's number.
	 * @return Const row's value.
	 */
	[[nodiscard]] constexpr Vector<BaseType, NCol> row(const std::size_t i) const {
		Vector<BaseType, NCol> result;
		for (size_t it = 0; it < NCol; ++it) result[it] = get(i, it);
		return result;
	}
	/**
	 * @brief Define column values.
	 * @param i The column's number.
	 * @param iVector The column values.
	 */
	template<size_t Dim>
	constexpr void setColumn(const std::size_t i, const Vector<BaseType, Dim>& iVector) {
		for (size_t it = 0; it < std::min(NRow, Dim); ++it) get(it, i) = iVector[it];
	}
	/**
	 * @brief Define row values.
	 * @param i The row's number.
	 * @param iVector The row values.
	 */
	template<size_t Dim>
	constexpr void setRow(const std::size_t i, const Vector<BaseType, NCol>& iVector) {
		for (size_t it = 0; it < std::min(NCol, Dim); ++it) get(i, it) = iVector[it];
	}

	/**
	 * @brief Self Addition operator.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr Matrix& operator+=(const Matrix& iOther) {
		for (size_t it = 0; it < NCol * NRow; ++it) { m_data[it] += iOther.m_data[it]; }
		return *this;
	}
	/**
	 * @brief Self subtration operator.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr Matrix& operator-=(const Matrix& iOther) {
		for (size_t it = 0; it < NCol * NRow; ++it) { m_data[it] -= iOther.m_data[it]; }
		return *this;
	}
	/**
	 * @brief Addition.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr Matrix operator+(const Matrix& iOther) const {
		Matrix res{*this};
		res += iOther;
		return res;
	}
	/**
	 * @brief Subtaction.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr Matrix operator-(const Matrix& iOther) const {
		Matrix res{*this};
		res -= iOther;
		return res;
	}
	/**
	 * @brief Self multiplication with a scalar.
	 * @param iScalar the scalar to multiply.
	 * @return This actualized vector.
	 */
	constexpr Matrix& operator*=(const BaseType iScalar) {
		for (auto& data: m_data) { data *= iScalar; }
		return *this;
	}
	/**
	 * @brief Multiplication with a scalar.
	 * @param iScalar The scalar to multiply.
	 * @return Vector result.
	 */
	constexpr Matrix operator*(const BaseType iScalar) const {
		Matrix res{*this};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Multiplication with a scalar
	 * @param iScalar The scalar to multiply.
	 * @param iVector Vector to multiply.
	 * @return Vector result.
	 */
	constexpr friend Matrix operator*(const BaseType iScalar, const Matrix& iVector) {
		Matrix res{iVector};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Self division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return This actualized vector.
	 */
	constexpr Matrix& operator/=(const BaseType iScalar) {
		for (auto& data: m_data) { data /= iScalar; }
		return *this;
	}
	/**
	 * @brief Division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return Vector result.
	 */
	constexpr Matrix operator/(const BaseType iScalar) const {
		Matrix res{*this};
		res /= iScalar;
		return res;
	}
	/**
	 * @brief Self matrix product.
	 * @param iOther The other matrix.
	 * @return This updated matrix.
	 */
	constexpr Matrix& operator*=(const Matrix& iOther)
		requires(NRow == NCol)
	{
		for (std::size_t row = 0; row < NRow; ++row) {
			std::array<BaseType, NCol> rowVal{};
			for (std::size_t col = 0; col < NCol; ++col) {
				for (std::size_t it = 0; it < NCol; ++it) { rowVal[col] += get(row, it) * iOther(it, col); }
			}
			for (std::size_t col = 0; col < NCol; ++col) { get(row, col) = rowVal[col]; }
		}
		return *this;
	}
	/**
	 * @brief Matrix-Matrix product.
	 * @tparam NCol2 Number of column in the second matrix.
	 * @param iOther The second matrix
	 * @return The product.
	 */
	template<std::size_t NCol2>
	constexpr Matrix<BaseType, NCol2, NRow> operator*(const Matrix<BaseType, NCol2, NCol>& iOther) const {
		Matrix<BaseType, NCol2, NRow> mat;
		for (std::size_t row = 0; row < NRow; ++row) {
			for (std::size_t col = 0; col < NCol2; ++col) {
				for (std::size_t it = 0; it < NCol; ++it) { mat(row, col) += get(row, it) * iOther(it, col); }
			}
		}
		return mat;
	}
	/**
	 * @brief Get a transposed version of this matrix.
	 * @return Transposed matrix.
	 */
	constexpr Matrix<BaseType, NRow, NCol> transposed() const {
		Matrix<BaseType, NRow, NCol> mat;
		for (std::size_t row = 0; row < NRow; ++row) {
			for (std::size_t col = 0; col < NCol; ++col) { mat(col, row) = get(row, col); }
		}
		return mat;
	}
	/**
	 * @brief Transpose this matrix.
	 * @return This updated matrix.
	 */
	constexpr Matrix& transpose() {
		for (std::size_t row = 0; row < NRow; ++row) {
			for (std::size_t col = row; col < NCol; ++col) { std::swap(get(col, row), get(row, col)); }
		}
		return *this;
	}
	/**
	 * @brief Compute the norm of the matrix.
	 * @return The norm of the matrix.
	 */
	[[nodiscard]] constexpr BaseType norm() const {
		BaseType normVal{};
		for (std::size_t row = 0; row < NRow; ++row) {
			for (std::size_t col = 0; col < NCol; ++col) { normVal += get(col, row) * get(row, col); }
		}
		if constexpr (std::is_floating_point_v<BaseType>)
			normVal = std::sqrt(normVal);
		return normVal;
	}

	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	[[nodiscard]] constexpr const BaseType* data() const { return m_data.data(); }
	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	[[nodiscard]] constexpr BaseType* data() { return m_data.data(); }

private:
	/**
	 * @brief Access to component.
	 * @param iRow Row number.
	 * @param iCol Column number.
	 * @return The component value.
	 */
	constexpr BaseType& get(const std::size_t iRow, const std::size_t iCol) { return m_data[iRow + iCol * NRow]; }
	/**
	 * @brief Access to component.
	 * @param iRow Row number.
	 * @param iCol Column number.
	 * @return The component value.
	 */
	constexpr const BaseType& get(const std::size_t iRow, const std::size_t iCol) const {
		return m_data[iRow + iCol * NRow];
	}
	/// The matrix components.
	std::array<BaseType, NCol * NRow> m_data;
};

using mat2 = Matrix<float, 2, 2>;
using mat3 = Matrix<float, 3, 3>;
using mat4 = Matrix<float, 4, 4>;

}// namespace owl::math
