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
 * @tparam Dim The Vector's dimension.
 */
template<typename BaseType, std::size_t Dim>
class Vector {
public:
	/**
	 * @brief Default constructor.
	 */
	constexpr Vector() noexcept { std::fill(m_data.begin(), m_data.end(), BaseType{}); }
	/**
	 * @brief Copy constructor.
	 */
	constexpr Vector(const Vector&) noexcept = default;
	/**
	 * @brief Move constructor.
	 */
	constexpr Vector(Vector&&) noexcept = default;
	/**
	 * @brief Default Destructor.
	 */
	constexpr ~Vector() noexcept = default;
	/**
	 * @brief Copy affectation operator.
	 * @return This vector.
	 */
	constexpr auto operator=(const Vector&) noexcept -> Vector& = default;
	/**
	 * @brief Move affectation operator.
	 * @return This vector.
	 */
	constexpr auto operator=(Vector&&) noexcept -> Vector& = default;
	/**
	 * @brief Constructor with initializer list.
	 * @param iInitList List for initialization.
	 */
	constexpr Vector(std::initializer_list<BaseType> iInitList) noexcept
		requires(Dim == 1 || Dim > 4)
	{
		std::copy_n(iInitList.begin(), Dim, m_data.begin());
	}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY) noexcept
		requires(Dim == 2)
		: m_data{iX, iY} {}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 * @param iZ Z value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY, const BaseType& iZ) noexcept
		requires(Dim == 3)
		: m_data{iX, iY, iZ} {}
	/**
	 * @brief Constructor by component.
	 * @param iX X value.
	 * @param iY Y value.
	 * @param iZ Z value.
	 * @param iW Z value.
	 */
	constexpr Vector(const BaseType& iX, const BaseType& iY, const BaseType& iZ, const BaseType& iW) noexcept
		requires(Dim == 4)
		: m_data{iX, iY, iZ, iW} {}
	/**
	 * @brief Constructor with initializer list.
	 * @param iVector Vector for initialization.
	 */
	// NOLINTBEGIN(google-explicit-constructor,hicpp-explicit-conversions)
	template<size_t Dim2>
	constexpr Vector(const Vector<BaseType, Dim2>& iVector) noexcept {
		if constexpr (Dim2 < Dim)
			std::fill(m_data, BaseType{0});
		std::copy_n(iVector.begin(), std::min(Dim, Dim2), m_data.begin());
	}
	//NOLINTEND(google-explicit-constructor,hicpp-explicit-conversions)
	/**
	 * @brief Copy affectation operator.
	 * @param iVector Vector for initialization.
	 * @return This vector.
	 */
	template<size_t Dim2>
	constexpr auto operator=(const Vector<BaseType, Dim2>& iVector) noexcept -> Vector& {
		std::copy_n(iVector.begin(), std::min(Dim, Dim2), m_data.begin());
		return *this;
	}
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return true if identical vectors.
	 */
	constexpr auto operator==(const Vector& iOther) const noexcept -> bool { return m_data == iOther.m_data; }
	/**
	 * @brief Comparison operator
	 * @param iOther Other vector to compare.
	 * @return false if identical vectors.
	 */
	constexpr auto operator!=(const Vector& iOther) const noexcept -> bool { return m_data != iOther.m_data; }
	/**
	 * @brief Access to components values.
	 * @param i The component's number.
	 * @return Component's value.
	 */
	constexpr auto operator[](std::size_t i) noexcept -> BaseType& { return m_data[i]; }
	/**
	 * @brief Access to const components values.
	 * @param i The component's number.
	 * @return Const component's value.
	 */
	constexpr auto operator[](std::size_t i) const noexcept -> const BaseType& { return m_data[i]; }
	constexpr auto x() noexcept -> BaseType& requires(Dim >= 1) { return m_data[0]; }

	constexpr auto y() noexcept -> BaseType& requires(Dim >= 2) { return m_data[1]; }

	constexpr auto z() noexcept -> BaseType& requires(Dim >= 3) { return m_data[2]; }

	constexpr auto w() noexcept -> BaseType& requires(Dim >= 4) { return m_data[3]; }

	constexpr auto r() noexcept -> BaseType& requires(Dim == 4) { return x(); }

	constexpr auto g() noexcept -> BaseType& requires(Dim == 4) { return y(); }

	constexpr auto b() noexcept -> BaseType& requires(Dim == 4) { return z(); }

	constexpr auto a() noexcept -> BaseType& requires(Dim == 4) { return w(); }

	[[nodiscard]] constexpr auto x() const noexcept -> const BaseType& requires(Dim >= 1) { return m_data[0]; }

	[[nodiscard]] constexpr auto y() const noexcept -> const BaseType& requires(Dim >= 2) { return m_data[1]; }

	[[nodiscard]] constexpr auto z() const noexcept -> const BaseType& requires(Dim >= 3) { return m_data[2]; }

	[[nodiscard]] constexpr auto w() const noexcept -> const BaseType& requires(Dim >= 4) { return m_data[3]; }

	[[nodiscard]] constexpr auto r() const noexcept -> const BaseType& requires(Dim == 4) { return x(); }

	[[nodiscard]] constexpr auto g() const noexcept -> const BaseType& requires(Dim == 4) { return y(); }

	[[nodiscard]] constexpr auto b() const noexcept -> const BaseType& requires(Dim == 4) { return z(); }

	[[nodiscard]] constexpr auto a() const noexcept -> const BaseType& requires(Dim == 4) { return w(); }
	// iterator
	constexpr auto begin() noexcept -> typename std::array<BaseType, Dim>::iterator {
		return m_data.begin();
	}
	constexpr auto end() noexcept -> typename std::array<BaseType, Dim>::iterator { return m_data.end(); }
	[[nodiscard]] constexpr auto begin() const noexcept -> typename std::array<BaseType, Dim>::const_iterator {
		return m_data.begin();
	}
	[[nodiscard]] constexpr auto end() const noexcept -> typename std::array<BaseType, Dim>::const_iterator {
		return m_data.end();
	}
	// base operator
	/**
	 * @brief Self Addition operator.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr auto operator+=(const Vector& iOther) noexcept -> Vector& {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] += iOther[it]; }
		return *this;
	}
	/**
	 * @brief Self subtraction operator.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr auto operator-=(const Vector& iOther) noexcept -> Vector& {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] -= iOther[it]; }
		return *this;
	}
	/**
	 * @brief Addition.
	 * @param iOther Other vector to add.
	 * @return This actualized vector.
	 */
	constexpr auto operator+(const Vector& iOther) const noexcept -> Vector {
		Vector res{*this};
		res += iOther;
		return res;
	}
	/**
	 * @brief Subtraction.
	 * @param iOther Other vector to subtract.
	 * @return This actualized vector.
	 */
	constexpr auto operator-(const Vector& iOther) const noexcept -> Vector {
		Vector res{*this};
		res -= iOther;
		return res;
	}
	/**
	 * @brief Unary minus operator.
	 * @return Opposite vector.
	 */
	constexpr auto operator-() const noexcept -> Vector {
		Vector res{*this};
		res *= BaseType{-1};
		return res;
	}
	/**
	 * @brief Self multiplication with a scalar.
	 * @param iScalar the scalar to multiply.
	 * @return This actualized vector.
	 */
	constexpr auto operator*=(const BaseType iScalar) noexcept -> Vector& {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] *= iScalar; }
		return *this;
	}
	/**
	 * @brief Multiplication with a scalar.
	 * @param iScalar The scalar to multiply.
	 * @return Vector result.
	 */
	constexpr auto operator*(const BaseType iScalar) const noexcept -> Vector {
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
	constexpr friend auto operator*(const BaseType iScalar, const Vector& iVector) noexcept -> Vector {
		Vector res{iVector};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Self division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return This actualized vector.
	 */
	constexpr auto operator/=(const BaseType iScalar) noexcept -> Vector& {
		for (size_t it = 0; it < Dim; ++it) { m_data[it] /= iScalar; }
		return *this;
	}
	/**
	 * @brief Division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return Vector result.
	 */
	constexpr auto operator/(const BaseType iScalar) const noexcept -> Vector {
		Vector res{*this};
		res /= iScalar;
		return res;
	}

	/**
	 * @brief Dot Product.
	 * @param iOther Other vector to multiply.
	 * @return The dot Product.
	 */
	constexpr auto operator*(const Vector& iOther) const noexcept -> BaseType {
		BaseType res{};
		for (size_t it = 0; it < Dim; ++it) { res += m_data[it] * iOther[it]; }
		return res;
	}
	/**
	 * @brief Compute the square norm.
	 * @return The square norm.
	 */
	[[nodiscard]] constexpr auto normSq() const noexcept -> BaseType { return *this * *this; }
	/**
	 * @brief Compute the norm.
	 * @return The norm.
	 */
	[[nodiscard]] constexpr auto norm() const -> BaseType {
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
	constexpr auto normalize() noexcept -> Vector& {
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
	[[nodiscard]] constexpr auto normalized() const noexcept -> Vector { return Vector{*this}.normalize(); }
	/**
	 * @brief Self cross product.
	 * @param iOther Other vector to multiply.
	 * @return This actualized vector.
	 */
	constexpr auto operator^=(const Vector& iOther) noexcept -> Vector& requires(Dim == 3) {
		BaseType x = (m_data[1] * iOther[2]) - (m_data[2] * iOther[1]);
		BaseType y = (m_data[2] * iOther[0]) - (m_data[0] * iOther[2]);
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
	constexpr auto operator^(const Vector& iOther) noexcept -> Vector
		requires(Dim == 3)
	{
		Vector res{*this};
		res ^= iOther;
		return res;
	}

	/**
	 * @brief Compute the surface of a 2D vector;
	 * @return The surface.
	 */
	[[nodiscard]] constexpr auto surface() const noexcept -> BaseType
		requires(Dim == 2)
	{
		return x() * y();
	}
	/**
	 * @brief Compute the ratio between dimensions.
	 * @return The dimension's ratio.
	 */
	[[nodiscard]] constexpr auto ratio() const noexcept -> float
		requires(Dim == 2)
	{
		return static_cast<float>(x()) / static_cast<float>(y());
	}

	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	[[nodiscard]] constexpr auto data() const noexcept -> const BaseType* { return m_data.data(); }
	/**
	 * @brief Low level memory access.
	 * @return Pointer to the data.
	 */
	[[nodiscard]] constexpr auto data() noexcept -> BaseType* { return m_data.data(); }

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
