/**
 * @file box.h
 * @author Silmaen
 * @date 10/21/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"

namespace owl::math {
/**
 * @brief
 *  Very simple box definition.
 * @tparam BaseType Base type of coordinates.
 * @tparam Dim The space dimensions.
 */
template<typename BaseType, std::size_t Dim>
class Box {
public:
	constexpr Box() noexcept = default;

	constexpr Box(const Box& iOther) noexcept = default;

	constexpr Box(Box&& ioOther) noexcept = default;

	constexpr auto operator=(const Box& iOther) noexcept -> Box& = default;

	constexpr auto operator=(Box&& ioOther) noexcept -> Box& = default;

	constexpr ~Box() = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iMin Minimum bound (inclusive).
	 * @param[in] iMax Maximum bound (inclusive).
	 */
	constexpr Box(const Vector<BaseType, Dim>& iMin, const Vector<BaseType, Dim>& iMax) noexcept
		: m_min{iMin}, m_max{iMax} {
		reorder();
	}

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iMin Minimum bound (inclusive).
	 * @param[in] iMax Maximum bound (inclusive).
	 */
	constexpr Box(Vector<BaseType, Dim>&& iMin, Vector<BaseType, Dim>&& iMax) noexcept
		: m_min{std::move(iMin)}, m_max{std::move(iMax)} {
		reorder();
	}

	// Accessor
	/**
	 * @brief
	 *  Const access to the minimum corner.
	 * @return The minimum corner.
	 */
	[[nodiscard]] constexpr auto min() const noexcept -> const Vector<BaseType, Dim>& { return m_min; }

	/**
	 * @brief
	 *  Const access to the maximum corner.
	 * @return The maximum corner.
	 */
	[[nodiscard]] constexpr auto max() const noexcept -> const Vector<BaseType, Dim>& { return m_max; }

	/**
	 * @brief
	 *  Mutable access to the minimum corner.
	 * @return The minimum corner.
	 */
	constexpr auto min() noexcept -> Vector<BaseType, Dim>& { return m_min; }

	/**
	 * @brief
	 *  Mutable access to the maximum corner.
	 * @return The maximum corner.
	 */
	constexpr auto max() noexcept -> Vector<BaseType, Dim>& { return m_max; }

	/**
	 * @brief
	 *  Check if the box is empty.
	 * @return True if the box is empty.
	 */
	[[nodiscard]] constexpr auto isEmpty() const noexcept -> bool {
		return m_max.normSq() < 0.00001f && m_min.normSq() < 0.00001f;
	}

	// modifications
	/**
	 * @brief
	 *  Update state.
	 * @param[in] iInPoint Input point.
	 */
	constexpr void update(const Vector<BaseType, Dim>& iInPoint) noexcept {
		if (isEmpty()) {
			m_min = iInPoint;
			m_max = iInPoint;
			return;
		}
		for (std::size_t i = 0; i < Dim; i++) {
			m_min[i] = std::min(iInPoint[i], m_min[i]);
			m_max[i] = std::max(iInPoint[i], m_max[i]);
		}
	}

	/**
	 * @brief
	 *  Update state.
	 * @param[in] iInBox Input bounding box.
	 */
	constexpr void update(const Box& iInBox) noexcept {
		if (isEmpty()) {
			m_min = iInBox.m_min;
			m_max = iInBox.m_max;
			return;
		}
		for (std::size_t i = 0; i < Dim; i++) {
			m_min[i] = std::min(iInBox.m_min[i], m_min[i]);
			m_max[i] = std::max(iInBox.m_max[i], m_max[i]);
		}
	}

	/**
	 * @brief
	 *  Scale.
	 * @param[in] iScale Scale factor.
	 */
	constexpr void scale(const Vector<BaseType, Dim>& iScale) noexcept {
		if (isEmpty()) {
			return;
		}
		for (std::size_t i = 0; i < Dim; i++) {
			m_min[i] *= iScale[i];
			m_max[i] *= iScale[i];
		}
	}

	/**
	 * @brief
	 *  Translate.
	 * @param[in] iDelta Delta value.
	 */
	constexpr void translate(const Vector<BaseType, Dim>& iDelta) noexcept {
		for (std::size_t i = 0; i < Dim; i++) {
			m_min[i] += iDelta[i];
			m_max[i] += iDelta[i];
		}
	}

	/**
	 * @brief
	 *  The vector spanning from min to max corner.
	 * @return The diagonal vector.
	 */
	[[nodiscard]] auto diagonal() const noexcept -> Vector<BaseType, Dim> { return m_max - m_min; }

	// intersection
	/**
	 * @brief
	 *  Whether the box contains a point.
	 * @param[in] iPoint The point in space.
	 * @return True when the point lies inside the box.
	 */
	[[nodiscard]] constexpr auto contains(const Vector<BaseType, Dim>& iPoint) const noexcept -> bool {
		for (std::size_t i = 0; i < Dim; i++) {
			if (m_min[i] > iPoint[i] || m_max[i] < iPoint[i])
				return false;
		}
		return true;
	}

	/**
	 * @brief
	 *  Whether two boxes overlap.
	 * @param[in] iOther The other box to test against.
	 * @return True when the boxes intersect.
	 */
	[[nodiscard]] constexpr auto intersect(const Box& iOther) const noexcept -> bool {
		for (std::size_t i = 0; i < Dim; i++) {
			if (m_min[i] > iOther.m_max[i] || m_max[i] < iOther.m_min[i])
				return false;
		}
		return true;
	}

private:
	/**
	 * @brief
	 *  Reorder.
	 */
	void reorder() {
		for (std::size_t i = 0; i < Dim; i++) {
			if (m_min[i] > m_max[i]) {
				std::swap(m_min[i], m_max[i]);
			}
		}
	}
	/// Min point.
	Vector<BaseType, Dim> m_min;
	/// Max point.
	Vector<BaseType, Dim> m_max;
};


// Specialization.
using box2f = Box<float, 2>;
using box3f = Box<float, 3>;
using box4f = Box<float, 4>;
using box2d = Box<double, 2>;
using box3d = Box<double, 3>;
using box4d = Box<double, 4>;
using box2i = Box<int32_t, 2>;
using box3i = Box<int32_t, 3>;
using box4i = Box<int32_t, 4>;
using box2ui = Box<uint32_t, 2>;
using box3ui = Box<uint32_t, 3>;
using box4ui = Box<uint32_t, 4>;
using box2ui8 = Box<uint8_t, 2>;
using box3ui8 = Box<uint8_t, 3>;
using box4ui8 = Box<uint8_t, 4>;

}// namespace owl::math
