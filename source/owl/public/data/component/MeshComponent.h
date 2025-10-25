/**
 * @file MeshComponent.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/geometry/MeshCursorBase.h"
#include "data/geometry/primitive/MeshVertex.h"

/**
 * @brief Namespace for data components used in ranges.
 */
namespace owl::data::component {

class EditMeshVertexCoordinate;
template<bool IsConst = true>
class MeshVertexCoordinate;

/**
 * @brief
 *  Read structure for point's coordinates.
 */
struct Coordinate {
	using DataType = math::vec3;
	using ComponentType = MeshVertexCoordinate<>;
};

/**
 * @brief
 *  Read/Write structure for point's coordinates.
 */
struct EditCoordinate {
	using DataType = math::vec3;
	using ComponentType = EditMeshVertexCoordinate;
};

/// Component on point to access to its coordinates read only.
constexpr inline Coordinate Coordinates;
/// Component on point to access to its coordinates.
constexpr inline EditCoordinate EditCoordinates;

/**
 * @brief
 *  Base component used by the cloud range.
 * @tparam IsConst True if the cursor must be const, false otherwise.
 * @tparam ElementType The type of element
 */
template<bool IsConst, geometry::MeshElementType ElementType>
class OWL_API MeshComponentBase {
public:
	using CursorType = geometry::MeshCursorBase<IsConst, ElementType>;

	/**
	 * @brief
	 *  Set a new cursor. Used by copy constructor and operator of CloudCursor.
	 * @param iCursor Cursor used to iterate.
	 */
	void setCursor(CursorType& iCursor) { m_cursor = &iCursor; }

protected:
	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iCursor Cursor used to iterate.
	 */
	explicit MeshComponentBase(CursorType& iCursor) : m_cursor(&iCursor) {}

	/// Cursor used to iterate.
	CursorType* m_cursor = nullptr;
};

/**
 * @brief
 *  Read iterate component for point's coordinates.
 * @tparam IsConst False if we need to edit coordinate. True otherwise.
 * @see Coordinate, CloudRange
 */
template<bool IsConst>
class OWL_API MeshVertexCoordinate : public MeshComponentBase<IsConst, geometry::MeshElementType::Vertex> {
public:
	using CursorType = MeshComponentBase<IsConst, geometry::MeshElementType::Vertex>::CursorType;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 * @param[in] iReset True if the component's iterator must be reset.
	 */
	MeshVertexCoordinate(Coordinate, CursorType& iCursor, size_t iIndex = 0, bool iReset = true);

	/**
	 * @brief
	 *  Copy constructor.
	 */
	MeshVertexCoordinate(const MeshVertexCoordinate&) = default;

	/**
	 * @brief
	 *  Assignment operator.
	 * @return Current MeshVertexCoordinate.
	 */
	auto operator=(const MeshVertexCoordinate&) -> MeshVertexCoordinate& = default;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	MeshVertexCoordinate(MeshVertexCoordinate&&) noexcept = delete;

	/**
	 * @brief
	 *  Deleted move operator.
	 */
	auto operator=(MeshVertexCoordinate&&) noexcept -> MeshVertexCoordinate& = delete;
	/**
	 * @brief Default destructor.
	 */
	~MeshVertexCoordinate() = default;

	/**
	 * @brief
	 *  Reset the component's iterator.
	 * @param[in] iStart Starting index.
	 */
	void reset(size_t iStart = 0);

	/**
	 * @brief
	 *  Move to the next element.
	 * @param[in] iIncrement Increment value.
	 */
	void moveNext(const size_t iIncrement = 1) {
		if (iIncrement == 1)
			++m_pointIte;
		else
			m_pointIte += static_cast<typename PointIterator::difference_type>(iIncrement);
	}

	/**
	 * @brief
	 *  Get the current point's coordinates.
	 * @return Point's coordinates.
	 */
	[[nodiscard]] auto value() const -> math::vec3 { return m_pointIte->getPosition(); }

protected:
	// Utiliser le bon type d'itérateur selon IsConst
	using PointIterator = std::conditional_t<IsConst, std::vector<geometry::primitive::MeshVertex>::const_iterator,
											 std::vector<geometry::primitive::MeshVertex>::iterator>;
	/// Iterator on point.
	PointIterator m_pointIte;
};

/**
 * @brief
 *  Read/Write iterate component for point's coordinates.
 * @note
 *  This component unshares the coordinate.
 *  So this component can be thread safe if the range is created before the multithreaded call.
 * @see EditCoordinate, CloudRange
 */
class OWL_API EditMeshVertexCoordinate : public MeshVertexCoordinate<false> {
	using CursorType = geometry::MeshCursorBase<false, geometry::MeshElementType::Vertex>;

public:
	using MeshVertexCoordinate<false>::MeshVertexCoordinate;
	/**
	 * @brief
	 *  Default constructor.
	 * @param[in] iCursor Cursor used to iterate.
	 * @param[in] iIndex Starting index.
	 */
	EditMeshVertexCoordinate(EditCoordinate, CursorType& iCursor, size_t iIndex = 0);

	/**
	 * @brief
	 *  Set the coordinates of the current point.
	 * @param[in] iPt New coordinates.
	 */
	void setValue(const math::vec3& iPt);
};

}// namespace owl::data::component
