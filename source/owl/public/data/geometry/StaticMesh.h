/**
 * @file StaticMesh.h
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "data/extradata/ExtraDataTable.h"
#include "data/geometry/MeshCursorBase.h"
#include "data/geometry/MeshRangeIterator.h"
#include "data/geometry/primitive/MeshVertex.h"
#include "data/geometry/primitive/Triangle.h"

/**
 * @brief Namespace for geometry objects management.
 */
namespace owl::data::geometry {

/**
 * @brief Class representing a static mesh.
 */
class OWL_API StaticMesh {
public:
	using VertexIterator = std::vector<geometry::primitive::MeshVertex>::iterator;
	using ConstVertexIterator = std::vector<geometry::primitive::MeshVertex>::const_iterator;
	using TriangleIterator = MeshRangeIterator<false, MeshElementType::Triangle>;
	using ConstTriangleIterator = MeshRangeIterator<true, MeshElementType::Triangle>;
	/**
	 * @brief Default constructor.
	 */
	StaticMesh();
	/**
	 * @brief Default destructor.
	 */
	~StaticMesh();

	/**
	 * @brief Default copy constructor.
	 */
	StaticMesh(const StaticMesh&);

	/**
	 * @brief Default move constructor.
	 */
	StaticMesh(StaticMesh&&) noexcept;

	/**
	 * @brief Default copy affectation operator.
	 */
	auto operator=(const StaticMesh&) -> StaticMesh&;

	/**
	 * @brief Default move affectation operator.
	 */
	auto operator=(StaticMesh&&) noexcept -> StaticMesh&;

	/**
	 * Get the number of vertices.
	 * @return The number of vertices.
	 */
	[[nodiscard]] auto getVertexCount() const -> size_t;
	/**
	 * Get the number of triangles.
	 * @return The number of triangles.
	 */
	[[nodiscard]] auto getTriangleCount() const -> size_t;
	/**
	 * @brief Get the vertices.
	 * @return The vertices.
	 */
	[[nodiscard]] auto getVertices() const -> const std::vector<primitive::MeshVertex>&;
	/**
	 * @brief Get the vertices.
	 * @return The vertices.
	 */
	auto getVertices() -> std::vector<primitive::MeshVertex>&;
	/**
	 * @brief Get the vertex iterator at the given index.
	 * @param iIndex The index of the vertex.
	 * @return The vertex iterator.
	 */
	[[nodiscard]] auto getVertexIterator(size_t iIndex) -> VertexIterator;

	/**
	 * @brief Get the vertex iterator at the given index.
	 * @param iIndex The index of the vertex.
	 * @return The vertex iterator.
	 */
	[[nodiscard]] auto getVertexIterator(size_t iIndex) const -> ConstVertexIterator;

	/**
	 * @brief Get the triangles.
	 * @return The triangles.
	 */
	[[nodiscard]] auto getTriangles() const -> const std::vector<primitive::Triangle>&;
	/**
	 * @brief Get the triangles.
	 * @return The triangles.
	 */
	auto getTriangles() -> std::vector<primitive::Triangle>&;

	/**
	 * @brief Check if the mesh is empty.
	 * @return True if the mesh is empty, false otherwise.
	 */
	[[nodiscard]] auto isEmpty() const -> bool { return getTriangleCount() == 0 && getVertexCount() == 0; }

	/**
	 * @brief Add a vertex to the mesh.
	 * @param iVertex The vertex to add.
	 */
	void addVertex(const primitive::MeshVertex& iVertex) {
		m_vertices.push_back(iVertex);
		m_verticesExtraDataContainer.resize(m_vertices.size());
	}
	/**
	 * @brief Add a vertex to the mesh.
	 * @param iVertex The vertex to add.
	 */
	void addVertex(primitive::MeshVertex&& iVertex) {
		m_vertices.push_back(std::move(iVertex));
		m_verticesExtraDataContainer.resize(m_vertices.size());
	}
	/**
	 * @brief Add a vertex to the mesh.
	 * @param iPosition The position of the vertex to add.
	 */
	void addVertex(const math::vec3& iPosition) {
		m_vertices.emplace_back(iPosition);
		m_verticesExtraDataContainer.resize(m_vertices.size());
	}

