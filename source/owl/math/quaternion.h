/**
 * @file quaternion.h
 * @author Silmaen
 * @date 30/06/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "vectors.h"

namespace owl::math {

/**
 * @brief Simple Quaternion implementation.
 * @tparam BaseType The internal data type.
 */
template<typename BaseType>
class Quaternion {
public:
	/**
	 * @brief Default constructor.
	 */
	constexpr Quaternion() { std::fill(m_data.begin(), m_data.end(), BaseType{}); }
	/**
	 * @brief Copy constructor.
	 */
	constexpr Quaternion(const Quaternion&) = default;
	/**
	 * @brief Move constructor.
	 */
	constexpr Quaternion(Quaternion&&) = default;
	/**
	 * @brief Default Destructor.
	 */
	~Quaternion() = default;
	/**
	 * @brief Copy affectation operator.
	 * @return This Quaternion.
	 */
	constexpr Quaternion& operator=(const Quaternion&) = default;
	/**
	 * @brief Move affectation operator.
	 * @return This Quaternion.
	 */
	constexpr Quaternion& operator=(Quaternion&&) = default;
	/**
	 * @brief Constructor with initializer list.
	 * @param iW W component.
	 * @param iX X component.
	 * @param iY Y component.
	 * @param iZ Z component.
	 */
	constexpr Quaternion(BaseType iW, BaseType iX, BaseType iY, BaseType iZ) {
		x() = iX;
		y() = iY;
		z() = iZ;
		w() = iW;
	}
	/**
	 * @brief Constructor with a euler angle vector
	 * @param iVector The euler angles.
	 */
	constexpr explicit Quaternion(const Vector<BaseType, 3>& iVector) {
		constexpr BaseType demi{0.5};
		Vector<BaseType, 3> c{std::cos(iVector.x() * demi), std::cos(iVector.y() * demi), std::cos(iVector.z() * demi)};
		Vector<BaseType, 3> s{std::sin(iVector.x() * demi), std::sin(iVector.y() * demi), std::sin(iVector.z() * demi)};
		w() = c.x() * c.y() * c.z() + s.x() * s.y() * s.z();
		x() = s.x() * c.y() * c.z() - c.x() * s.y() * s.z();
		y() = c.x() * s.y() * c.z() + s.x() * c.y() * s.z();
		z() = c.x() * c.y() * s.z() - s.x() * s.y() * c.z();
	}
	// comparisons
	/**
	 * @brief Comparison operator
	 * @param iOther Other Quaternion to compare.
	 * @return true if identical Quaternions.
	 */
	constexpr bool operator==(const Quaternion& iOther) const { return m_data == iOther.m_data; }
	/**
	 * @brief Comparison operator
	 * @param iOther Other Quaternion to compare.
	 * @return false if identical Quaternions.
	 */
	constexpr bool operator!=(const Quaternion& iOther) const { return m_data != iOther.m_data; }
	// Accessors
	/**
	 * @brief Access to components values.
	 * @param i The component's number.
	 * @return Component's value.
	 */
	BaseType& operator[](std::size_t i) { return m_data[i]; }
	/**
	 * @brief Access to const components values.
	 * @param i The component's number.
	 * @return Const component's value.
	 */
	constexpr const BaseType& operator[](std::size_t i) const { return m_data[i]; }
	constexpr BaseType& x() { return m_data[0]; }
	constexpr BaseType& y() { return m_data[1]; }
	constexpr BaseType& z() { return m_data[2]; }
	constexpr BaseType& w() { return m_data[3]; }
	constexpr const BaseType& x() const { return m_data[0]; }
	constexpr const BaseType& y() const { return m_data[1]; }
	constexpr const BaseType& z() const { return m_data[2]; }
	constexpr const BaseType& w() const { return m_data[3]; }
	// iterator
	typename std::array<BaseType, 4>::iterator begin() { return m_data.begin(); }
	typename std::array<BaseType, 4>::iterator end() { return m_data.end(); }
	typename std::array<BaseType, 4>::const_iterator begin() const { return m_data.begin(); }
	typename std::array<BaseType, 4>::const_iterator end() const { return m_data.end(); }
	// base operator
	/**
	 * @brief Self Addition operator.
	 * @param iOther Other Quaternion to add.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion& operator+=(const Quaternion& iOther) {
		for (size_t it = 0; it < 4; ++it) { m_data[it] += iOther[it]; }
		return *this;
	}
	/**
	 * @brief Self subtration operator.
	 * @param iOther Other Quaternion to subtract.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion& operator-=(const Quaternion& iOther) {
		for (size_t it = 0; it < 4; ++it) { m_data[it] -= iOther[it]; }
		return *this;
	}
	/**
	 * @brief Addition.
	 * @param iOther Other Quaternion to add.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion operator+(const Quaternion& iOther) const {
		Quaternion res{*this};
		res += iOther;
		return res;
	}
	/**
	 * @brief Subtaction.
	 * @param iOther Other Quaternion to subtract.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion operator-(const Quaternion& iOther) const {
		Quaternion res{*this};
		res -= iOther;
		return res;
	}
	/**
	 * @brief Unary minus.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion operator-() const
		requires(std::is_signed_v<BaseType>)
	{
		Quaternion res{*this};
		res *= BaseType{-1};
		return res;
	}
	/**
	 * @brief Self multiplication with a scalar.
	 * @param iScalar the scalar to multiply.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion& operator*=(const BaseType iScalar) {
		for (size_t it = 0; it < 4; ++it) { m_data[it] *= iScalar; }
		return *this;
	}
	/**
	 * @brief Multiplication with a scalar.
	 * @param iScalar The scalar to multiply.
	 * @return Quaternion result.
	 */
	constexpr Quaternion operator*(const BaseType iScalar) const {
		Quaternion res{*this};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Multiplication with a scalar
	 * @param iScalar The scalar to multiply.
	 * @param iQuaternion Quaternion to multiply.
	 * @return Quaternion result.
	 */
	constexpr friend Quaternion operator*(const BaseType iScalar, const Quaternion& iQuaternion) {
		Quaternion res{iQuaternion};
		res *= iScalar;
		return res;
	}
	/**
	 * @brief Self division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return This actualized Quaternion.
	 */
	constexpr Quaternion& operator/=(const BaseType iScalar) {
		for (size_t it = 0; it < 4; ++it) { m_data[it] /= iScalar; }
		return *this;
	}
	/**
	 * @brief Division by a scalar.
	 * @param iScalar The scalar to divide.
	 * @return Quaternion result.
	 */
	constexpr Quaternion operator/(const BaseType iScalar) const {
		Quaternion res{*this};
		res /= iScalar;
		return res;
	}

	/**
	 * @brief Quaternion Product.
	 * @param iOther Other Quaternion to multiply.
	 * @return The product quaternion.
	 */
	constexpr Quaternion operator*(const Quaternion& iOther) const {
		Quaternion res{};
		res.w() = w() * iOther.w() - x() * iOther.x() - y() * iOther.y() - z() * iOther.z();
		res.x() = w() * iOther.x() + x() * iOther.w() + y() * iOther.z() - z() * iOther.y();
		res.y() = w() * iOther.y() - x() * iOther.z() + y() * iOther.w() + z() * iOther.x();
		res.z() = w() * iOther.z() + x() * iOther.y() - y() * iOther.x() + z() * iOther.w();
		return res;
	}
	/**
	 * @brief Quaternion Product.
	 * @param iOther Other Quaternion to multiply.
	 * @return The product quaternion.
	 */
	constexpr Quaternion& operator*=(const Quaternion& iOther) {
		*this = *this * iOther;
		return *this;
	}

	/**
	 * @brief Quaternion dot product.
	 * @param iOther Other quaternion to multiply.
	 * @return The dot product.
	 */
	constexpr BaseType dot(const Quaternion& iOther) const {
		return w() * iOther.w() + x() * iOther.x() + y() * iOther.y() + z() * iOther.z();
	}
	/**
	 * @brief Get a conjugated copy of this quaternion.
	 * @return Conjugated quaternion.
	 */
	constexpr Quaternion conjugated() const { return Quaternion{*this}.conjugate(); }
	/**
	 * @brief Conjugate this quaternion.
	 * @return Conjugated quaternion.
	 */
	constexpr Quaternion& conjugate() {
		x() = -x();
		y() = -y();
		z() = -z();
		return *this;
	}
	/**
	 * @brief Compute the square norm.
	 * @return The sqare norm.
	 */
	constexpr BaseType normSq() const { return w() * w() + x() * x() + y() * y() + z() * z(); }
	/**
	 * @brief Compute the norm.
	 * @return The norm.
	 */
	constexpr BaseType norm() const {
		if constexpr (std::is_floating_point_v<BaseType>) {
			return std::sqrt(normSq());
		}
		return {};
	}
	/**
	 * @brief Normalize this Quaternion if possible.
	 * @return This Quaternion.
	 */
	constexpr Quaternion& normalize() {
		if constexpr (std::is_floating_point_v<BaseType>) {
			const BaseType n = normSq();
			if (n < std::numeric_limits<BaseType>::epsilon())
				return *this;
			*this /= std::sqrt(n);
		}
		return *this;
	}

	/**
	 * @brief Create a new normalized Quaternion.
	 * @return Normalized Quaternion.
	 */
	constexpr Quaternion normalized() const { return Quaternion{*this}.normalize(); }
	/**
	 * @brief Inverse this quaternion.
	 * @return This quaternion.
	 */
	constexpr Quaternion& inverse() {
		const BaseType n = normSq();
		if (n < std::numeric_limits<BaseType>::epsilon())
			return *this;
		conjugate();
		*this /= normSq();
		return *this;
	}
	/**
	 * @brief Get an inverted copy of this quaternion.
	 * @return Inverted quaternion
	 */
	constexpr Quaternion inverted() const { return Quaternion{*this}.inverse(); }

private:
	std::array<BaseType, 4> m_data;
};

/**
 * @brief Use a Quaternion to transform a vector.
 * @tparam BaseType The internal data type
 * @param iQuaternion The Quaternion defining the transformation.
 * @param iVector The Vector to transform.
 * @return The transformed vector.
 */
template<typename BaseType>
constexpr Vector<BaseType, 3> rotate(const Quaternion<BaseType>& iQuaternion, const Vector<BaseType, 3>& iVector) {
	Vector<BaseType, 3> qv{iQuaternion.x(), iQuaternion.y(), iQuaternion.z()};
	Vector<BaseType, 3> uv(qv ^ iVector);
	Vector<BaseType, 3> uuv(qv ^ uv);
	return iVector + ((uv * iQuaternion.w()) + uuv) * BaseType{2};
}

using quat = Quaternion<float>;
using quatd = Quaternion<double>;

}// namespace owl::math
