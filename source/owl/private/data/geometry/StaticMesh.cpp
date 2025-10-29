/**
 * @file StaticMesh.cpp
 * @author Silmaen
 * @date 18/10/2025
 * Copyright © 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "core/execution_utils.h"
#include "data/geometry/StaticMesh.h"

namespace owl::data::geometry {

StaticMesh::StaticMesh() = default;
StaticMesh::~StaticMesh() = default;

StaticMesh::StaticMesh(const StaticMesh& iOther) { *this = iOther.clone(); }

StaticMesh::StaticMesh(StaticMesh&&) noexcept = default;

auto StaticMesh::operator=(const StaticMesh& iOther) -> StaticMesh& {
	*this = iOther.clone();
	return *this;
}

auto StaticMesh::operator=(StaticMesh&&) noexcept -> StaticMesh& = default;

[[nodiscard]] auto StaticMesh::getVertexCount() const -> size_t { return m_vertices.size(); }
[[nodiscard]] auto StaticMesh::getTriangleCount() const -> size_t { return m_triangles.size(); }
[[nodiscard]] auto StaticMesh::getVertices() const -> const std::vector<primitive::MeshVertex>& { return m_vertices; }
[[nodiscard]] auto StaticMesh::getVertices() -> std::vector<primitive::MeshVertex>& { return m_vertices; }
[[nodiscard]] auto StaticMesh::getTriangles() const -> const std::vector<primitive::Triangle>& { return m_triangles; }
[[nodiscard]] auto StaticMesh::getTriangles() -> std::vector<primitive::Triangle>& { return m_triangles; }

[[nodiscard]] auto StaticMesh::getVertexIterator(const size_t iIndex) -> VertexIterator {
	return m_vertices.begin() + static_cast<std::vector<primitive::MeshVertex>::difference_type>(iIndex);
}
[[nodiscard]] auto StaticMesh::getVertexIterator(const size_t iIndex) const -> ConstVertexIterator {
	return m_vertices.begin() + static_cast<std::vector<primitive::MeshVertex>::difference_type>(iIndex);
}
[[nodiscard]] auto StaticMesh::getTriangleIterator(const size_t iIndex) -> TriangleIterator {
	return m_triangles.begin() + static_cast<std::vector<primitive::Triangle>::difference_type>(iIndex);
}
[[nodiscard]] auto StaticMesh::getTriangleIterator(const size_t iIndex) const -> ConstTriangleIterator {
	return m_triangles.begin() + static_cast<std::vector<primitive::Triangle>::difference_type>(iIndex);
}

void StaticMesh::addTriangle(const std::array<uint32_t, 3>& iVertexIndices) {
	primitive::Triangle triangle;
	triangle.setIndex(static_cast<uint32_t>(m_triangles.size()));
	for (uint8_t i = 0; i < 3; ++i) { triangle.setVertex(i, &m_vertices.at(iVertexIndices[i])); }
	m_triangles.push_back(std::move(triangle));
	m_trianglesExtraDataContainer.resize(m_triangles.size());
}

void StaticMesh::clear() {
	m_vertices.clear();
	m_triangles.clear();
	m_verticesExtraDataContainer.clear();
	m_trianglesExtraDataContainer.clear();
}

[[nodiscard]] auto StaticMesh::clone() const -> StaticMesh {
	StaticMesh newMesh;
	newMesh.m_vertices = m_vertices;
	newMesh.m_triangles.clear();
	newMesh.m_triangles.reserve(m_triangles.size());
	for (const auto& tri: m_triangles) {
		auto vs = tri.getVertices();
		primitive::Triangle newTri;
		newTri.setIndex(tri.getIndex());
		for (uint8_t i = 0; i < 3; ++i) {
			newTri.setVertex(i, &newMesh.m_vertices.at(static_cast<size_t>(std::distance(
										m_vertices.data(), const_cast<const primitive::MeshVertex*>(vs[i])))));
		}
		newMesh.m_triangles.emplace_back(std::move(newTri));
	}
	newMesh.m_verticesExtraDataContainer = m_verticesExtraDataContainer.clone();
	newMesh.m_trianglesExtraDataContainer = m_trianglesExtraDataContainer.clone();
	return newMesh;
}

auto StaticMesh::addVertexExtraData(const core::FactoryPid iExtraDataId) -> bool {
	if (!isExtraDataDefinedOnAllVertices(iExtraDataId))
		m_verticesExtraDataContainer.addExtraData(iExtraDataId);
	return isExtraDataDefinedOnAllVertices(iExtraDataId);
}

auto StaticMesh::deleteVertexExtraData(const core::FactoryPid iExtraDataId) -> bool {
	return m_verticesExtraDataContainer.deleteExtraData(iExtraDataId);
}

auto StaticMesh::isExtraDataDefinedOnAllVertices(const core::FactoryPid iExtraDataId) const -> bool {
	return m_verticesExtraDataContainer.isExtraDataDefined(iExtraDataId);
}
auto StaticMesh::getVertexExtraData(const core::FactoryPid iExtraDataId) const -> const extradata::ExtraDataContainer* {
	return m_verticesExtraDataContainer.getExtraData(iExtraDataId);
}


auto StaticMesh::addTriangleExtraData(const core::FactoryPid iExtraDataId) -> bool {
	if (!isExtraDataDefinedOnAllTriangles(iExtraDataId))
		m_trianglesExtraDataContainer.addExtraData(iExtraDataId);
	return isExtraDataDefinedOnAllTriangles(iExtraDataId);
}


auto StaticMesh::deleteTriangleExtraData(const core::FactoryPid iExtraDataId) -> bool {
	return m_trianglesExtraDataContainer.deleteExtraData(iExtraDataId);
}

auto StaticMesh::isExtraDataDefinedOnAllTriangles(const core::FactoryPid iExtraDataId) const -> bool {
	return m_trianglesExtraDataContainer.isExtraDataDefined(iExtraDataId);
}

auto StaticMesh::getTriangleExtraData(const core::FactoryPid iExtraDataId) const
		-> const extradata::ExtraDataContainer* {
	return m_trianglesExtraDataContainer.getExtraData(iExtraDataId);
}

}// namespace owl::data::geometry