	/**
	 * @brief Add a triangle to the mesh.
	 * @param iTriangle The triangle to add.
	 */
	void addTriangle(const primitive::Triangle& iTriangle) {
		m_triangles.push_back(iTriangle);
		m_trianglesExtraDataContainer.resize(m_triangles.size());
	}
	/**
	 * @brief Add a triangle to the mesh.
	 * @param iTriangle The triangle to add.
	 */
	void addTriangle(primitive::Triangle&& iTriangle) {
		m_triangles.push_back(std::move(iTriangle));
		m_trianglesExtraDataContainer.resize(m_triangles.size());
	}
	/**
	 * @brief Add a triangle to the mesh.
	 * @param iVertexIndices The indices of the triangle's vertices.
	 */
	void addTriangle(const std::array<uint32_t, 3>& iVertexIndices);

	/**
	 * @brief Clear the mesh.
	 */
	void clear();

	/**
	 * @brief Clone the mesh.
	 */
	[[nodiscard]] auto clone() const -> StaticMesh;

	/**
	 * @brief
	 *  Add an extra data to each vertex of the polyhedron.
	 * @note
	 *  The association between the indices of the vertices and the extra data is preserved.
	 * @tparam     ExtraDataType  The type of extra data to attach.
	 * @return	True All extra data have been attached to vertices, false otherwise.
	 */
	template<typename ExtraDataType>
	auto addVertexExtraData() -> bool {
		return addVertexExtraData(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Detach and delete extra data from each vertex.
	 * @tparam ExtraDataType The type of extra data to delete.
	 * @return True if there was no error during the execution, false if the extra data are not found.
	 */
	template<typename ExtraDataType>
	auto deleteVertexExtraData() -> bool {
		return deleteVertexExtraData(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Verify if an extra data is defined all over the vertices.
	 * @tparam  ExtraDataType The type of the extra data to check.
	 * @return True if the extra data is present on each element.
	 */
	template<typename ExtraDataType>
	[[nodiscard]] auto isExtraDataDefinedOnAllVertices() const -> bool {
		return isExtraDataDefinedOnAllVertices(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Add an extra data to each triangle of the polyhedron.
	 * @note
	 *  The association between the indices of the triangles and the extra data is preserved.
	 * @tparam     ExtraDataType  The type of extra data to attach.
	 * @return	True All extra data have been attached to triangles, false otherwise.
	 */
	template<typename ExtraDataType>
	auto addTriangleExtraData() -> bool {
		return addTriangleExtraData(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Detach and delete extra data from each triangle.
	 * @tparam ExtraDataType The type of extra data to delete.
	 * @return True if there was no error during the execution, false if the extra data are not found.
	 */
	template<typename ExtraDataType>
	auto deleteTriangleExtraData() -> bool {
		return deleteTriangleExtraData(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Verify if an extra data is defined all over the triangles.
	 * @tparam  ExtraDataType The type of the extra data to check.
	 * @return True if the extra data is present on each element.
	 */
	template<typename ExtraDataType>
	[[nodiscard]] auto isExtraDataDefinedOnAllTriangles() const -> bool {
		return isExtraDataDefinedOnAllTriangles(core::getFactoryPid<ExtraDataType>());
	}

	/**
	 * @brief
	 *  Create a range on particular extradata attached to triangles
	 *
	 * The range is a view on the extra data, but it has the ownership on the view.
	 * @note
	 *  This operation has an O(n) complexity.
	 * @note
	 *  The association between the indices of the triangles and the extra data is preserved.
	 *  If the extra data at a Triangle is undefined, the value of the corresponding extra data in the range is nullptr.
	 * @tparam ExtraDataType The type of extra data to retrieve.
	 * @return A range on extra data.
	 * @see MeshExtraDataRangeImpl
	 */
	template<typename ExtraDataType>
	auto createTriangleExtraDataRange() const -> std::vector<shared<ExtraDataType>> {
		auto cluster = getTriangleExtraData(core::getFactoryPid<ExtraDataType>());
		if (cluster == nullptr)
			return {};
		// Convert to proper type
		std::vector<shared<ExtraDataType>> result;
		result.reserve(m_triangles.size());
		for (size_t i = 0; i < m_triangles.size(); ++i) {
			result.emplace_back(cluster->template getExtraDataAs<ExtraDataType>(i));
		}
		return result;
	}

	/**
	 * @brief
	 *  Create a range on particular extradata attached to vertices
	 *
	 * The range is a view on the extra data, but it has the ownership on the view.
	 * @note
	 *  This operation has an O(n) complexity.
	 * @note
	 *  The association between the indices of the vertices and the extra data is preserved.
	 *  If the extra data at a vertex is undefined, the value of the corresponding extra data in the range is nullptr.
	 * @tparam ExtraDataType The type of extra data to retrieve.
	 * @return A range on extra data.
	 */
	template<typename ExtraDataType>
	auto createVertexExtraDataRange() const -> std::vector<shared<ExtraDataType>> {
		auto cluster = getVertexExtraData(core::getFactoryPid<ExtraDataType>());
		if (cluster == nullptr)
			return {};
		// Convert to proper type
		std::vector<shared<ExtraDataType>> result;
		result.reserve(m_vertices.size());
		for (size_t i = 0; i < m_vertices.size(); ++i) {
			result.emplace_back(cluster->template getExtraDataAs<ExtraDataType>(i));
		}
		return result;
	}

private:
	using ExtraDataCluster = std::vector<data::extradata::ExtraDataBase*>;

	/**
	 * @brief
	 *  Add an extra data to each vertex of the polyhedron.
	 * @param[in]  iExtraDataId        Product ID of extra data to attach.
	 * @return	True All extra data have been attached to vertices, false otherwise.
	 */
	auto addVertexExtraData(core::FactoryPid iExtraDataId) -> bool;

	/**
	 * @brief
	 *  Retrieve particular extra data of each polyhedron vertex.
	 * @param[in]  iExtraDataId Product ID of extra data to retrieve.
	 * @return Objects allocated from the factory and attached to the vertices or
	 *         nullopt if extra data are incomplete or not found.
	 */
	[[nodiscard]] auto getVertexExtraData(core::FactoryPid iExtraDataId) const
			-> const ::owl::data::extradata::ExtraDataContainer*;

	/**
	 * @brief
	 *  Detach and delete extra data from each vertex.
	 * @param[in] iExtraDataId Product ID of extra data to delete.
	 * @return True if there was no error during the execution, false if the extra data are not found.
	 */
	auto deleteVertexExtraData(core::FactoryPid iExtraDataId) -> bool;

	/**
	 * @brief
	 *  Add an extra data to each triangle of the polyhedron/
	 * @param[in]  iExtraDataId       Product ID of extra data to attach.
	 * @return	True All extra data have been attached to triangles, false otherwise.
	 */
	auto addTriangleExtraData(core::FactoryPid iExtraDataId) -> bool;

	/**
	 * @brief
	 *  Retrieve particular extra data of each polyhedron triangle.
	 * @param[in]  iExtraDataId Product ID of extra data to retrieve.
	 * @return Objects allocated from the factory and attached to the triangles or
	 *         nullopt if extra data are incomplete or not found.
	 */
	[[nodiscard]] auto getTriangleExtraData(core::FactoryPid iExtraDataId) const
			-> const ::owl::data::extradata::ExtraDataContainer*;

	/**
	 * @brief
	 *  Detach and delete extra data from each triangle.
	 * @param[in] iExtraDataId Product ID of extra data to delete.
	 * @return True if there was no error during the execution, false if the extra data are not found.
	 */
	auto deleteTriangleExtraData(core::FactoryPid iExtraDataId) -> bool;

	/**
	 * @brief
	 *  Verify if an extra data is defined all over the vertices.
	 * @param[in] iExtraDataId Product ID of the extra data to check.
	 * @return True if the extra data is present on each element.
	 */
	[[nodiscard]] auto isExtraDataDefinedOnAllVertices(core::FactoryPid iExtraDataId) const -> bool;

	/**
	 * @brief
	 *  Verify if an extra data is defined all over the triangles.
	 * @param[in] iExtraDataId Product ID of the extra data to check.
	 * @return True if the extra data is present on each element.
	 */
	[[nodiscard]] auto isExtraDataDefinedOnAllTriangles(core::FactoryPid iExtraDataId) const -> bool;

	template<bool IsConst, MeshElementType ElementType>
	friend class MeshCursorBase;
	// Base Data
	/// The list of vertices.
	std::vector<primitive::MeshVertex> m_vertices;
	/// The list of triangles.
	std::vector<primitive::Triangle> m_triangles;

	// Extra Data
	/// Extra data container for vertices.
	::owl::data::extradata::ExtraDataTable m_verticesExtraDataContainer;
	/// Extra data container for triangles.
	::owl::data::extradata::ExtraDataTable m_trianglesExtraDataContainer;
};

}// namespace owl::data::geometry
