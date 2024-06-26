/**
 * @file vectors.h
 * @author Silmaen
 * @date 24/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

namespace owl::math {

/**
 * @brief Basic class for math vectors.
 * @tparam BaseType The internal data type.
 * @tparam Dim The Vector's dimention.
 */
template<typename BaseType, std::size_t Dim>
class Vector {
public:
	/**
	 * @brief Default constructor.
	 */
	constexpr Vector() { std::fill(m_data.begin(), m_data.end(), BaseType{}); }
	/**
	 * @brief Copy constructor.
	 */
	constexpr Vector(const Vector&) = default;
	/**
	 * @brief Move constructor.
	 */
	constexpr Vector(Vector&&) = default;
	/**
	 * @brief Default Destructor.
	 */
	constexpr ~Vector() = default;
	/**
	 * @brief Copy affectation operator.
	 * @return This vector.
	 */
	constexpr Vector& operator=(const Vector&) = default;
	/**
	 * @brief Move affectation operator.
	 * @return This vector.
	 */
	constexpr Vector& operator=(Vector&&) = default;
	/**
	 * @brief Constructor with initializer list.
	 * @param iInitList List for initialization.
	 */
	constexpr Vector(std::initializer_list<BaseType> iInitList)
		requires(Dim == 1 || Dim > 4)
	{
		std::copy_n(iInitList.begin(), Dim, m_data.begin());
	}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY)
		requires(Dim == 2)
		: m_data{iX, iY} {}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 * @param iZ Z value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY, const BaseType& iZ)
		requires(Dim == 3)
		: m_data{iX, iY, iZ} {}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 * @param iZ Z value.
	 * @param iW Z value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY, const BaseType& iZ, const BaseType& iW)
		requires(Dim == 4)
		: m_data{iX, iY, iZ, iW} {}
	/**
	 * @brief Constructor with initializer list.
	 * @param iVector Vector for initialization.
	 */
	template<size_t Dim2>
	constexpr Vector(const Vector<BaseType, Dim2>& iVector) {
		if constexpr (Dim2 < Dim)
			std::fill(m_data, BaseType{0});
		std::copy_n(iVector.begin(), std::min(Dim, Dim2), m_data.begin());
	}
	/**
	 * @brief Copy affectation operator.
	 * @param iVector Vector for initialization.
	 * @return This vector.
	 */
	template<size_t Dim2>
	constexpr Vector& operator=(const Vector<BaseType, Dim2>& iVector) {
		std::copy_n(iVector.begin(), std::min(Dim, Dim2), m_data.begin());
		return *this;
	}
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return true if identical vectors.
	 */
	constexpr bool operator==(const Vector& iOther) const { return m_data == iOther.m_data; }
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return false if identical vectors.
	 */
	constexpr bool operator!=(const Vector& iOther) const { return m_data != iOther.m_data; }
	/**
	 * @brief Access to components values.
	 * @param i The component's number.
	 * @return Component's value.
	 */
	constexpr BaseType& operator[](std::size_t i) { return m_data[i]; }
	/**
	 * @brief Access to const components values.
	 * @param i The component's number.
	 * @return Const component's value.
	 */
	constexpr const BaseType& operator[](std::size_t i) const { return m_data[i]; }
	constexpr BaseType& x()
		requires(Dim >= 1)
	{
		return m_data[0];
	}
	constexpr BaseType& y()
		requires(Dim >= 2)
	{
		return m_data[1];
	}
	constexpr BaseType& z()
		requires(Dim >= 3)
	{
		return m_data[2];
	}
	constexpr BaseType& w()
		requires(Dim >= 4)
	{
		return m_data[3];
	}
	constexpr BaseType& r()
		requires(Dim == 4)
	{
		return x();
	}
	constexpr BaseType& g()
		requires(Dim == 4)
	{
		return y();
	}
	constexpr BaseType& b()
		requires(Dim == 4)
	{
		return z();
	}
	constexpr BaseType& a()
		requires(Dim == 4)
	{
		return w();
	}
	constexpr const BaseType& x() const
		requires(Dim >= 1)
	{
		return m_data[0];
	}
	constexpr const BaseType& y() const
		requires(Dim >= 2)
	{
		return m_data[1];
	}
	constexpr const BaseType& z() const
		requires(Dim >= 3)
	{
		return m_data[2];
	}
	constexpr const BaseType& w() const
		requires(Dim >= 4)
	{
		return m_data[3];
	}
	constexpr const BaseType& r() const
		requires(Dim == 4)
	{
		return x();
	}
	constexpr const BaseType& g() const
		requires(Dim == 4)
	{
		return y();
	}
	constexpr const BaseType& b() const
		requires(Dim == 4)
	{
		return z();
	}
	constexpr const BaseType& a() const
		requires(Dim == 4)
	{
		return w();
	}
	// iterator
	constexpr typename std::array<BaseType, Dim>::iterator begin() { return m_data.begin(); }
	constexpr typename std::array<BaseType, Dim>::iterator end() { return m_data.end(); }
	constexpr typename std::array<BaseType, Dim>::const_iterator begin() const { return m_data.begin(); }
	constexpr typename std::array<BaseType, Dim>::const_iterator end() const { return m_data.end(); }
	// base operator
	/**
	 * @brief Self Addition operator.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr Vector& operator+=(const Vector& iOther) {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] += iOther[it]; }
		return *this;
	}
	/**
	 * @brief Self subtration operator.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr Vector& operator-=(const Vector& iOther) {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] -= iOther[it]; }
		return *this;
	}
	/**
	 * @brief Addition.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr Vector operator+(const Vector& iOther) const {
		Vector res{*this};
		res += iOther;
		return res;
	}
	/**
	 * @brief Subtaction.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr Vector operator-(const Vector& iOther) const {
		Vector res{*this};
		res -= iOther;
		return res;
	}
	/**
	 * @brief Unary minus operator.
	 * @return Opposite vector.
	 */
	constexpr Vector operator-() const {
		Vector res{*this};
		res *= BaseType{-1};
		return res;
	}
	/**
	 * @brief Self multiplication with a scalar.
	 * @param iScalar the scalar to multiply.
	 * @return This actualized vector.
	 */
	constexpr Vector& operator*=(const BaseType iScalar) {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] *= iScalar; }
		return *this;
	}
	/**
	 * @brief Multiplication with a scalar.
	 * @param iScalar The scalar to multiply.
	 * @return Vector result.
	 */
	constexpr Vector operator*(const BaseType iScalar) const {
		Vector res{*this};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Multiplication with a scalar
	 * @param iScalar The scalar to multiply.
	 * @param iVector Vector to multiply.
	 * @return Vector result.
	 */
	constexpr friend Vector operator*(const BaseType iScalar, const Vector& iVector) {
		Vector res{iVector};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Self division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return This actualized vector.
	 */
	constexpr Vector& operator/=(const BaseType iScalar) {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] /= iScalar; }
		return *this;
	}
	/**
	 * @brief Division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return Vector result.
	 */
	constexpr Vector operator/(const BaseType iScalar) const {
		Vector res{*this};
		res /= iScalar;
		return res;
	}

	/**
	 * @brief Dot Product.
	 * @param iOther Other vector to multiply.
	 * @return The dot Product.
	 */
	constexpr BaseType operator*(const Vector& iOther) const {
		BaseType res{};
		for (size_t it = 0; it < Dim; ++it) { res += m_data[it] * iOther[it]; }
		return res;
	}
	/**
	 * @brief Compute the square norm.
	 * @return The sqare norm.
	 */
	constexpr BaseType normSq() const { return *this * *this; }
	/**
	 * @brief Compute the norm.
	 * @return The norm.
	 */
	constexpr BaseType norm() const {
		if constexpr (std::is_floating_point_v<BaseType>) {
			return std::sqrt(normSq());
		}
		if constexpr (std::is_integral_v<BaseType>) {
			BaseType res{};
			if constexpr (std::is_signed_v<BaseType>)
				for (size_t it = 0; it < Dim; ++it) { res += std::abs(m_data[it]); }
			else
				for (size_t it = 0; it < Dim; ++it) { res += m_data[it]; }
			return res;
		}
		return {};
	}
	/**
	 * @brief Normalize this vector if possible.
	 * @return This vector.
	 */
	constexpr Vector& normalize() {
		if constexpr (std::is_floating_point_v<BaseType>) {
			const BaseType n = normSq();
			if (n < std::numeric_limits<BaseType>::epsilon())
				return *this;
			*this /= std::sqrt(n);
		}
		if constexpr (std::is_integral_v<BaseType>) {
			std::size_t idx = 0;
			BaseType val{};
			if constexpr (std::is_signed_v<BaseType>) {
				for (std::size_t it = 0; it < Dim; ++it) {
					if (std::abs(m_data[it]) > std::abs(val)) {
						idx = it;
						val = m_data[it];
					}
				}
				std::fill(m_data.begin(), m_data.end(), BaseType{});
				m_data[idx] = val > 0 ? 1 : -1;
			} else {
				for (std::size_t it = 0; it < Dim; ++it) {
					if (m_data[it] > val) {
						idx = it;
						val = m_data[it];
					}
				}
				std::fill(m_data.begin(), m_data.end(), BaseType{});
				m_data[idx] = 1;
			}
		}
		return *this;
	}
	/**
	 * @brief Create a new normalized vector.
	 * @return Normalized vector.
	 */
	constexpr Vector normalized() const { return Vector{*this}.normalize(); }
	/**
	 * @brief Self cross product.
	 * @param iOther Other vector to multiply.
	 * @return This actualized vector.
	 */
	constexpr Vector& operator^=(const Vector& iOther)
		requires(Dim == 3)
	{
		BaseType x = m_data[1] * iOther[2] - m_data[2] * iOther[1];
		BaseType y = m_data[2] * iOther[0] - m_data[0] * iOther[2];
		m_data[2] = m_data[0] * iOther[1] - m_data[1] * iOther[0];
		std::swap(m_data[0], x);
		std::swap(m_data[1], y);
		return *this;
	}
	/**
	 * @brief Cross product.
	 * @param iOther Other vector to multiply.
	 * @return Vector result.
	 */
	constexpr Vector operator^(const Vector& iOther)
		requires(Dim == 3)
	{
		Vector res{*this};
		res ^= iOther;
		return res;
	}

	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	constexpr const BaseType* data() const { return m_data.data(); }
	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	[[nodiscard]] constexpr BaseType* data() { return m_data.data(); }

private:
	std::array<BaseType, Dim> m_data;
};


// Specialization.
using vec2f = Vector<float, 2>;
using vec3f = Vector<float, 3>;
using vec4f = Vector<float, 4>;
using vec2d = Vector<double, 2>;
using vec3d = Vector<double, 3>;
using vec4d = Vector<double, 4>;
using vec2i = Vector<int32_t, 2>;
using vec3i = Vector<int32_t, 3>;
using vec4i = Vector<int32_t, 4>;
using vec2ui = Vector<uint32_t, 2>;
using vec3ui = Vector<uint32_t, 3>;
using vec4ui = Vector<uint32_t, 4>;
using vec2ui8 = Vector<uint8_t, 2>;
using vec3ui8 = Vector<uint8_t, 3>;
using vec4ui8 = Vector<uint8_t, 4>;

using vec2 = vec2f;
using vec3 = vec3f;
using vec4 = vec4f;

}// namespace owl::math
