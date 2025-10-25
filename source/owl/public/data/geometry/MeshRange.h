/**
 * @file MeshRange.h
 * @author Silmaen
 * @date 20/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/geometry/MeshRangeIterator.h"
#include "data/geometry/StaticMesh.h"

namespace owl::data::geometry {

constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

/**
 * @brief Class to store the start and end index of the mesh range.
 */
template<bool IsConst, MeshElementType ElementType, typename... Components>
class OWL_API MeshRange {
public:
	using MeshType = std::conditional_t<IsConst, const StaticMesh, StaticMesh>;
	using iterator = MeshRangeIterator<IsConst, ElementType, Components...>;
	using const_iterator = MeshRangeIterator<true, ElementType, Components...>;
	static_assert(
			!IsConst ||
					std::conjunction_v<std::negation<
							std::disjunction<details::IsWriteExtraData<std::decay_t<Components>, ElementType>,
											 details::IsWriteExtraDataPid<std::decay_t<Components>, ElementType>,
											 std::is_same<std::decay_t<Components>, component::EditCoordinate>>>...>,
			"You are trying to construct a MeshRange with WriteMeshExtraData or EditCoordinate but a const StaticMesh  "
			"which can't be mutated");

	/**
	 * @brief
	 *  Constructor. Build a basic range.
	 * @tparam Components2 Component type, same as Components
	 * @param[in] iMesh 		The mesh on which to iterate.
	 * @param[in] iComponents 	Cloud's components on which to iterate.
	 */
	template<typename... Components2>
	explicit MeshRange(MeshType& iMesh, Components2&&... iComponents)
		: m_mesh(&iMesh), m_components(std::forward<Components2>(iComponents)...) {}
	/**
	 * @brief
	 *  Constructor. Build a range between two indices.
	 * @tparam Components2 	Component type, same as Components.
	 * @param[in] iMesh 		The cloud on which to iterate.
	 * @param[in] iFirstIndex 	First index of the iterator.
	 * @param[in] iEndIndex 	End index of the iterator (last + 1).
	 * @param[in] iComponents 	Cloud's components on which to iterate.
	 */
	template<typename... Components2>
	MeshRange(MeshType& iMesh, const size_t iFirstIndex, const size_t iEndIndex, Components2&&... iComponents)
		: m_firstIndex{iFirstIndex}, m_endIndex{iEndIndex}, m_mesh(&iMesh),
		  m_components(std::forward<Components2>(iComponents)...) {
		OWL_CORE_ASSERT(m_endIndex <= getObjectSize(), "")
	}

	/**
	 * @brief
	 *  Return an iterator on the first element of the mesh.
	 * @return Iterator on the first element.
	 */
	auto begin() const -> const_iterator { return const_iterator(*m_mesh, m_firstIndex, m_components); }

	/**
	 * @brief
	 *  Return an iterator on the first element of the mesh.
	 * @return Iterator on the first element.
	 */
	auto begin() -> iterator { return iterator(*m_mesh, m_firstIndex, m_components); }

	/**
	 * @brief
	 *  Return a const iterator on the first element of the mesh.
	 * @return Const iterator on the first element.
	 */
	auto cbegin() const -> const_iterator { return const_iterator(*m_mesh, m_firstIndex, m_components); }

	/**
	 * @brief
	 *  Return an iterator on the element following the last element of the mesh.
	 * @return Iterator on the element following the last element.
	 */
	auto end() const -> const_iterator {
		OWL_CORE_ASSERT(m_endIndex != INVALID_INDEX, "")
		return const_iterator(*m_mesh, m_endIndex, m_components);
	}

	/**
	 * @brief
	 *  Return an iterator on the element following the last element of the mesh.
	 * @return Iterator on the element following the last element.
	 */
	auto end() -> iterator {
		OWL_CORE_ASSERT(m_endIndex != INVALID_INDEX, "")
		return iterator(const_cast<MeshType&>(*m_mesh), m_endIndex, m_components);
	}

	/**
	 * @brief
	 *  Return a const iterator on the element following the last element of the mesh.
	 * @return Const iterator on the element following the last element.
	 */
	auto cend() const -> const_iterator {
		OWL_CORE_ASSERT(m_endIndex != INVALID_INDEX, "")
		return const_iterator(*m_mesh, m_endIndex, m_components);
	}

	/**
	 * @brief
	 *  Get the mesh used to iterate.
	 * @return A mesh.
	 */
	auto getObject() const -> MeshType& { return *m_mesh; }

	/**
	 * @brief
	 * Get the Range's size.
	 * @return The range's size.
	 */
	[[nodiscard]] auto getSize() const -> size_t { return m_endIndex - m_firstIndex; }

private:
	/**
	 * @brief
	 * Get the Object's size.
	 * @return The object's size.
	 */
	[[nodiscard]]
	constexpr auto getObjectSize() const -> size_t {
		OWL_CORE_ASSERT(m_mesh, "No Mesh given")
		return ElementType == MeshElementType::Vertex ? m_mesh->getVertexCount() : m_mesh->getTriangleCount();
	}
	/// Mesh to iterate.
	MeshType* m_mesh;
	/// Cloud's components on which to iterate.
	const std::tuple<Components...> m_components;
	/// First index of the range.
	const size_t m_firstIndex = 0;
	/// End index of the range.
	const size_t m_endIndex = getObjectSize();
};

// Specializations for vertex and triangle ranges.

/**
 * @brief Class to store the start and end index of the vertex mesh range.
 */
template<bool IsConst, typename... Components>
class OWL_API MeshVertexRange : public MeshRange<IsConst, MeshElementType::Vertex, Components...> {
	using MeshRange<IsConst, MeshElementType::Vertex, Components...>::MeshRange;
};

/**
 * @brief Class to store the start and end index of the triangle mesh range.
 */
template<bool IsConst, typename... Components>
class OWL_API MeshTriangleRange : public MeshRange<IsConst, MeshElementType::Triangle, Components...> {
	using MeshRange<IsConst, MeshElementType::Triangle, Components...>::MeshRange;
};

// Helper to determine if a component requires write access.

template<typename... Components>
MeshVertexRange(StaticMesh&, Components&&...) -> MeshVertexRange<false, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshVertexRange(const StaticMesh&, Components&&...) -> MeshVertexRange<true, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshVertexRange(StaticMesh&, size_t, size_t, Components&&...)
		-> MeshVertexRange<false, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshVertexRange(const StaticMesh&, size_t, size_t, Components&&...)
		-> MeshVertexRange<true, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshTriangleRange(StaticMesh&, Components&&...) -> MeshTriangleRange<false, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshTriangleRange(const StaticMesh&, Components&&...) -> MeshTriangleRange<true, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshTriangleRange(StaticMesh&, size_t, size_t, Components&&...)
		-> MeshTriangleRange<false, std::remove_cvref_t<Components>...>;

template<typename... Components>
MeshTriangleRange(const StaticMesh&, size_t, size_t, Components&&...)
		-> MeshTriangleRange<true, std::remove_cvref_t<Components>...>;

}// namespace owl::data::geometry
