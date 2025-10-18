/**
 * @file MeshRangeIterator.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/geometry/MeshCursor.h"

namespace owl::data::geometry {

class StaticMesh;

/**
 * @brief
 *  Check if the given components is only the coordinate component.
 * @tparam Components The components to check.
 */
template<typename... Components>
constexpr static bool IsOnlyCoordinateComponent =
		sizeof...(Components) == 1 &&
		std::is_same_v<std::tuple_element_t<0, std::tuple<Components...>>, component::Coordinate>;

/**
 * @brief
 *  Iterator for mesh ranges.
 * @tparam IsConst     True if the iterator is const, false otherwise.
 * @tparam ElementType The type of element to iterate.
 * @tparam Components  The components to iterate.
 */
template<bool IsConst, MeshElementType ElementType, typename... Components>
class OWL_API MeshRangeIterator {
	using MeshType = std::conditional_t<IsConst, const StaticMesh, StaticMesh>;
	using CursorType = MeshCursor<IsConst, ElementType, Components...>;

public:
	using iterator_category = std::random_access_iterator_tag;
	using difference_type = std::ptrdiff_t;

	using value_type = std::conditional_t<
			sizeof...(Components) == 1,
			std::tuple_element_t<0, std::tuple<typename std::remove_cvref_t<Components>::ComponentType...>>,
			CursorType>;

	using reference = std::conditional_t<IsOnlyCoordinateComponent<Components...>, math::vec3,
										 std::conditional_t<IsConst, const value_type, value_type>&>;
	/**
	 * @brief
	 *  Empty constructor.
	 * @param[in] iMesh 		The mesh on which to iterate.
	 * @param[in] iPointIndex 	Starting index of the iterator.
	 */
	MeshRangeIterator(MeshType& iMesh, size_t iPointIndex) : m_cursor(iMesh, iPointIndex) {}

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iMesh 		The mesh on which to iterate.
	 * @param[in] iPointIndex 	Starting index of the iterator.
	 * @param[in] iComponents 	Mesh's components on which to iterate.
	 */
	MeshRangeIterator(MeshType& iMesh, size_t iPointIndex, const std::tuple<Components...>& iComponents)
		: m_components(iComponents), m_cursor(iMesh, iPointIndex, iComponents) {}

	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iMesh 		The mesh on which to iterate.
	 * @param[in] iPointIndex 	Starting index of the iterator.
	 * @param[in] iComponents 	Mesh's components on which to iterate.
	 */
	template<typename... Components2>
	MeshRangeIterator(MeshType& iMesh, size_t iPointIndex, Components2&&... iComponents)
		: m_components(std::make_optional<std::tuple<Components...>>(std::forward<Components2>(iComponents)...)),
		  m_cursor(iMesh, iPointIndex, *m_components) {}

	/**
	 * @brief Default destructor.
	 */
	~MeshRangeIterator() = default;

	/**
	 * @brief
	 *  Copy constructor.
	 */
	MeshRangeIterator(const MeshRangeIterator&) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	MeshRangeIterator(MeshRangeIterator&&) noexcept = default;

	/**
	 * @brief
	 *  Assignment operator.
	 * @return The iterator copied.
	 */
	auto operator=(const MeshRangeIterator&) -> MeshRangeIterator& = default;

	/**
	 * @brief
	 *  Move operator.
	 * @return The iterator moved.
	 */
	auto operator=(MeshRangeIterator&&) noexcept -> MeshRangeIterator& = default;

	/**
	 * @brief
	 *  Pre-increment operator (++iterator).
	 * @return Current iterator.
	 */
	auto operator++() -> MeshRangeIterator& {
		m_cursor.moveNext();
		std::apply([](auto&... iComp) -> auto { (iComp.moveNext(), ...); }, m_cursor.m_components.value());
		return *this;
	}

	/**
	 * @brief
	 *  Dereferencing operator.
	 * @return The cursor of the iterator.
	 */
	auto operator*() -> reference {
		static_assert(sizeof...(Components) > 0);
		if constexpr (IsOnlyCoordinateComponent<Components...>)
			return m_cursor.template get<0>().value();
		else if constexpr (sizeof...(Components) == 1)
			return m_cursor.template get<0>();
		else
			return m_cursor;
	}

	/**
	 * @brief
	 *  Check if the iterator is equal to another.
	 * @param[in] iOther A mesh iterator on the same mesh.
	 * @retval True if the iterators are equal
	 * @retval False otherwise.
	 */
	auto operator==(const MeshRangeIterator& iOther) const -> bool { return m_cursor == iOther.m_cursor; }

	/**
	 * @brief
	 *  Check if the iterator is different from another.
	 * @param[in] iOther A mesh iterator on the same mesh.
	 * @retval True if the iterators are different.
	 * @retval False otherwise.
	 */
	auto operator!=(const MeshRangeIterator& iOther) const -> bool { return m_cursor != iOther.m_cursor; }

	/**
	 * @brief
	 *   Less than operator.
	 * @param[in] iOther A mesh iterator on the same mesh.
	 * @retval True if the current iterator is lower than the other iterator,
	 * @retval False otherwise.
	 */
	auto operator<(const MeshRangeIterator& iOther) const -> bool { return m_cursor < iOther.m_cursor; }

	/**
	 * @brief
	 *  Subtraction operator.
	 * @param[in] iOther A mesh iterator on the same mesh.
	 * @return The distance between the two iterators.
	 */
	auto operator-(const MeshRangeIterator& iOther) const -> difference_type { return m_cursor - iOther.m_cursor; }

	/**
	 * @brief
	 *  Addition assignment operator.
	 * @param[in] iIncrement Increment.
	 * @return A reference to the current iterator.
	 */
	auto operator+=(size_t iIncrement) -> MeshRangeIterator& {
		m_cursor += iIncrement;
		return *this;
	}

	/**
	 * @brief
	 *  Addition operator.
	 * @param[in] iIncrement Increment.
	 * @return A copy of the current iterator.
	 */
	auto operator+(size_t iIncrement) const -> MeshRangeIterator {
		auto it(*this);
		it += iIncrement;
		return it;
	}

private:
	/// Mesh's components on which to iterate.
	std::optional<std::tuple<Components...>> m_components{};
	/// Cursor on the mesh.
	CursorType m_cursor;
};

}// namespace owl::data::geometry
