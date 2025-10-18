/**
 * @file MeshCursorBase.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once
#include <data/extradata/ExtraDataContainer.h>

namespace owl::data::geometry {

class StaticMesh;

/**
 * @brief Type of static mesh element.
 */
enum class MeshElementType : uint8_t {
	Vertex,///< Vertex element
	Triangle///< Triangle element
};

/**
 * @brief
 *  Base class for mesh cursors.
 * @tparam IsConst     True if the cursor is const, false otherwise.
 * @tparam ElementType The type of element to iterate.
 */
template<bool IsConst, MeshElementType ElementType>
class OWL_API MeshCursorBase;

template<>
class OWL_API MeshCursorBase<true, MeshElementType::Vertex> {
public:
	using difference_type = std::ptrdiff_t;
	using MeshType = const StaticMesh;
	using ExtraDataIterator = ::owl::data::extradata::ExtraDataContainer::ConstExtraDataIterator;
	using ConstExtraDataIterator = ::owl::data::extradata::ExtraDataContainer::ConstExtraDataIterator;

	MeshCursorBase(const MeshType& iMesh, size_t iIndex);

	~MeshCursorBase() = default;
	/**
	 * @brief
	 *  Get the current index of the cursor.
	 * @return The current index.
	 */
	[[nodiscard]] auto getIndex() const -> size_t { return m_index; }

	/**
	 * @brief
	 *  Get the mesh on which the cursor is iterating.
	 * @return The mesh.
	 */
	[[nodiscard]] auto getMesh() const -> MeshType* { return m_mesh; }

	/**
	 * @brief
	 *  Get the extra data iterator at the given index.
	 * @param[in] iPid 	The factory PID of the extra data.
	 * @return The extra data iterator.
	 */
	[[nodiscard]] auto getExtraDataContainer(const core::FactoryPid& iPid) const
			-> const extradata::ExtraDataContainer*;
	/**
	 * @brief
	 *  Check if the cursor is equal to another.
	 * @param[in] iOther A cursor on the same cloud.
	 * @retval True if the cursors are equal
	 * @retval False otherwise.
	 */
	auto operator==(const MeshCursorBase& iOther) const -> bool {
		OWL_CORE_ASSERT(m_mesh == iOther.m_mesh, "MeshCursorBase: operator==")
		return m_index == iOther.m_index;
	}

	/**
	 * @brief
	 *  Check if the cursor is different from another.
	 * @param[in] iOther A cursor on the same cloud.
	 * @retval True if the cursors are different.
	 * @retval False otherwise.
	 */
	auto operator!=(const MeshCursorBase& iOther) const -> bool { return !(*this == iOther); }

	/**
	 * @brief
	 *   Less than operator.
	 * @param[in] iOther A cursor on the same cloud.
	 * @retval True if the current cursor is lower than the other iterator.
	 * @retval False otherwise.
	 */
	auto operator<(const MeshCursorBase& iOther) const -> bool {
		OWL_CORE_ASSERT(m_mesh == iOther.m_mesh, "MeshCursorBase: operator<")
		return m_index < iOther.m_index;
	}

	/**
	 * @brief
	 *  Move to the next element.
	 * @param[in] iIncrement Increment.
	 */
	void moveNext(const size_t iIncrement = 1) {
		if (m_index + iIncrement >= m_meshSize) {
			gotoEnd();
			return;
		}
		m_index += iIncrement;
	}

protected:
	MeshCursorBase(const MeshCursorBase& iOther) = default;

	/**
	 * @brief
	 *  Move constructor.
	 */
	MeshCursorBase(MeshCursorBase&&) noexcept = default;

	/**
	 * @brief
	 *  Assignment operator.
	 * @return A reference on the current cursor.
	 */
	auto operator=(const MeshCursorBase&) -> MeshCursorBase& = default;

	/**
	 * @brief
	 *  Move operator.
	 * @return A reference on the current cursor.
	 */
	auto operator=(MeshCursorBase&&) noexcept -> MeshCursorBase& = default;

	/**
	 * @brief
	 *  Go to the end of the cursor, the iterators become invalid.
	 */
	void gotoEnd() { m_index = m_meshSize; }


	/**
	 * @brief
	 *  Reset the cursor to an index.
	 * @param[in] iIndex Starting index of the cursor.
	 */
	void reset(const size_t iIndex) {
		if (iIndex >= m_meshSize) {
			gotoEnd();
			return;
		}
		m_index = iIndex;
	}

	/**
	 * @brief
	 *  Subtraction operator.
	 * @param[in] iOther A cursor on the same cloud.
	 * @return The distance between the two cursors.
	 */
	auto operator-(const MeshCursorBase& iOther) const -> difference_type {
		OWL_CORE_ASSERT(m_mesh == iOther.m_mesh, "MeshCursorBase: operator-")
		return static_cast<difference_type>(m_index) - static_cast<difference_type>(iOther.m_index);
	}

	/**
	 * @brief
	 *  Addition assignment operator.
	 * @param[in] iIncrement Increment.
	 * @return A reference to the current cursor.
	 */
	auto operator+=(const size_t iIncrement) -> MeshCursorBase& {
		moveNext(iIncrement);
		return *this;
	}

	/// The mesh on which the cursor is iterating.
	const StaticMesh* m_mesh;
	/// The current index of the cursor.
	size_t m_index{0};
	/// The size of the mesh element table.
	size_t m_meshSize{0};
};

/**
 * @brief
 *  Specialization of MeshCursorBase for non-const vertex elements.
 */
template<>
class OWL_API MeshCursorBase<false, MeshElementType::Vertex> : public MeshCursorBase<true, MeshElementType::Vertex> {
public:
	using MeshType = StaticMesh;
	using BaseCursor = MeshCursorBase<true, MeshElementType::Vertex>;
	MeshCursorBase(const MeshType& iMesh, const size_t iIndex) : BaseCursor(iMesh, iIndex) {}

	/**
	 * @brief
	 *  Get the extra data iterator at the given index.
	 * @param[in] iPid 	The factory PID of the extra data.
	 * @return The extra data iterator.
	 */
	[[nodiscard]] auto getExtraDataContainer(const core::FactoryPid& iPid) const
			-> const extradata::ExtraDataContainer*;
};

/**
 * @brief
 *  Specialization of MeshCursorBase for const triangle elements.
 */
template<>
class OWL_API MeshCursorBase<true, MeshElementType::Triangle> : public MeshCursorBase<true, MeshElementType::Vertex> {
public:
	using difference_type = std::ptrdiff_t;
	using MeshType = const StaticMesh;
	using BaseCursor = MeshCursorBase<true, MeshElementType::Vertex>;

	MeshCursorBase(const MeshType& iMesh, size_t iIndex);
	/**
	 * @brief
	 *  Get the extra data iterator at the given index.
	 * @param[in] iPid 	The factory PID of the extra data.
	 * @return The extra data iterator.
	 */
	[[nodiscard]] auto getExtraDataContainer(const core::FactoryPid& iPid) const
			-> const extradata::ExtraDataContainer*;
};

/**
 * @brief
 *  Specialization of MeshCursorBase for non-const triangle elements.
 */
template<>
class OWL_API MeshCursorBase<false, MeshElementType::Triangle>
	: public MeshCursorBase<true, MeshElementType::Triangle> {
public:
	using MeshType = StaticMesh;
	using BaseCursor = MeshCursorBase<true, MeshElementType::Triangle>;
	MeshCursorBase(const MeshType& iMesh, const size_t iIndex) : BaseCursor(iMesh, iIndex) {}
	/**
	 * @brief
	 *  Get the extra data iterator at the given index.
	 * @param[in] iPid 	The factory PID of the extra data.
	 * @return The extra data iterator.
	 */
	[[nodiscard]] auto getExtraDataContainer(const core::FactoryPid& iPid) const
			-> const extradata::ExtraDataContainer*;
};

}// namespace owl::data::geometry
